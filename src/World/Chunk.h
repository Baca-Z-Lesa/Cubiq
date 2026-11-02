#pragma once
#include "Engine/Mesh.h"
#include "World/Block.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace cubiq {

// Sekcie chunku (16×16×16) pre cave culling.
constexpr int SECTION_HEIGHT = 16;
constexpr int CHUNK_SECTIONS = 256 / SECTION_HEIGHT;

// Strany sekcie: 0 = +Z, 1 = -Z, 2 = +X, 3 = -X, 4 = +Y, 5 = -Y (rovnako ako steny v mesheri).
inline int oppositeFace(int f) { return f ^ 1; }

// Bit pre dvojicu strán (a, b), a != b – 15 dvojíc v uint16_t.
inline uint16_t facePairBit(int a, int b)
{
    if (a > b) { int t = a; a = b; b = t; }
    static const int OFFSET[6] = {0, 5, 9, 12, 14, 15};
    return (uint16_t)(1u << (OFFSET[a] + (b - a - 1)));
}
constexpr uint16_t ALL_CONNECTED = 0x7FFF;

class Chunk {
public:
    static constexpr int SIZE_X = 16;
    static constexpr int SIZE_Y = 256;
    static constexpr int SIZE_Z = 16;

    explicit Chunk(glm::ivec2 coord);

    static bool inBounds(int x, int y, int z)
    {
        return x >= 0 && x < SIZE_X && y >= 0 && y < SIZE_Y && z >= 0 && z < SIZE_Z;
    }

    // Lokálne súradnice v rámci chunku.
    BlockId get(int x, int y, int z) const { return m_blocks[index(x, y, z)]; }
    void set(int x, int y, int z, BlockId id)
    {
        m_blocks[index(x, y, z)] = id;
        if (id != BlockId::Air && y > m_maxY) m_maxY = y;
        dirty = true;
        ++m_version;
    }

    // Riadok 16 blokov pozdĺž X (pre rýchle kopírovanie celých riadkov).
    const BlockId* row(int y, int z) const { return &m_blocks[index(0, y, z)]; }

    glm::ivec2 coord() const { return m_coord; }
    int maxY() const { return m_maxY; } // najvyšší neprázdny blok (mesher nad ním nič nehľadá)
    uint32_t version() const { return m_version; }

    // Farba biómu pre trávu a listy, jedna na stĺpec.
    glm::vec3 tint(int x, int z) const { return m_tints[z * SIZE_X + x]; }
    void setTint(int x, int z, const glm::vec3& c) { m_tints[z * SIZE_X + x] = c; }

    std::unique_ptr<Mesh> mesh;       // nepriehľadné bloky (aj listy)
    std::unique_ptr<Mesh> liquidMesh; // voda a láva (kreslí sa až po teréne)
    bool dirty = true;    // mesh treba prestavať
    bool meshing = false; // mesh sa práve stavia vo vlákne
    bool modified = false; // hráč ho zmenil → treba ho uložiť
    // Ktoré strany každej sekcie sú prepojené vzduchom (z posledného meshu).
    std::array<uint16_t, CHUNK_SECTIONS> visibility;

private:
    static int index(int x, int y, int z) { return (y * SIZE_Z + z) * SIZE_X + x; }

    glm::ivec2 m_coord;
    int m_maxY = -1;
    uint32_t m_version = 0;
    std::vector<BlockId> m_blocks;
    std::array<glm::vec3, SIZE_X * SIZE_Z> m_tints;
};

} // namespace cubiq
