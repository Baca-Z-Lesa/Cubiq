#pragma once
#include "World/Noise.h"

#include <glm/vec2.hpp>
#include <cstdint>

namespace cubiq {

class Chunk;

enum class Biome : uint8_t {
    Ocean,
    Beach,
    River,
    Plains,
    Forest,
    Desert,
    Tundra,
    Mountains,
    SnowyPeaks,
    Count
};

const char* biomeName(Biome biome);

// Procedurálny terén zo simplex noise. Iba na čítanie → bezpečný pre viac vlákien.
//
// Výška stĺpca sa skladá z vrstiev:
//   kontinenty (oceán ↔ pevnina) → hory (ridged noise, len na pevnine) → detail → rieky
// Biom určuje teplota a vlhkosť (+ výška a blízkosť vody).
class TerrainGenerator {
public:
    static constexpr int SEA_LEVEL = 62;

    explicit TerrainGenerator(uint32_t seed);

    void generate(Chunk& chunk) const;

    int heightAt(int x, int z) const;
    Biome biomeAt(int x, int z) const;
    uint32_t seed() const { return m_seed; }

    // Najbližšie miesto na pevnine okolo (0,0) – aby sa hráč nespawnol v mori.
    glm::ivec2 findSpawn() const;

private:
    struct Column {
        int height;
        Biome biome;
        bool cold; // studená klíma (sneh aj mimo tundry, napr. v horách)
    };
    Column sampleColumn(int x, int z) const;

    uint32_t m_seed;
    float m_offsetX, m_offsetZ; // posun podľa seedu (inak by bol bod (0,0) pre každý seed rovnaký)
    SimplexNoise m_continent, m_mountain, m_ridge, m_detail, m_river, m_temperature, m_humidity;
};

} // namespace cubiq
