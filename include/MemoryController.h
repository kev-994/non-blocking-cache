#pragma once

#include "CacheLine.h"
#include "CoherenceMessage.h"

#include <cstdint>
#include <unordered_map>

class Interconnect;

class MemoryController
{
public:
    MemoryController(Interconnect* interconnect)
        : m_interconnect{interconnect}
    {}

    void processBusMessage(const CoherenceMessage& msg);


private:
    std::unordered_map<std::uint64_t, CacheLine<std::uint8_t>> m_backing_store{};

    Interconnect* m_interconnect{};
};

/*
in real hardware, DRAM is just always there with some value (garbage or zero-initialized).
hash map avoids allocating massive empty arrays for unused memory space
*/