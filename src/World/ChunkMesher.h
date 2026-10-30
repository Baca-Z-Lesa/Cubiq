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

// Kópia chunku + okraj MARGIN blokov zo susedov (svetlo dosiahne max. 15 blokov ďaleko).
// Mesh aj svetlo sa z nej dajú spočítať v inom vlákne bez čítania živého sveta.
struct MeshInput {
    static constexpr int MARGIN = 15;
    static constexpr int PX = Chunk::SIZE_X + 2 * MARGIN;
    static constexpr int PZ = Chunk::SIZE_Z + 2 * MARGIN;

    glm::ivec2 coord{0};
    int maxY = -1;   // najvyšší blok stredového chunku
    int height = 0;  // počet uložených vrstiev Y (nad nimi je len vzduch a plné slnko)
    std::vector<BlockId> blocks; // PX * PZ * height
    std::array<glm::vec3, Chunk::SIZE_X * Chunk::SIZE_Z> tints;

    static int index(int x, int y, int z) { return (y * PZ + (z + MARGIN)) * PX + (x + MARGIN); }

    // x, z v rozsahu -MARGIN .. SIZE + MARGIN - 1
    BlockId at(int x, int y, int z) const
    {
        if (y < 0) return BlockId::Bedrock; // pod svetom je "plné", spodné steny sa nekreslia
        if (y >= height) return BlockId::Air;
        return blocks[index(x, y, z)];
    }
};

// Volá sa na hlavnom vlákne (číta svet).
MeshInput snapshotChunk(const World& world, const Chunk& chunk);

// Dá sa volať z ľubovoľného vlákna: spočíta svetlo a postaví mesh.
ChunkMeshData buildChunkMesh(const MeshInput& input, int atlasTiles);

} // namespace cubiq
