#include "World/TerrainGenerator.h"
#include "World/Chunk.h"

#include <cmath>

namespace cubiq {

static int terrainHeight(int x, int z)
{
    float h = 64.0f
            + 4.0f * std::sin(x * 0.12f) * std::cos(z * 0.09f)
            + 2.0f * std::sin((x + z) * 0.25f);
    return (int)std::floor(h);
}

void generateTerrain(Chunk& chunk)
{
    const int baseX = chunk.coord().x * Chunk::SIZE_X;
    const int baseZ = chunk.coord().y * Chunk::SIZE_Z;

    for (int z = 0; z < Chunk::SIZE_Z; ++z) {
        for (int x = 0; x < Chunk::SIZE_X; ++x) {
            int height = terrainHeight(baseX + x, baseZ + z);
            for (int y = 0; y <= height; ++y) {
                BlockId id = y == height    ? BlockId::Grass
                           : y > height - 4 ? BlockId::Dirt
                                            : BlockId::Stone;
                chunk.set(x, y, z, id);
            }
        }
    }
}

} // namespace cubiq
