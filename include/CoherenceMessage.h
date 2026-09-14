#pragma once

#include "CacheLine.h"

#include <cstdint>

constexpr std::uint64_t MEMORY_ID{0xFFFF};

enum class MESIState
{
    Invalid,
    Shared,
    Exclusive,
    Modified,

    InvalidShared, // read miss (e.g., waiting for data)
    InvalidModified // write miss (e.g., waiting for invalidation acknowledgments from other caches)
};

enum class MessageType
{
    ReadRequest,
    ReadResponse,

    WriteRequest,

    Writeback,

    SnoopRead,
    SnoopInvalidate,
    SnoopAck
};

struct CoherenceMessage
{
    MessageType type{};
    std::uint64_t address{};
    std::uint64_t transaction_id{};
    std::uint64_t sender_id{};
    std::uint64_t receiver_id{};
    CacheLine<std::uint8_t> data{};
};