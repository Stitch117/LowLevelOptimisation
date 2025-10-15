#pragma once
#include "Tracker.h"
#include <unordered_map> //unordered map is quickere to reqad from than a map if data doesn't need to be ordered
#include <string>

class TrackerManager
{
private:
	static std::unordered_map<std::string, Tracker*> trackers; //store the names with the tracker type in an unordered dictionary

public:
    static Tracker& GetTracker(const std::string& name)
    {
        //add the tracke to the list ifit can't find it
        if (trackers.find(name) == trackers.end())
        {
            trackers[name] = new Tracker(name);
        }

        return *trackers[name];
    }

    //print function call from the tracker half of the dictionary entry
    static void PrintAll()
    {
        for (auto& pair : trackers)
            pair.second->printInfo();
    }

    static void Cleanup()
    {
        for (auto& pair : trackers)
            delete pair.second;
        trackers.clear();
    }
};
