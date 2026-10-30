#include "World/Chunk.h"

namespace cubiq {

Chunk::Chunk(glm::ivec2 coord)
    : m_coord(coord), m_blocks(SIZE_X * SIZE_Y * SIZE_Z, BlockId::Air)
{
}

} // namespace cubiq
