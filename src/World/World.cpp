#include "World/World.h"

namespace cubiq {

namespace {
    // Delenie zaokrúhlené nadol aj pre záporné čísla (-1 / 16 = -1, nie 0).
    int floorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
    int floorMod(int a, int b) { return a - floorDiv(a, b) * b; }
}

glm::ivec2 World::chunkCoordOf(int x, int z)
{
    return {floorDiv(x, Chunk::SIZE_X), floorDiv(z, Chunk::SIZE_Z)};
}

Chunk* World::getChunk(glm::ivec2 coord) const
{
    auto it = m_chunks.find(key(coord));
    return it != m_chunks.end() ? it->second.get() : nullptr;
}

Chunk& World::createChunk(glm::ivec2 coord)
{
    auto& slot = m_chunks[key(coord)];
    if (!slot) slot = std::make_unique<Chunk>(coord);
    return *slot;
}

void World::insertChunk(std::unique_ptr<Chunk> chunk)
{
    m_chunks[key(chunk->coord())] = std::move(chunk);
}

void World::removeChunk(glm::ivec2 coord) { m_chunks.erase(key(coord)); }

bool World::hasAllNeighbors(glm::ivec2 coord) const
{
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if ((dx || dz) && !getChunk(coord + glm::ivec2(dx, dz))) return false;
    return true;
}

BlockId World::getBlock(int x, int y, int z) const
{
    if (y < 0 || y >= Chunk::SIZE_Y) return BlockId::Air;
    Chunk* chunk = getChunk(chunkCoordOf(x, z));
    if (!chunk) return BlockId::Air;
    return chunk->get(floorMod(x, Chunk::SIZE_X), y, floorMod(z, Chunk::SIZE_Z));
}

void World::setBlock(int x, int y, int z, BlockId id)
{
    if (y < 0 || y >= Chunk::SIZE_Y) return;
    glm::ivec2 cc = chunkCoordOf(x, z);
    Chunk* chunk = getChunk(cc);
    if (!chunk) return;

    int lx = floorMod(x, Chunk::SIZE_X), lz = floorMod(z, Chunk::SIZE_Z);
    chunk->set(lx, y, lz, id);

    // Blok na okraji mení aj viditeľné steny a tiene (AO) suseda – v rohu aj diagonálneho.
    auto markDirty = [&](int dx, int dz) {
        if (Chunk* n = getChunk(cc + glm::ivec2(dx, dz))) n->dirty = true;
    };
    int ex = lx == 0 ? -1 : lx == Chunk::SIZE_X - 1 ? 1 : 0;
    int ez = lz == 0 ? -1 : lz == Chunk::SIZE_Z - 1 ? 1 : 0;
    if (ex) markDirty(ex, 0);
    if (ez) markDirty(0, ez);
    if (ex && ez) markDirty(ex, ez);
}

} // namespace cubiq
