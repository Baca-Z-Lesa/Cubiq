#include "World/Block.h"

namespace cubiq {

static const BlockType BLOCK_TYPES[(int)BlockId::Count] = {
    // name     solid  top              side             bottom
    {"air",     false, 0,               0,               0},
    {"grass",   true,  TILE_GRASS_TOP,  TILE_GRASS_SIDE, TILE_DIRT},
    {"dirt",    true,  TILE_DIRT,       TILE_DIRT,       TILE_DIRT},
    {"stone",   true,  TILE_STONE,      TILE_STONE,      TILE_STONE},
};

const BlockType& blockType(BlockId id) { return BLOCK_TYPES[(int)id]; }

} // namespace cubiq
