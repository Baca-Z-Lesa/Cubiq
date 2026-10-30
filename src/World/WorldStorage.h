#pragma once
#include <glm/vec2.hpp>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cubiq {

class Chunk;

// Uložené chunky sveta na disku.
//
// Ukladajú sa len chunky, ktoré hráč zmenil – ostatné sa dajú kedykoľvek znova
// vygenerovať zo seedu. Chunky sú zoskupené do regiónov 32×32 (jeden súbor
// r.X.Z.cbq na región) a komprimované RLE (dlhé úseky rovnakých blokov).
// Bezpečné pre viac vlákien: načítava sa z pracovných vlákien, ukladá z hlavného.
class WorldStorage {
public:
    explicit WorldStorage(std::filesystem::path directory);

    // Načíta uložený chunk; false = nie je uložený (treba ho vygenerovať).
    bool load(glm::ivec2 coord, Chunk& chunk);

    // Zapamätá chunk (na disk sa zapíše pri flush).
    void store(const Chunk& chunk);

    // Zapíše zmenené regióny na disk.
    void flush();

private:
    static constexpr int REGION = 32;

    struct Region {
        std::unordered_map<int, std::vector<uint8_t>> chunks; // index v regióne → RLE dáta
        bool dirty = false;
    };

    Region& region(glm::ivec2 regionCoord); // načíta zo súboru pri prvom použití
    std::filesystem::path regionPath(glm::ivec2 regionCoord) const;

    std::filesystem::path m_dir;
    std::mutex m_mutex;
    std::unordered_map<int64_t, Region> m_regions;
};

} // namespace cubiq
