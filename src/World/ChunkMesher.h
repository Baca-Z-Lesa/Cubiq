#pragma once
#include "Engine/Mesh.h"

#include <memory>

namespace cubiq {

class Chunk;
class World;

// Postaví jeden mesh pre celý chunk. Stenu bloku pridá len vtedy,
// keď je za ňou nepevný blok (vzduch) – vnútorné steny sa vôbec nekreslia.
std::unique_ptr<Mesh> buildChunkMesh(const World& world, const Chunk& chunk, int atlasTiles);

} // namespace cubiq
