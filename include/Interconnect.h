#pragma once

#include "CoherenceMessage.h"

#include <vector>
#include <cassert>

class Cache;
class MemoryController;

class Interconnect
{
public:
    void attachCache(Cache* cache);
    void attachMemory(MemoryController* memory); // setter
    void routeMessage(const CoherenceMessage& msg);
    void tick() const;

private:
    std::vector<Cache*> m_caches{};
    MemoryController* m_memory{};

};