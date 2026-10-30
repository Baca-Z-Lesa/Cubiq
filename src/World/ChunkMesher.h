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

// Postaví geometriu pre celý chunk (len CPU, bez OpenGL). Stenu bloku pridá len vtedy,
// keď je za ňou nepevný blok (vzduch) – vnútorné steny sa vôbec nekreslia.
MeshData buildChunkMesh(const World& world, const Chunk& chunk, int atlasTiles);

} // namespace cubiq
