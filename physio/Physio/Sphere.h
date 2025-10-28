#pragma once
#include "ColliderObject.h"
class Sphere :
    public ColliderObject
{
public:
	inline void* operator new (size_t size)
	{
		size_t nRequestedBytes = size + sizeof(Header) + sizeof(Footer);
		char* pMem = (char*)malloc(nRequestedBytes);
		Header* pHeader = (Header*)pMem; //pointer to header

		pHeader->size = size;

		void* pFooterAdd = pMem + sizeof(Header); //pointer to footer 
		Footer* pFooter = (Footer*)pFooterAdd; //cast the pointer

		//memory tracking data
		TrackerManager::GetTracker("Global").addAllocations(size);
		TrackerManager::GetTracker("GlobalWithHeaderAndFooter").addAllocations(nRequestedBytes);
		TrackerManager::GetTracker("Sphere").addAllocations(size);
		TrackerManager::GetTracker("SphereWithHeaderAndFooter").addAllocations(nRequestedBytes);

		void* pStartMemBlock = pMem + sizeof(Header);
		return pStartMemBlock;
	}


	inline void operator delete (void* pMem)
	{
		Header* pHeader = (Header*)((char*)pMem - sizeof(Header));
		Footer* pFooter = (Footer*)((char*)pMem + pHeader->size);

		//memory tracking data
		TrackerManager::GetTracker("Global").freeAllocation(pHeader->size);
		TrackerManager::GetTracker("GlobalWithHeaderAndFooter").freeAllocation(pHeader->size + sizeof(Header) + sizeof(Footer));
		TrackerManager::GetTracker("Sphere").freeAllocation(pHeader->size);
		TrackerManager::GetTracker("SphereWithHeaderAndFooter").freeAllocation(pHeader->size + sizeof(Header) + sizeof(Footer));


		free(pHeader);
	}

	Sphere()
	{
		ColliderTypeInt = ColliderType::SphereCollider;
	}

    void drawMesh() { glutSolidSphere(0.5, 5, 5); }
}; 

