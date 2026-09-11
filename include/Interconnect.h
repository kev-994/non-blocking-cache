#pragma once

#include "Cache.h"

#include <vector>

class Interconnect
{
public:
    void attach_cache(Cache* cache);
    void routeMessage(const CoherenceMessage& msg);

private:
    std::vector<Cache*> m_caches{};

};