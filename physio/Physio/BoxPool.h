#pragma once
#include "globals.h"
#include "TrackerManager.h"

class BoxPool
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

	static BoxPool* instance;

public:
	static BoxPool& Get(size_t numBoxes = 0);

	//constructor
	BoxPool(size_t numBoxes);
	
	//add a box to the pool
	void* Allocate(size_t size);

	//remove the pool memory and make it "freelist" again
	void Free(void* ptr);
};

