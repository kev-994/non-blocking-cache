#pragma once

#include <cstdint>
#include <array>

constexpr std::size_t CACHE_LINE_SIZE{64}; 

enum class MESIState
{
    Invalid,
    Shared,
    Exclusive,
    Modified,

    InvalidShared, // read miss (e.g., waiting for data)
    InvalidModified // write miss (e.g., waiting for invalidation acknowledgments from other caches)
};

template <typename T> // byte or word
struct CacheLine
{
    std::array<T, CACHE_LINE_SIZE> data{};
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