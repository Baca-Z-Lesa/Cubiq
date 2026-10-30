#pragma once
#include "Engine/Mesh.h"
#include "World/Block.h"

#include <glm/vec2.hpp>
#include <memory>
#include <vector>

namespace cubiq {

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
        dirty = true;
    }

    glm::ivec2 coord() const { return m_coord; }

    std::unique_ptr<Mesh> mesh;
    bool dirty = true; // mesh treba prestavať

private:
    static int index(int x, int y, int z) { return (y * SIZE_Z + z) * SIZE_X + x; }

    glm::ivec2 m_coord;
    std::vector<BlockId> m_blocks;
};

} // namespace cubiq
