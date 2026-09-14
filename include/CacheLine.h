#pragma once

#include <array>

constexpr std::size_t CACHE_LINE_SIZE{64}; 

template <typename T> // byte or word
struct CacheLine
{
    std::array<T, CACHE_LINE_SIZE> data{};
};