// Copyright 2026 RainYangty - SPDX-License-Identifier: Apache-2.0

#ifndef SEREIN_GRID_DETAIL_COORDINATES_HPP
#define SEREIN_GRID_DETAIL_COORDINATES_HPP

#include <cstdint>

namespace sereingrid
{
namespace detail
{

inline int floor_div(int value, int divisor)
{
    return (value >= 0) ? (value / divisor) : ((value - divisor + 1) / divisor);
}

inline int floor_mod(int value, int divisor)
{
    int remainder = value % divisor;
    return remainder >= 0 ? remainder : remainder + divisor;
}

inline uint64_t make_key(int U, int V)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(U)) << 32) | static_cast<uint32_t>(V);
}

inline void global_to_local(int N, int x, int y, int& U, int& V, int& u, int& v)
{
    U = floor_div(x, N);
    V = floor_div(y, N);
    u = floor_mod(x, N);
    v = floor_mod(y, N);
}

inline void macro_to_global(int N, int U, int V, int& x, int& y)
{
    x = U * N;
    y = V * N;
}

inline void local_to_global(int N, int U, int V, int u, int v, int& x, int& y)
{
    x = U * N + u;
    y = V * N + v;
}

} // namespace detail
} // namespace sereingrid

#endif // SEREIN_GRID_DETAIL_COORDINATES_HPP
