#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <iostream>
#define SUCCESS 1
#define FAILURE 0

typedef struct AllInfo
{
    uint64_t maximumFrameVisited;
    uint64_t currentlyVisitedSon; // TBD
    uint64_t currentPrev;
    uint64_t offSet;
    uint64_t pageNumber; //TBD
    uint64_t physicalAddr;
    uint64_t addrToEvict;
    uint64_t currentPage;
    uint64_t oldDist;



    uint64_t frameToReadWrite;
    word_t *value; // TBD
    bool toWrite; // TBD

    word_t *currFrame;
    word_t *frameToEvict;
    word_t *pageToEvict;
    word_t *emptyFrame;
    word_t *emptyAddr;

    bool *usedFrames;


} AllInfo;


uint64_t getDist(uint64_t n1, uint64_t n2)
{
    uint64_t dist;
    // dist = abs (n1 - n2)
    if (n1 < n2)
    {
        dist = n1 - n2;
    }
    else
    {
        dist = n2 - n1;
    }
    if (dist >= NUM_PAGES - dist)
    {
        return NUM_PAGES - dist;
    }
    else
    {
        return dist;
    }
}

uint64_t getOffset(uint64_t virtualAddress)
{
    return virtualAddress % ((uint64_t) 1 << OFFSET_WIDTH);
}

int saveFrameToEvict(AllInfo info)
{
    uint64_t newDist = getDist(info.physicalAddr, info.currentPage);
    if (newDist > info.oldDist)
    {
        info.oldDist = newDist;
        info.frameToEvict = info.currFrame;
        *info.pageToEvict = (word_t) info.currentPage;
        info.addrToEvict = info.currentPrev * PAGE_SIZE + info.offSet;
        return SUCCESS;
    }
    return FAILURE;
}

void saveEmptyFrame(AllInfo info)
{
    info.emptyFrame = info.currFrame;
    *info.emptyAddr = info.currentPrev * PAGE_SIZE * info.offSet;
}

//bool getIthBit(unsigned int i, uint64_t num) // TBD
//{
//    return ((num >> i) & 1u);
//}
//
//bool getIthMSBSet(unsigned int i, uint64_t num) // TBD
//{
//
//    return ((num >> i) & 1u);
//}


int VMtraverse(AllInfo &info)
{
    if (info.pageNumber == 0)
    {
        return FAILURE;
    }

    if (*info.currFrame > (word_t) info.maximumFrameVisited)
    {
        info.maximumFrameVisited = *info.currFrame;
    }

    // in case we reach a page:
    if (info.currentPrev == TABLES_DEPTH)
    {
        if (!info.usedFrames[(uint64_t) info.currFrame])
        {
            saveFrameToEvict(info);
        }
        return SUCCESS;
    }

    word_t nextFrame = 0;
    bool isEmpty = true;

    for (uint64_t page = 0; page < PAGE_SIZE; page++)
    {
        PMread((uint64_t) info.currFrame * PAGE_SIZE + page, &nextFrame);
        if (nextFrame)
        {
//            uint64_t nextFrame = ((int64_t) info.currentPage << OFFSET_WIDTH) + page;
            VMtraverse(info);
            isEmpty = false;
        }
    }
    if (!isEmpty && !info.usedFrames[(uint64_t) info.currFrame])
    {
        saveEmptyFrame(info);
    }

//    uint64_t currDepthAddr = 0;
//    for (unsigned int depth = 0; depth <= (unsigned int) TABLES_DEPTH; depth++)
//    {
//
//        PMread(currDepthAddr * PAGE_SIZE + );
//        depth = 1;
//
//    }

    return SUCCESS;
}

/***
 * @brief initialize array with offsets.
 * @param pageArr
 * @param virtualAddress
 */
void initPageArray(uint64_t pageArr[], uint64_t virtualAddress)
{
    for (int depth = TABLES_DEPTH; depth >= 0; depth--)
    {
        if (depth > 0)
        {
            pageArr[depth] = virtualAddress & (PAGE_SIZE - 1);
            virtualAddress = virtualAddress >> OFFSET_WIDTH;
        }
        else
        {
            pageArr[0] = virtualAddress;
        }
    }
}

void clearTable(uint64_t frameIndex)
{
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

uint64_t getPhysicalAddress(uint64_t virtualAddress, AllInfo info)
{
    uint64_t physicalAddress;
    word_t prevFrame = 0;
    word_t currFrame = 0;
    uint64_t maxFrame = 0;
    uint64_t pageArr[TABLES_DEPTH];
    bool usedFrames[NUM_FRAMES] = {true};
    info.usedFrames = usedFrames;

    uint64_t toRestore = virtualAddress >> OFFSET_WIDTH;
    initPageArray(pageArr, virtualAddress);

    for (int depth = 0; depth < TABLES_DEPTH; depth++)
    {
//        std::cout << "pageArr: " << pageArr[depth] << std::endl;
//        std::cout << "prevFrame: " << prevFrame << std::endl;
//        std::cout << "page size: " << PAGE_SIZE << std::endl;
        physicalAddress = prevFrame * PAGE_SIZE + pageArr[depth];
        PMread(physicalAddress, &currFrame);
        if (currFrame == 0)
        {
            info.addrToEvict = 0;
            info.oldDist = 0;
            info.frameToEvict = 0;
            info.pageToEvict = 0;
            info.emptyFrame = 0;
            info.emptyAddr = 0;
            VMtraverse(info);

            if (info.emptyFrame) // empty table.
            {
//                std::cout << "line 205: "  << currFrame << std::endl;
                currFrame = (uint64_t) info.emptyFrame;
                PMwrite((uint64_t) info.emptyAddr, 0);
            }

            else if (info.maximumFrameVisited + 1 < NUM_FRAMES) // unused frame.
            {
//                std::cout << "line 211: " << currFrame << std::endl;
                currFrame = (uint64_t) info.maximumFrameVisited + 1;
            }
            else // evict - all frames are already used.
            {
//                std::cout << "line 216: " << currFrame << std::endl;
                currFrame = (uint64_t ) info.frameToEvict;
                PMevict(currFrame, (uint64_t) info.pageToEvict);
                PMwrite((uint64_t ) info.addrToEvict, 0);
            }

            PMwrite(physicalAddress, currFrame);

            usedFrames[currFrame] = true;

            if (depth != TABLES_DEPTH - 1)
            {
                clearTable(currFrame);
            }
            else
            {
//                std::cout << "line 233: " << currFrame << std::endl;
                PMrestore(currFrame, toRestore);
            }



        }
        currFrame = prevFrame;
//        prevFrame = currFrame;
    }
    return currFrame * PAGE_SIZE + getOffset(virtualAddress);
}

void VMinitialize()
{
    clearTable(0);
}


int VMread(uint64_t virtualAddress, word_t* value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) // TODO check if value is of the right number of bits?
    {
        return FAILURE;
    }

    AllInfo info = {0};
//    info.offSet = virtualAddress % ((uint64_t) 1 << OFFSET_WIDTH);
//    info.pageNumber = virtualAddress >> OFFSET_WIDTH;
//    info.toWrite = false;
//    info.value = value;
    std::cout << "val: " << value << std::endl;
    PMread(getPhysicalAddress(virtualAddress, info), value);
    return SUCCESS;
}


int VMwrite(uint64_t virtualAddress, word_t value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) // TODO check if value is of the right number of bits?
    {
        return FAILURE;
    }
    AllInfo info = {0};
//    info.offSet = virtualAddress % ((uint64_t) 1 << OFFSET_WIDTH);
//    info.pageNumber = virtualAddress >> OFFSET_WIDTH;
//    info.toWrite = true;
    PMwrite(getPhysicalAddress(virtualAddress, info), value);

    return SUCCESS;
}
