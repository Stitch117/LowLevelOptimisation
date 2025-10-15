#include "BoxPool.h"
#include "Box.h" 

BoxPool* BoxPool::instance = nullptr;

BoxPool& BoxPool::Get(size_t numBoxes)
{
    if (!instance)
    {
        instance = new BoxPool(numBoxes);
    }
    return *instance;
}

BoxPool::BoxPool(size_t numBoxes)
{
    poolSize = numBoxes * (sizeof(Box) + sizeof(Header) + sizeof(Footer)); 
    poolMemory = malloc(poolSize); 
    char* p = (char*)poolMemory;

    for (size_t i = 0; i < numBoxes; i++)
    {
        Node* node = (Node*)p; 
        node->next = freeList; 
        freeList = node; 
        p += sizeof(Box) + sizeof(Header) + sizeof(Footer);
    }
}

void* BoxPool::Allocate(size_t size)
{

    Node* node = freeList;
    freeList = freeList->next;

    Header* h = (Header*)node;
    h->size = size;

    // memory tracking
    TrackerManager::GetTracker("Global").addAllocations(size);
    TrackerManager::GetTracker("GlobalWithHeaderAndFooter").addAllocations(size + sizeof(Header) + sizeof(Footer));
    TrackerManager::GetTracker("Boxes").addAllocations(size);
    TrackerManager::GetTracker("BoxesWithHeaderAndFooter").addAllocations(size + sizeof(Header) + sizeof(Footer));

    return node;
}

void BoxPool::Free(void* ptr)
{
    Header* h = (Header*)ptr;

    // memory tracking
    TrackerManager::GetTracker("Global").freeAllocation(h->size);
    TrackerManager::GetTracker("GlobalWithHeaderAndFooter").freeAllocation(h->size + sizeof(Header) + sizeof(Footer));
    TrackerManager::GetTracker("Boxes").freeAllocation(h->size);
    TrackerManager::GetTracker("BoxesWithHeaderAndFooter").freeAllocation(h->size + sizeof(Header) + sizeof(Footer));

    Node* node = (Node*)ptr;
    node->next = freeList;
    freeList = node;
}
