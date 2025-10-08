#pragma once
#include <vector>
#include <thread>

#define minX -10.0f
#define maxX 30.0f
#define minZ -30.0f
#define maxZ 30.0f

#define FLOORY 0.0f
#define CIELINGY 60.0f

// gravity - change it and see what happens (usually negative!)
const float gravity = -19.81f;

//void* operator new (size_t size)
//{
//	char* pMem = (char*)malloc(size);
//	void* pStartMemBlock = pMem;
//	return pStartMemBlock;
//}
//
//void operator delete (void* pMem)
//{
//	free(pMem);
//}