#pragma once
#include "Engine/Mesh.h"
#include "World/Chunk.h"

#include <array>
#include <cstdint>
#include <vector>

namespace cubiq {

class World;

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

// Nepriehľadné bloky a priesvitné tekutiny idú do samostatných meshov,
// lebo voda sa kreslí až po celom teréne s blendingom.
struct ChunkMeshData {
    MeshData opaque;
    MeshData liquid;
};

// Kópia chunku + 1 blok okraja zo všetkých 8 susedov.
// Mesh sa z nej dá postaviť v inom vlákne bez toho, aby sa čítal živý svet.
struct MeshInput {
    static constexpr int PX = Chunk::SIZE_X + 2;
    static constexpr int PZ = Chunk::SIZE_Z + 2;

    glm::ivec2 coord{0};
    int maxY = -1;
    std::vector<BlockId> blocks; // PX * PZ * SIZE_Y
    std::array<glm::vec3, Chunk::SIZE_X * Chunk::SIZE_Z> tints;

    // x, z v rozsahu -1..16
    BlockId at(int x, int y, int z) const
    {
        if (y < 0) return BlockId::Bedrock; // pod svetom je "plné", spodné steny sa nekreslia
        if (y >= Chunk::SIZE_Y) return BlockId::Air;
        return blocks[(y * PZ + (z + 1)) * PX + (x + 1)];
    }
};

// Volá sa na hlavnom vlákne (číta svet).
MeshInput snapshotChunk(const World& world, const Chunk& chunk);

// Dá sa volať z ľubovoľného vlákna. Stenu bloku pridá len vtedy, keď ju nič nezakrýva.
ChunkMeshData buildChunkMesh(const MeshInput& input, int atlasTiles);

} // namespace cubiq
