#include "World/Chunk.h"

namespace cubiq {

Chunk::Chunk(glm::ivec2 coord)
    : m_coord(coord), m_blocks(SIZE_X * SIZE_Y * SIZE_Z, BlockId::Air)
{
    m_tints.fill(glm::vec3(0.48f, 0.74f, 0.30f));
}

} // namespace cubiq
