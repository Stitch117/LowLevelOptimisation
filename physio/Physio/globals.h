#pragma once
#include <vector>
#include <thread>
#include "TrackerManager.h"

#define minX -10.0f
#define maxX 30.0f
#define minZ -30.0f
#define maxZ 30.0f

#define FLOORY 0.0f
#define CIELINGY 60.0f

// this is the number of falling physical items. 
#define NUMBER_OF_BOXES 100
#define NUMBER_OF_SPHERES 100

// gravity - change it and see what happens (usually negative!)
const float gravity = -19.81f;

struct Header
{
	int size; 
};

struct Footer
{
	int reserved; 
};

inline void* operator new (size_t size)
{
	size_t nRequestedBytes = size + sizeof(Header) + sizeof(Footer);
	char* pMem = (char*)malloc(nRequestedBytes);
	Header* pHeader = (Header*)pMem; //pointer to header

	pHeader->size = size;

	void* pFooterAdd = pMem + sizeof(Header); //pointer to footer 
	Footer* pFooter = (Footer*)pFooterAdd; //cast the pointer

	void* pStartMemBlock = pMem + sizeof(Header);
	return pStartMemBlock;
}

inline void operator delete (void* pMem)
{
	Header* pHeader = (Header*)((char*)pMem - sizeof(Header));
	Footer* pFooter = (Footer*)((char*)pMem + pHeader->size);

	free(pHeader);
}