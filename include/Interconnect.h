#pragma once

#include "CoherenceMessage.h"

#include <vector>
#include <cassert>

class Cache;

class Interconnect
{
public:
    void attachCache(Cache* cache);
    void routeMessage(const CoherenceMessage& msg);

private:
    std::vector<Cache*> m_caches{};

};