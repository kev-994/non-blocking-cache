#pragma once

#include "CoherenceMessage.h"

#include <cstdint>
#include <vector>

enum class TargetType
{
    Read,
    Write
};

struct Target // a single pending cpu instruction
{
    TargetType type{};
    std::uint64_t requester_id{};
    std::uint8_t offset{};
    std::uint8_t write_data{};
};

struct MSHREntry
{
    std::uint64_t block_address{}; // the base, cache-line-aligned physical address
    MESIState transient_state{};
    bool request_issued{};
    std::vector<Target> targets{}; // queue up all the individual cpu requests waiting on this block, in reality this would be bounded
};