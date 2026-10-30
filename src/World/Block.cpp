#include "World/Block.h"

namespace cubiq {

static const BlockType BLOCK_TYPES[(int)BlockId::Count] = {
    // name          solid  opaque liquid top              side             bottom
    {"air",          false, false, false, 0,               0,               0},
    {"grass",        true,  true,  false, TILE_GRASS_TOP,  TILE_GRASS_SIDE, TILE_DIRT},
    {"dirt",         true,  true,  false, TILE_DIRT,       TILE_DIRT,       TILE_DIRT},
    {"stone",        true,  true,  false, TILE_STONE,      TILE_STONE,      TILE_STONE},
    {"sand",         true,  true,  false, TILE_SAND,       TILE_SAND,       TILE_SAND},
    {"water",        false, false, true,  TILE_WATER,      TILE_WATER,      TILE_WATER},
    {"snowy_grass",  true,  true,  false, TILE_SNOW,       TILE_SNOW_SIDE,  TILE_DIRT},
    {"snow",         true,  true,  false, TILE_SNOW,       TILE_SNOW,       TILE_SNOW},
    {"gravel",       true,  true,  false, TILE_GRAVEL,     TILE_GRAVEL,     TILE_GRAVEL},
};

const BlockType& blockType(BlockId id) { return BLOCK_TYPES[(int)id]; }

} // namespace cubiq
