#pragma once
#include "ColliderObject.h"
#include "SpherePool.h"

class Sphere :
    public ColliderObject
{
public:
	inline void* operator new (size_t size)
	{
		void* mem = SpherePool::Get().Allocate(size);  //get a block of memory from the pool

		//if not enough memory space in pool, default to global new function
		if (!mem)
		{
			mem = ::operator new(size);
		}

		//assign header 
		Header* h = (Header*)mem;
		h->size = size;

		return (char*)mem + sizeof(Header);
	}


	inline void operator delete (void* pMem)
	{
		Header* h = (Header*)((char*)pMem - sizeof(Header));
		SpherePool::Get().Free(h);
	}

	Sphere()
	{
		ColliderTypeInt = ColliderType::SphereCollider;
	}

    void drawMesh() { glutSolidSphere(0.5, 5, 5); }
}; 

