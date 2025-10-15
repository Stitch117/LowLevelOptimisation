#pragma once
#include "globals.h"
#include "TrackerManager.h"

class SpherePool
{
private:
	//holds each space for a box
	struct Node
	{
		Node* next;
	};

	Node* freeList = nullptr; //potential spaces for boxes
	void* poolMemory = nullptr;
	size_t poolSize = 0;

	static SpherePool* instance;

public:
	static SpherePool& Get(size_t numSpheres = 0);

	//constructor
	SpherePool(size_t numSpheres);

	//add a box to the pool
	void* Allocate(size_t size);

	//remove the pool memory and make it "freelist" again
	void Free(void* ptr);
};
