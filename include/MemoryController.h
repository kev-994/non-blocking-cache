#pragma once

#include "CacheLine.h"
#include "CoherenceMessage.h"

#include <cstdint>
#include <unordered_map>
#include <queue>

class Interconnect;

struct PendingRequest
{
    CoherenceMessage msg{};
    std::uint64_t resolve_cycle{};
};

class MemoryController
{
public:
    explicit MemoryController(Interconnect* interconnect)
        : m_interconnect{interconnect}
    {}

    void processBusMessage(const CoherenceMessage& msg);
    void tick();

private:
    std::unordered_map<std::uint64_t, CacheLine<std::uint8_t>> m_backing_store{};
    std::queue<PendingRequest> m_pending_requests{};
    std::uint64_t m_current_cycle{};
    const std::uint64_t m_memory_latency{10}; // simulated latency 10 cycles

    Interconnect* m_interconnect{};
};

/*
in real hardware, DRAM is just always there with some value (garbage or zero-initialized).
hash map avoids allocating massive empty arrays for unused memory space
*/