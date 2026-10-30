#pragma once
#include "World/Chunk.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace cubiq {

class World {
public:
    // Svetové súradnice. Mimo načítaných chunkov vracia vzduch.
    BlockId getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockId id);

    Chunk* getChunk(glm::ivec2 coord) const;
    Chunk& createChunk(glm::ivec2 coord);
    void insertChunk(std::unique_ptr<Chunk> chunk);
    void removeChunk(glm::ivec2 coord);
    size_t chunkCount() const { return m_chunks.size(); }

    // Má chunk všetkých 8 susedov? (až potom sa dá postaviť mesh bez falošných stien a tieňov na okraji)
    bool hasAllNeighbors(glm::ivec2 coord) const;

    template <typename F>
    void forEachChunk(F&& fn)
    {
        for (auto& [key, chunk] : m_chunks) fn(*chunk);
    }

    static glm::ivec2 chunkCoordOf(int x, int z);
    static int64_t key(glm::ivec2 c) { return ((int64_t)c.x << 32) | (uint32_t)c.y; }

private:
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> m_chunks;
};

} // namespace cubiq
