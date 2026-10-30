#pragma once
#include "World/Block.h"
#include "World/Noise.h"

#include <glm/glm.hpp>
#include <cstdint>

namespace cubiq {

class Chunk;

enum class Biome : uint8_t {
    Ocean,
    Beach,
    River,
    Plains,
    Forest,
    BirchForest,
    CherryGrove,
    Desert,
    Badlands,
    Tundra,
    Mountains,
    SnowyPeaks,
    Count
};

const char* biomeName(Biome biome);

// Procedurálny terén. Iba na čítanie → bezpečný pre viac vlákien.
//
// 2D výška stĺpca:  kontinenty → hory (ridged) → detail → mesa/terasy → rieky (len v nížinách)
// 3D tvarovanie:    v horách a mesách 3D noise vytvára útesy, previsy a skalné oblúky
// Nad tým:          lietajúce ostrovy s visiacimi skalami
// Pod tým:          jaskyne (tunely + veľké dutiny), láva, rudy
// Na povrchu:       stromy a kaktusy (Features.cpp)
class TerrainGenerator {
public:
    static constexpr int SEA_LEVEL = 62;

    // Všetko, čo sa dá o stĺpci zistiť bez 3D noise.
    struct Column {
        int height;           // 2D výška terénu
        Biome biome;
        bool cold;            // studená klíma (sneh aj mimo tundry)
        float overhang;       // 0..1 – sila 3D tvarovania (útesy, previsy, oblúky)
        float islandShape;    // 0..1 – sila lietajúceho ostrova nad stĺpcom (0 = žiadny)
        int islandCenter;     // výška stredu ostrova
        int snowLine;         // nad touto výškou je všade sneh
        glm::vec3 tint;       // farba trávy a listov
    };

    // Najvyšší pevný blok stĺpca (aj na lietajúcom ostrove).
    struct Surface {
        int y;
        BlockId block;
        Biome biome;
        bool island;
    };

    explicit TerrainGenerator(uint32_t seed);

    void generate(Chunk& chunk) const;

    Column columnAt(int x, int z) const;
    Surface surfaceAt(int x, int z) const;
    int heightAt(int x, int z) const { return columnAt(x, z).height; }
    Biome biomeAt(int x, int z) const { return columnAt(x, z).biome; }
    uint32_t seed() const { return m_seed; }

    // Najbližšie miesto na pevnine okolo (0,0) – aby sa hráč nespawnol v mori.
    glm::ivec2 findSpawn() const;

    // Deterministické náhodné číslo 0..1 pre stĺpec (stromy, rudy).
    float random01(int x, int z, uint32_t salt) const;

private:
    // Hodnoty 3D noise priamo v bodoch mriežky (medzi nimi sa interpoluje).
    float overhangRaw(int x, int y, int z) const;
    float islandRaw(int x, int y, int z) const;
    float overhangAt(int x, int y, int z) const;
    float islandAt(int x, int y, int z) const;

    static bool terrainSolid(int y, const Column& c, float overhangNoise);
    static bool islandSolid(int y, const Column& c, float islandNoise);
    BlockId terrainMaterial(int x, int y, int z, int depth, const Column& c, int slope) const;
    BlockId islandMaterial(int depth, const Column& c) const;
    BlockId strata(int x, int y, int z) const;

    uint32_t m_seed;
    float m_offsetX, m_offsetZ; // posun podľa seedu (inak by bol bod (0,0) pre každý seed rovnaký)
    SimplexNoise m_continent, m_mountain, m_ridge, m_detail, m_river, m_temperature, m_humidity;
    SimplexNoise m_warp, m_mesa, m_spire, m_island, m_cherry, m_snow, m_strata;
    SimplexNoise m_overhang3, m_island3, m_caveA, m_caveB, m_caveC;
};

} // namespace cubiq
