#include "SpherePool.h"
#include "Sphere.h" 

SpherePool* SpherePool::instance = nullptr;

SpherePool& SpherePool::Get(size_t numSpheres)
{
    if (!instance)
    {
        instance = new SpherePool(numSpheres);
    }
    return *instance;
}

SpherePool::SpherePool(size_t numSpheres)
{
    poolSize = numSpheres * (sizeof(Sphere) + sizeof(Header) + sizeof(Footer));
    poolMemory = malloc(poolSize);
    char* p = (char*)poolMemory;

    for (size_t i = 0; i < numSpheres; i++)
    {
        Node* node = (Node*)p;
        node->next = freeList;
        freeList = node;
        p += sizeof(Sphere) + sizeof(Header) + sizeof(Footer);
    }
}

void* SpherePool::Allocate(size_t size)
{
    if (!freeList) return nullptr; // no free memory
    Node* node = freeList;
    freeList = node->next;


    // tracking data updates
    TrackerManager::GetTracker("Global").addAllocations(size);
    TrackerManager::GetTracker("GlobalWithHeaderAndFooter").addAllocations(size + sizeof(Header) + sizeof(Footer));
    TrackerManager::GetTracker("Spheres").addAllocations(size);
    TrackerManager::GetTracker("SpheresWithHeaderAndFooter").addAllocations(size + sizeof(Header) + sizeof(Footer));


    return node;
}

void SpherePool::Free(void* ptr)
{
    Header* h = (Header*)ptr;

    // tracking data update
    TrackerManager::GetTracker("Global").freeAllocation(h->size);
    TrackerManager::GetTracker("GlobalWithHeaderAndFooter").freeAllocation(h->size + sizeof(Header) + sizeof(Footer));
    TrackerManager::GetTracker("Spheres").freeAllocation(h->size);
    TrackerManager::GetTracker("SpheresWithHeaderAndFooter").freeAllocation(h->size + sizeof(Header) + sizeof(Footer));

    Node* node = (Node*)ptr;
    node->next = freeList;
    freeList = node;
}
