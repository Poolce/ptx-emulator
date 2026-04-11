#pragma once

#include <cstdint>

struct dim3 // NOLINT(readability-identifier-naming)
{
    using type = uint32_t;
    uint32_t x, y, z;
    dim3(uint32_t _x = 1, uint32_t _y = 1, uint32_t _z = 1) : x(_x), y(_y), z(_z) {}
};

using uint3 = dim3; // NOLINT(readability-identifier-naming)

template <class T>
struct V3
{
    using type = T;

    type x = 0;
    type y = 0;
    type z = 0;
};

template <class T>
struct V4
{
    using type = T;

    V4(const V3<T>& vec) : x{vec.x}, y{vec.y}, z{vec.z} {}

    type x = 0;
    type y = 0;
    type z = 0;
    type w = 0;
};

using int4 = V4<int64_t>;   // NOLINT(readability-identifier-naming)
using uint4 = V4<uint64_t>; // NOLINT(readability-identifier-naming)
using uint4_32 = V4<uint32_t>;
