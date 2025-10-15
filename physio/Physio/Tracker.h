#pragma once
#include <string>
#include <iostream>

class Tracker
{
private:
	std::string name;
	size_t totalAllocated = 0;
	size_t totalFreed = 0;
	size_t currentlyUsed = 0;

public:
	Tracker(const std::string& trackerName) : name(trackerName) {} //constructor 

	void addAllocations(size_t size)
	{
		totalAllocated += size;
		currentlyUsed += size;
	}

	void freeAllocation(size_t size)
	{
		totalFreed += size;
		currentlyUsed -= size;
	}

	//used to print out current information onm either the cubes or the sphere or the total
	void printInfo() const
	{
		std::cout << "Tracker [" << name << "]\n "
			<< "Allocated: " << totalAllocated << " bytes, "
			<< "Freed: " << totalFreed << " bytes, "
			<< "In Use: " << currentlyUsed << " bytes\n";
	}
};