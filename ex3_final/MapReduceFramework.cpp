#include "MapReduceFramework.h"
#include <pthread.h>
#include <cstdio>
#include <iostream>
#include <atomic>
#include <vector>
#include <map>
#include <semaphore.h>
#include "Barrier/Barrier.h"
#include <algorithm>


using std::atomic;
using std::vector;
using std::cerr;
using std::endl;

struct ThreadContext;
struct JobContext;


const int64_t FIRST_31_BITS = (uint64_t) 1 << 31;


typedef struct ThreadContext {
    JobContext *jobContext;
    InputVec inputVec;
    IntermediateVec *intermediateVec;
//    std::map<int, vector<IntermediatePair>*> *intermediateVecMap;
    sem_t *semaphore;
    atomic<int> *mappedPairsCounter;

    const MapReduceClient *client;
    Barrier *barrier;
    pthread_mutex_t *mutex;
    int multiThreadLevel;
    int threadNum;


    ThreadContext(JobContext *_jobContexts,
                  const InputVec &_inputVec,
                  sem_t *_semaphore,
                  atomic<int> *_threadCounter,
                  const MapReduceClient *_client,
                  Barrier *_barrier,
                  pthread_mutex_t *_mutex,
                  int _multiThreadLevel,
                  int _threadNum) :

            jobContext(_jobContexts),
            inputVec(_inputVec),
            semaphore(_semaphore),
            mappedPairsCounter(_threadCounter),
            client(_client),
            barrier(_barrier),
            mutex(_mutex),
            multiThreadLevel(_multiThreadLevel),
            threadNum(_threadNum)
    {
//        intermediateVecMap = new std::map<int, IntermediateVec*>();
        intermediateVec = new IntermediateVec();
    }

    ~ThreadContext()
    {
        if (intermediateVec != nullptr)
        {
            delete intermediateVec;
        }
    }
} ThreadContext;

typedef struct JobContext {
    vector<ThreadContext*> *threads{};
    pthread_t *threadsArray;
    JobState *state;
    atomic<int> totalNumberOfMappedPairs;
    atomic<int> totalNumberOfInterPairs;
    atomic<int> totalNumberOfSortedPairs;
    atomic<int> totalNumberOfShuffledPairs;
    atomic<int> placeInReducedVector;
    atomic<int> totalNumberOfReducedPairs;
    atomic<float> atomicPercentage;
    atomic<int> atomicStage;
    atomic<uint64_t> atomicStageCounter;
    vector<IntermediateVec*> *reduceVec;
    OutputVec &outputVec;

    JobContext(vector<ThreadContext* > *_threads, pthread_t *_arr, OutputVec &outVec) :
    threads(_threads), threadsArray(_arr),  outputVec(outVec)
    {
        state = new JobState();
        state->percentage = 0;
        state->stage = UNDEFINED_STAGE;
        totalNumberOfMappedPairs = 0;
        totalNumberOfInterPairs = 0;
        totalNumberOfSortedPairs = 0;
        totalNumberOfShuffledPairs = 0;
        placeInReducedVector = 0;
        totalNumberOfReducedPairs = 0;
        atomicPercentage = 0;
        atomicStage = UNDEFINED_STAGE;
        reduceVec = new vector<IntermediateVec*>();
        atomicStageCounter = 0;
    }

    ~JobContext()
    {
        if(not threads->empty())
        {
            delete threads->back()->mappedPairsCounter;
            delete threads->back()->semaphore;
            delete threads->back()->mutex;
            delete threads->back()->barrier;
//            delete threads->back()->intermediateVecMap;
        }

        if (threads != nullptr && not threads->empty())
        {
            for (ThreadContext *threadContext : *threads)
            {
                delete threadContext;
            }
        }

        if (reduceVec != nullptr && not reduceVec->empty()) // let only thread 0 delete reduceVec
        {
            for (IntermediateVec *interVec : *reduceVec)
            {
                delete interVec;
            }
            delete reduceVec;
        }
        delete threads;
        delete[] threadsArray;
        delete state;
    }
} JobContext;

void updateAtomicPercent(ThreadContext *context, stage_t stage, const int sizeOfStep, const int numOfDone)
{
    // 2 MSB: stage, 31 next MSB: total pairs to do, 31 LSB: number of those done
    unsigned long long counter = ((uint64_t) stage <<  62) + ((uint64_t) sizeOfStep << 31) + ((uint64_t) numOfDone);

    context->jobContext->atomicStageCounter = counter;
}

bool pairsComparator(const IntermediatePair p1, const IntermediatePair p2)
{
    return *p1.first < *p2.first;
}

/**
 * Products a (K2*, V2*) pair.
 * Recieves as input intermediary element (K2, V2) and context which contains data structure of the thread that created
 * the intermediary element.
 * @param key
 * @param value
 * @param context
 */
void emit2 (K2 *key, V2 *value, void *context)
{
    ThreadContext *tempContext = (ThreadContext *) context;
    IntermediatePair pair = IntermediatePair(key, value);
    tempContext->intermediateVec->push_back(pair);
    (tempContext->jobContext->totalNumberOfInterPairs)++;
}

void emit3(K3 *key, V3 *value, void *context)
{
    ThreadContext *curr_context = (ThreadContext *) context;

    OutputPair pair = OutputPair(key, value);
    curr_context->jobContext->outputVec.push_back(pair);
}

/**
 * (Not from the API) The map phase function is applied to each input element,
 * producing a sequence of intermediary elements.
 * @param context
 */
 void mapPhase(ThreadContext *context)
{
    int oldVal;
    if (pthread_mutex_lock(context->mutex))
    {
        cerr << "system error: mutex lock error"  << endl;
        exit(1);
    }


    while ((unsigned long) context->jobContext->totalNumberOfMappedPairs.load() < context->inputVec.size())
    {
        oldVal = context->jobContext->totalNumberOfMappedPairs++;
        InputPair currPair = context->inputVec.at(static_cast<int>(oldVal));


        // Call emit2 through map, with the current interVector
        context->client->map(currPair.first, currPair.second, context);
        updateAtomicPercent(context, MAP_STAGE, context->inputVec.size(), context->jobContext->totalNumberOfMappedPairs);

    }

    if (pthread_mutex_unlock(context->mutex))
    {
        cerr << "system error: mutex lock error"  << endl;
        exit(1);
    }
}

/// sort phase: sort the intermediateVector of each thread
void sortPhase(ThreadContext *context)
{

    if (not context->intermediateVec->empty())
    {
        std::sort(context->intermediateVec->begin(), context->intermediateVec->end(), pairsComparator);
    }
}

/** Returns a pointer to the key with max value among all threads. **/
K2 *getMaximumKey(vector<ThreadContext*> *threads)
{

    K2 *k2Max = nullptr; // if threads->empty() or every intermediateVec in threads is empty, return nullptr

    if (threads->empty())
    {
        return nullptr;
    }

    for (ThreadContext *threadContext : *threads)
    {
        if (not threadContext->intermediateVec->empty())
        {
            K2 *currKey = threadContext->intermediateVec->back().first;
            if (k2Max == nullptr || *k2Max < *currKey)
            {
                k2Max = threadContext->intermediateVec->back().first;
            }
        }
    }

    return k2Max;
}

/** Pops all pairs with the given maximum k2Max key into a new vector.
 * If there is no such, or if there are no pairs left in any vector, returns nullptr.
 * **/
IntermediateVec *popBackOfAllMaximum(vector<ThreadContext*> *threads, K2 *k2Max)
{
    if (k2Max == nullptr)
    {
        return nullptr;
    }

    bool areAllEmpty = true;
    auto *vectorOfCurrentKey = new IntermediateVec(); // REMEMBER: use erase to delete the vector of vectors

    for (ThreadContext *currContext : *threads)
    {
        if (not currContext->intermediateVec->empty())
        {
            areAllEmpty = false;

            K2 *currKey = currContext->intermediateVec->back().first;
            while (not (*k2Max < *currKey || *currKey < *k2Max || currContext->intermediateVec->empty())) // while there are more keys in currContext equal to k2Max
            {
                vectorOfCurrentKey->push_back(currContext->intermediateVec->back());
                currContext->intermediateVec->pop_back();

                if (not currContext->intermediateVec->empty())
                {
                    currKey = currContext->intermediateVec->back().first;
                }
                else
                {
                    break;
                }
            }
        }
    }

    if (vectorOfCurrentKey->empty() || areAllEmpty)
    {
        delete vectorOfCurrentKey; // If it's empty, delete it and return nullptr, so we don't waste memory
        return nullptr;
    }

    return vectorOfCurrentKey;
}


/** pops iteratively all maximum values **/
void shufflePhase(ThreadContext *context, vector<IntermediateVec*> *reduceVec)
{
    // Only thread 0 runs this part
    // Update the jobContext

    K2 *k2Max = getMaximumKey(context->jobContext->threads);
    IntermediateVec *vectorOfCurrentKey = popBackOfAllMaximum(context->jobContext->threads, k2Max);

    // Insert the first vector out of the loop (Ugly, but didn't find a better way, as we need k2Max initialized
    if (vectorOfCurrentKey != nullptr)
    {
        reduceVec->push_back(vectorOfCurrentKey);
        // Update percent
        context->jobContext->totalNumberOfShuffledPairs += vectorOfCurrentKey->size();
    }

    while (k2Max != nullptr && vectorOfCurrentKey != nullptr && context->jobContext->state->percentage < 100)
    {
        k2Max = getMaximumKey(context->jobContext->threads);
        vectorOfCurrentKey = popBackOfAllMaximum(context->jobContext->threads, k2Max);

        if (vectorOfCurrentKey != nullptr)
        {
            reduceVec->push_back(vectorOfCurrentKey);

            // Update percent
            context->jobContext->totalNumberOfShuffledPairs += vectorOfCurrentKey->size();
            updateAtomicPercent(context, SHUFFLE_STAGE, context->jobContext->totalNumberOfInterPairs.load(),
                                context->jobContext->totalNumberOfShuffledPairs);
        }
    }
}

void reducePhase(ThreadContext *context, const vector<IntermediateVec*> *reduceVec)
{
    // handle mutex
    if (pthread_mutex_lock(context->mutex))
    {
        cerr << "system error: mutex lock error"  << endl;
        exit(1);
    }

    int oldVal;

    while ((unsigned long) context->jobContext->placeInReducedVector.load() < reduceVec->size())
    {


        oldVal = context->jobContext->placeInReducedVector++;
        IntermediateVec *currKeyVec = reduceVec->at(static_cast<int>(oldVal));
        context->jobContext->totalNumberOfReducedPairs += currKeyVec->size();

        // Call emit3 through reduce, with the current interVector
        context->client->reduce(currKeyVec, context);
        updateAtomicPercent(context, REDUCE_STAGE, context->jobContext->totalNumberOfInterPairs.load(),
                            context->jobContext->totalNumberOfReducedPairs);
    }

    if (pthread_mutex_unlock(context->mutex))
    {
        cerr << "mutex unlock error" << endl;
        exit(1);
    }
}

void *mapReduce(ThreadContext *context, JobContext *jobContext)
{
//    std::cout << "Before Map, id: " << context->threadNum << std::endl; // TBD
    mapPhase(context);
//    std::cout << "After Map, id: " << context->threadNum << std::endl; // TBD

    sortPhase(context);
//    std::cout << "After Sort, id: " << context->threadNum << std::endl; // TBD


    context->barrier->barrier();
//    std::cout << "After First Barrier, id: " << context->threadNum << std::endl; // TBD


    if (context->threadNum == 0)
    {
//        std::cout << "Before Shuffle, id: " << context->threadNum << std::endl; // TBD
        shufflePhase(context, context->jobContext->reduceVec);
//        std::cout << "After Shuffle, id: " << context->threadNum << std::endl; // TBD
    }

//    std::cout << "Before Second Barrier, id: " << context->threadNum << std::endl; // TBD
    context->barrier->barrier();
//    std::cout << "After Second Barrier, id: " << context->threadNum << std::endl; // TBD

    reducePhase(context, context->jobContext->reduceVec);
//    std::cout << "After Reduce, id: " << context->threadNum << std::endl; // TBD

    context->jobContext->state->stage = REDUCE_STAGE;
//    std::cout << "After setting stage, id: " << context->threadNum << std::endl; // TBD

    return nullptr;
}

JobHandle startMapReduceJob(const MapReduceClient& client,
                            const InputVec& inputVec, OutputVec& outVec,
                            int multiThreadLevel)
{
    auto *semaphore = new sem_t();
    auto *atomicCounter = new atomic<int>(0);
    auto *threadContexts = new vector<ThreadContext*>();
    auto *threadsArray = new pthread_t[multiThreadLevel];
    auto *jobContext = new JobContext(threadContexts, threadsArray, outVec);
    auto *barrier = new Barrier(multiThreadLevel);
    auto *mutex = new pthread_mutex_t(PTHREAD_MUTEX_INITIALIZER);

    for (int i = 0; i < multiThreadLevel; ++i)
    {
        ThreadContext *context = new ThreadContext(jobContext, inputVec, semaphore,
                atomicCounter, &client, barrier, mutex, multiThreadLevel, i);
        threadContexts->push_back(context);

        if (pthread_create(threadsArray + i, nullptr, (void *(*)(void *))mapReduce, context))
        {
            cerr << "system error: pthread create error" << endl;
            exit(1);
        }
    }

//    if (jobContext->state->stage == UNDEFINED_STAGE)
//    {
//        std::cout << "inside if 406" << std::endl;
//        jobContext->atomicStageCounter = 0b0000000000000000000000000001010000000000000000000000000000000000;
//    }
//
    return jobContext;
}

void waitForJob(JobHandle job)
{
    auto *jobContext = (JobContext*) job;

    for (int i = 0; i < (int) jobContext->threads->size(); ++i)
    {
        pthread_join(jobContext->threadsArray[i], nullptr);
    }
}

void getJobState(JobHandle job, JobState* state)
{
    auto *jobContext = (JobContext*) job; // TBC - maybe lock and unlock mutex?

    unsigned long long int counter = jobContext->atomicStageCounter.load();
    stage_t currStage = (stage_t) (counter >> 62);
    unsigned long totalSizeOfStep = counter << 2;
    totalSizeOfStep = totalSizeOfStep >> 33;
    unsigned long numOfPairsDone = counter % FIRST_31_BITS;

     state->percentage = 100 * ((float) numOfPairsDone) / (float) totalSizeOfStep;

    if (state->percentage != state->percentage)
    {
        state->percentage = 0;
    }

    state->stage = currStage;
//    std::cout << "done: " << numOfPairsDone<< ", total size: " << totalSizeOfStep << std::endl; // TBD

//    std::cout << "percentage: " << state->percentage << "%, stage: " << state->stage << std::endl; // TBD
}

void closeJobHandle(JobHandle job)
{
    atomic<int> counter;
    waitForJob(job);
    auto jobContext = (JobContext *) job;

    vector<ThreadContext*>::iterator iter;

    for (auto & element : *jobContext->threads)
    {
        pthread_mutex_destroy(element->mutex);
    }

    delete jobContext;
}
