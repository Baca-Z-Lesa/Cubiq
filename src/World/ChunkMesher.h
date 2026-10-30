#pragma once
#include "Engine/Mesh.h"

#include <cstdint>
#include <vector>

namespace cubiq {

class Chunk;
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

// Postaví geometriu pre celý chunk (len CPU, bez OpenGL). Stenu bloku pridá len vtedy,
// keď ju nič nezakrýva – vnútorné steny sa vôbec nekreslia.
ChunkMeshData buildChunkMesh(const World& world, const Chunk& chunk, int atlasTiles);

} // namespace cubiq
