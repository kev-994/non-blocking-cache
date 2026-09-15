#pragma once

#include "CoherenceMessage.h"
#include "MSHR.h"
#include "CacheLine.h"

#include <cstdint>
#include <vector>
#include <iostream>
#include <format>

class Interconnect;

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
    std::uint64_t last_accessed{};
};

class Cache
{
public:
    Cache(std::uint64_t id, Interconnect* interconnect, std::size_t total_caches)
        : m_cache_id{id}, m_interconnect{interconnect}, m_total_caches{total_caches}
    {}

    RequestStatus processCPURequest(TargetType type, std::uint64_t address, std::uint8_t write_data, std::uint8_t& read_data_out);
    bool processBusMessage(const CoherenceMessage& msg);
    bool hasValidBlock(std::uint64_t address) const;
    void evictLRUBlock();

private:
    const std::size_t max_mshrs{4};
    std::size_t m_total_caches{};
    const std::size_t m_max_blocks{4}; 
    std::uint64_t m_access_counter{0};

    std::vector<CacheBlock> m_cache_blocks{}; // fully associative mapping
    std::vector<MSHREntry> m_MSHRFile{};
    std::uint64_t m_cache_id{}; 

    Interconnect* m_interconnect{};

};