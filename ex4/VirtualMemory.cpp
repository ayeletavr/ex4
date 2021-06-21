#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#define SUCCESS 1
#define FAILURE 0

typedef struct AllInfo
{
    uint64_t maximumFrameVisited;
    uint64_t currentlyVisitedSon;
    uint64_t currentPrev;
    uint64_t offSet;
    uint64_t pageNumber;

    uint64_t frameToReadWrite;
    word_t *value;
    bool toWrite;



} AllInfo;


bool getIthBit(unsigned int i, uint64_t num)
{
    return ((num >> i) & 1u);
}

bool getIthMSBSet(unsigned int i, uint64_t num)
{

    return ((num >> i) & 1u);
}

int VMtraverse(AllInfo &info)
{
    if (info.pageNumber == 0)
    {
        return FAILURE;
    }

    uint64_t


    uint64_t currDepthAddr = 0;
    for (unsigned int depth = 0; depth <= (unsigned int) TABLES_DEPTH; depth++)
    {

        PMread(currDepthAddr * PAGE_SIZE + );
        depth = 1;

    }

    return SUCCESS;
}

void clearTable(uint64_t frameIndex)
{
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

void VMinitialize()
{
    clearTable(0);
}


int VMread(uint64_t virtualAddress, word_t* value)
{
    if (virtualAddress > VIRTUAL_MEMORY_SIZE) // TODO check if value is of the right number of bits?
    {
        return FAILURE;
    }

    AllInfo info = {0};
    info.offSet = virtualAddress % ((uint64_t) 1 << OFFSET_WIDTH);
    info.pageNumber = virtualAddress >> OFFSET_WIDTH;
    info.toWrite = false;
    info.value = value;

    return SUCCESS;
}


int VMwrite(uint64_t virtualAddress, word_t value)
{
    if (virtualAddress > VIRTUAL_MEMORY_SIZE) // TODO check if value is of the right number of bits?
    {
        return FAILURE;
    }
    AllInfo info = {0};
    info.offSet = virtualAddress % ((uint64_t) 1 << OFFSET_WIDTH);
    info.pageNumber = virtualAddress >> OFFSET_WIDTH;
    info.toWrite = true;

    return SUCCESS;
}
