#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <iostream>
#define SUCCESS 1
#define FAILURE 0


/***
 * @brief initialize array with offsets.
 * @param virtAddrByBits
 * @param virtualAddress
 */
int initVirtAddrByBits(uint64_t *virtAddrByBits, uint64_t virtualAddress)
{
    uint64_t requestedPageNumber = virtualAddress >> OFFSET_WIDTH;
    if (requestedPageNumber == 0) // TODO: check if this is what we should do
    {
        return FAILURE;
    }

    for (int depth = TABLES_DEPTH; depth > 0; depth--)
    {
        virtAddrByBits[depth] = virtualAddress & (PAGE_SIZE - 1);
        virtualAddress = virtualAddress >> OFFSET_WIDTH;
    }
    virtAddrByBits[0] = virtualAddress;
    return SUCCESS;
}

void clearTable(uint64_t frameIndex)
{
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}


void findNodeToRemove(
        uint64_t &currNodeAddr, uint64_t &prevNodeAddr, uint64_t &maximumFrameVisited, uint64_t &currDepth,
        uint64_t &nodeToRemove, uint64_t &currPage, uint64_t &prevOfNodeToRemove,
        uint64_t &pageOfNodeOfMaxWeight, uint64_t &maxWeightVal, uint64_t &weightOfCurrNode, bool &foundEmptyNode,
        const uint64_t ourFather    )
{
    if (currNodeAddr > maximumFrameVisited)
    {
        currNodeAddr = maximumFrameVisited;
    }

    // in case we reach a page (i.e., leaf), update the maximal weight value
    if (currDepth == TABLES_DEPTH)
    {
        weightOfCurrNode += currPage % 2 == 0 ? currPage * WEIGHT_EVEN : currPage * WEIGHT_ODD;

        // If we found the node of maximal weight, set it to be evicted (nodeToRemove)
        if ((weightOfCurrNode > maxWeightVal || (weightOfCurrNode == maxWeightVal && currPage < pageOfNodeOfMaxWeight))
            && currNodeAddr != ourFather)
        {
            nodeToRemove = currNodeAddr;
            pageOfNodeOfMaxWeight = currPage;
            maxWeightVal = weightOfCurrNode;
            prevOfNodeToRemove = prevNodeAddr;
        }
        return;
    }

    word_t nextFrame = 0;
    bool isAllSonsZeros = true; // if it stays true after the loop, we found an empty node, which can be removed

    for (uint64_t offSet = 0; offSet < PAGE_SIZE; offSet++)  // Iterate all sons
    {
        PMread((uint64_t) currNodeAddr * PAGE_SIZE + offSet, &nextFrame);
        if (nextFrame != 0)
        {
            isAllSonsZeros = false;

            // Before entering the recursion, save all of the current values
            uint64_t tempPrevAddr = prevNodeAddr;
            uint64_t tempCurrAddr = currNodeAddr;
            uint64_t tempCurrPage = currPage;
            uint64_t tempCurrWeight = weightOfCurrNode;

            // Update to the correct values of inside the recursion
            prevNodeAddr = currNodeAddr; // Update pointer to previous node
            currNodeAddr = (uint64_t) nextFrame;
            currPage = currPage * PAGE_SIZE + offSet; // Update page number recursively. This includes data on offset
            weightOfCurrNode += currNodeAddr % 2 == 0 ? currNodeAddr * WEIGHT_EVEN : currNodeAddr * WEIGHT_ODD;
            currDepth++; // Enter a deeper level by recursively calling the DFS

            findNodeToRemove(
                    currNodeAddr, prevNodeAddr, maximumFrameVisited, currDepth, nodeToRemove,
                    currPage, prevOfNodeToRemove, pageOfNodeOfMaxWeight,
                    maxWeightVal, weightOfCurrNode, foundEmptyNode, ourFather
            );

            // Step out of the recursion and reset the 'global' values to the previous value
            currDepth--;
            weightOfCurrNode -= tempCurrWeight; // Update current node's weight
            currPage = tempCurrPage;
            currNodeAddr = tempCurrAddr;
            prevNodeAddr = tempPrevAddr;
        }
    }
    if (isAllSonsZeros && currNodeAddr != ourFather)
    {
        foundEmptyNode = true;
        nodeToRemove = currNodeAddr;
        prevOfNodeToRemove = prevNodeAddr;
        return;
    }
}

// A function to restart all 'Global' values to 0 before every call to the DFS
void initializeAllGlobals(
        uint64_t &currNodeAddr, uint64_t &prevNodeAddr, uint64_t &maximumFrameVisited, uint64_t &currDepth,
        uint64_t &nodeToRemove, uint64_t &currPage, uint64_t &prevOfNodeToRemove,
        uint64_t &pageOfNodeOfMaxWeight, uint64_t &maxWeightVal, uint64_t &weightOfCurrNode, bool &foundEmptyNode    )
{
    // 'global' variables for the recursive DFS (findNodeToRemove):
    currNodeAddr = 0; // Current node in the DFS
    prevNodeAddr = 0; // pi(currNodeAddr) in the DFS: for easily removing an empty node
    maximumFrameVisited = 0;
    currDepth = 0;

    nodeToRemove = 0; // The node to evict or remove
    currPage = 0; // Updates in case we are in a page, i.e., leaf
    prevOfNodeToRemove = 0;

    pageOfNodeOfMaxWeight = 0;
    maxWeightVal = 0;
    weightOfCurrNode = 0;

    foundEmptyNode = false; // If it stays false, we should take maximal unvisited node or evict on
}


uint64_t getPhysicalAddress(uint64_t virtualAddress)
{
    // 'global' variables for the recursive DFS (findNodeToRemove):
    uint64_t currNodeAddr = 0; // Current node in the DFS
    uint64_t prevNodeAddr = 0; // pi(currNodeAddr) in the DFS: for easily removing an empty node
    uint64_t maximumFrameVisited = 0;
    uint64_t currDepth = 0;


    uint64_t nodeToRemove = 0; // The node to evict or remove
    uint64_t currPage = 0; // Updates in case we are in a page, i.e., leaf
    uint64_t prevOfNodeToRemove = 0;

    uint64_t pageOfNodeOfMaxWeight = 0;
    uint64_t maxWeightVal = 0;
    uint64_t weightOfCurrNode = 0;

    bool foundEmptyNode = false; // If it stays false, we should take maximal unvisited node or evict one

    // local variables for our searching function
    uint64_t currPhysicalAddress = 0;
    word_t currFrame = 0;
    uint64_t ourFather = 0; // To make sure we don't evict our own father
    uint64_t addrInBitsArr[TABLES_DEPTH]; // Initialize the page array
    initVirtAddrByBits(addrInBitsArr, virtualAddress);

    uint64_t virtualAddressToRestore = virtualAddress >> OFFSET_WIDTH;

    for (uint64_t depth : addrInBitsArr)
    {
        ourFather = currFrame; // Save the node that points to us before updating
        currPhysicalAddress = currFrame * PAGE_SIZE + depth; // next address to visit
        PMread(currPhysicalAddress, &currFrame);  // Update current frame

        if (currFrame == 0) // If the node doesn't exist, we need to find an empty node or evict one
        {
            initializeAllGlobals(currNodeAddr, prevNodeAddr, maximumFrameVisited, currDepth,
                                 nodeToRemove,
                                 currPage, prevOfNodeToRemove, pageOfNodeOfMaxWeight,
                                 maxWeightVal, weightOfCurrNode, foundEmptyNode);
            findNodeToRemove(
                    currNodeAddr, prevNodeAddr, maximumFrameVisited, currDepth, nodeToRemove,
                    currPage, prevOfNodeToRemove, pageOfNodeOfMaxWeight,
                    maxWeightVal, weightOfCurrNode, foundEmptyNode, ourFather
            );

            if (nodeToRemove == 0) {std::cerr << "Evicted frame0, you have a bug" << std::endl;} // TBD, sanity check

            if (foundEmptyNode) // If there's a node with no sons, just remove it
            {
                currFrame = nodeToRemove; // In this case, the empty one
                clearTable(currPhysicalAddress); // The previous of it now points to 0 EVERYWHERE
            }
            else if (maximumFrameVisited + 1 < NUM_FRAMES) // There is an unused frame
            {
                currFrame = (uint64_t) maximumFrameVisited + 1;
                clearTable(currFrame); // It may have garbage values, we initialize it to 0
            }
            else // All frames are already used, remove node with maximal weight
            {
                uint64_t offsetOfNodeToRemove = pageOfNodeOfMaxWeight % PAGE_SIZE;
                PMevict(nodeToRemove, pageOfNodeOfMaxWeight);
                PMwrite(prevOfNodeToRemove * PAGE_SIZE + offsetOfNodeToRemove,0); // Update pointer
            }

            // In any case, update the current node to point to the added one (currFrame)
            PMwrite(currPhysicalAddress, currFrame);
        }
        
        // This is after we found a page (leaf), so we should insert it into the map of pages <-> addresses:
        PMrestore(currFrame, virtualAddressToRestore);
    }
    return currFrame * PAGE_SIZE + (virtualAddress % ((uint64_t) 1 << OFFSET_WIDTH));
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

    PMread(getPhysicalAddress(virtualAddress), value);
    return SUCCESS;
}


int VMwrite(uint64_t virtualAddress, word_t value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) // TODO check if value is of the right number of bits?
    {
        return FAILURE;
    }
    PMwrite(getPhysicalAddress(virtualAddress), value);
    return SUCCESS;
}
