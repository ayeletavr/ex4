#include "osm.h"
#include <iostream>
#include <sys/time.h>
#include <math.h>
using namespace std;

// g++ -Wextra -Wall -Wvla -std=c++11 -lm osm.cpp -o osm_ex1

// ---consts and defines---
#define ERROR -1
#define ADD_INSTRUCTION 1
#define EMPTY_FUNC 2
#define TRAP 3
const unsigned int UNROLLING_COEFF = 5;

void emptyFunc() {}


double osm_operation_time(unsigned int iterations) {
    /** Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
    if (iterations == 0) {
        return ERROR;
    }

    timeval before, after;

    double timeSec, timeMicro;

    gettimeofday(&before, NULL);
    unsigned int x;
    x = 0;
    for (unsigned int i = 0; i < iterations / UNROLLING_COEFF; i += 1) {
        x = 1 + 1;
        x = 1 + 1;
        x = 1 + 1;
        x = 1 + 1;
        x = 1 + 1;
    }
    gettimeofday(&after, NULL);

    timeSec = (double) after.tv_sec - before.tv_sec;
    timeMicro = (double) after.tv_usec - before.tv_usec;

    return ((timeSec * 1000000 + timeMicro) * 1e3) / iterations;

}

double osm_function_time(unsigned int iterations) {
    /** Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
    if (iterations == 0) {
        return ERROR;
    }

    timeval before, after;

    double timeSec, timeMicro;

    gettimeofday(&before, NULL);
    for (unsigned int i = 0; i < iterations / UNROLLING_COEFF; i++) {
        emptyFunc();
        emptyFunc();
        emptyFunc();
        emptyFunc();
        emptyFunc();
    }
    gettimeofday(&after, NULL);


    timeSec = (double) after.tv_sec - before.tv_sec;
    timeMicro = (double) after.tv_usec - before.tv_usec;

    return ((timeSec * 1000000 + timeMicro) * 1e3) / iterations;
}

double osm_syscall_time(unsigned int iterations) {
    /** Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
    if (iterations == 0) {
        return ERROR;
    }

    timeval before, after;

    double timeSec, timeMicro;

    gettimeofday(&before, NULL);
    for (unsigned int i = 0; i < iterations / UNROLLING_COEFF; i++) {
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
    }
    gettimeofday(&after, NULL);

    timeSec = (double) after.tv_sec - before.tv_sec;
    timeMicro = (double) after.tv_usec - before.tv_usec;

    return ((timeSec * 1000000 + timeMicro) * 1e3) / iterations;
}


int main(int argc, char *argv[])
{
    int iterations;

    iterations = 100000;
    cout << "function: " << osm_function_time(iterations) << "\n"
    << "addition: " << osm_operation_time(iterations) << "\n" <<
    "trap: " << osm_syscall_time(iterations) << "\n";



    return 0;
}