#pragma once

#include "CoherenceMessage.h"
#include "MSHR.h"
#include "Interconnect.h"

#include <cstdint>
#include <vector>

enum class RequestStatus
{
    Hit, 
    Miss,
    Rejected
};

struct CacheBlock
{
    std::uint64_t tag{};
    MESIState state{};
    CacheLine<std::uint8_t> data{};
};

class Cache
{
public:
    Cache(std::uint64_t id, Interconnect* interconnect)
        : m_cache_id{id}, m_interconnect{interconnect}
    {}

    RequestStatus processCPURequest(TargetType type, std::uint64_t address, std::uint8_t write_data=0, std::uint8_t& read_data_out);
    void processBusMessage(const CoherenceMessage& msg);

private:
    std::size_t max_mshrs{};

    std::vector<CacheBlock> m_cache_blocks{}; // fully associative mapping
    std::vector<MSHREntry> m_MSHRFile{};
    std::uint64_t m_cache_id{}; 

    Interconnect* m_interconnect{};

    void forwardToCPU(std::uint64_t cpu_id, std::uint8_t data);

};