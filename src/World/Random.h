#pragma once
#include <cstdint>

namespace cubiq {

// Malý rýchly generátor náhodných čísel (xorshift) – deterministický podľa seedu.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
    uint32_t next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float f() { return (next() & 0xFFFFFF) / 16777216.0f; }
    int range(int a, int b) { return a + (int)(next() % (uint32_t)(b - a + 1)); }
};

// Hash troch súradníc a seedu (pre deterministické rozhodnutia podľa polohy).
inline uint32_t hashCoords(int x, int y, int z, uint32_t seed)
{
    uint32_t h = seed ^ (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)(z * 83492791);
    h = (h ^ (h >> 15)) * 2246822519u;
    return h ^ (h >> 13);
}

} // namespace cubiq
