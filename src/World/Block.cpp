#include "World/Block.h"

namespace cubiq {

static const BlockType BLOCK_TYPES[(int)BlockId::Count] = {
    // name               solid  opaque liquid leaves break  top                    side                   bottom
    {"air",               false, false, false, false, false, 0,                     0,                     0},
    {"grass",             true,  true,  false, false, true,  TILE_GRASS_TOP,        TILE_GRASS_SIDE,       TILE_DIRT},
    {"dirt",              true,  true,  false, false, true,  TILE_DIRT,             TILE_DIRT,             TILE_DIRT},
    {"stone",             true,  true,  false, false, true,  TILE_STONE,            TILE_STONE,            TILE_STONE},
    {"sand",              true,  true,  false, false, true,  TILE_SAND,             TILE_SAND,             TILE_SAND},
    {"water",             false, false, true,  false, false, TILE_WATER,            TILE_WATER,            TILE_WATER},
    {"snowy_grass",       true,  true,  false, false, true,  TILE_SNOW,             TILE_SNOW_SIDE,        TILE_DIRT},
    {"snow",              true,  true,  false, false, true,  TILE_SNOW,             TILE_SNOW,             TILE_SNOW},
    {"gravel",            true,  true,  false, false, true,  TILE_GRAVEL,           TILE_GRAVEL,           TILE_GRAVEL},
    {"oak_log",           true,  true,  false, false, true,  TILE_LOG_TOP,          TILE_OAK_LOG,          TILE_LOG_TOP},
    {"oak_leaves",        true,  false, false, true,  true,  TILE_OAK_LEAVES,       TILE_OAK_LEAVES,       TILE_OAK_LEAVES},
    {"birch_log",         true,  true,  false, false, true,  TILE_LOG_TOP,          TILE_BIRCH_LOG,        TILE_LOG_TOP},
    {"birch_leaves",      true,  false, false, true,  true,  TILE_BIRCH_LEAVES,     TILE_BIRCH_LEAVES,     TILE_BIRCH_LEAVES},
    {"spruce_log",        true,  true,  false, false, true,  TILE_LOG_TOP,          TILE_SPRUCE_LOG,       TILE_LOG_TOP},
    {"spruce_leaves",     true,  false, false, true,  true,  TILE_SPRUCE_LEAVES,    TILE_SPRUCE_LEAVES,    TILE_SPRUCE_LEAVES},
    {"cherry_log",        true,  true,  false, false, true,  TILE_LOG_TOP,          TILE_CHERRY_LOG,       TILE_LOG_TOP},
    {"cherry_leaves",     true,  false, false, true,  true,  TILE_CHERRY_LEAVES,    TILE_CHERRY_LEAVES,    TILE_CHERRY_LEAVES},
    {"cactus",            true,  true,  false, false, true,  TILE_CACTUS_TOP,       TILE_CACTUS_SIDE,      TILE_CACTUS_TOP},
    {"bedrock",           true,  true,  false, false, false, TILE_BEDROCK,          TILE_BEDROCK,          TILE_BEDROCK},
    {"coal_ore",          true,  true,  false, false, true,  TILE_COAL_ORE,         TILE_COAL_ORE,         TILE_COAL_ORE},
    {"iron_ore",          true,  true,  false, false, true,  TILE_IRON_ORE,         TILE_IRON_ORE,         TILE_IRON_ORE},
    {"gold_ore",          true,  true,  false, false, true,  TILE_GOLD_ORE,         TILE_GOLD_ORE,         TILE_GOLD_ORE},
    {"aetherite_ore",     true,  true,  false, false, true,  TILE_AETHERITE_ORE,    TILE_AETHERITE_ORE,    TILE_AETHERITE_ORE},
    {"lava",              false, false, true,  false, false, TILE_LAVA,             TILE_LAVA,             TILE_LAVA},
    {"red_sand",          true,  true,  false, false, true,  TILE_RED_SAND,         TILE_RED_SAND,         TILE_RED_SAND},
    {"terracotta",        true,  true,  false, false, true,  TILE_TERRACOTTA,       TILE_TERRACOTTA,       TILE_TERRACOTTA},
    {"terracotta_red",    true,  true,  false, false, true,  TILE_TERRACOTTA_RED,   TILE_TERRACOTTA_RED,   TILE_TERRACOTTA_RED},
    {"terracotta_yellow", true,  true,  false, false, true,  TILE_TERRACOTTA_YELLOW, TILE_TERRACOTTA_YELLOW, TILE_TERRACOTTA_YELLOW},
    {"terracotta_white",  true,  true,  false, false, true,  TILE_TERRACOTTA_WHITE, TILE_TERRACOTTA_WHITE, TILE_TERRACOTTA_WHITE},
    {"terracotta_brown",  true,  true,  false, false, true,  TILE_TERRACOTTA_BROWN, TILE_TERRACOTTA_BROWN, TILE_TERRACOTTA_BROWN},
};

const BlockType& blockType(BlockId id) { return BLOCK_TYPES[(int)id]; }

} // namespace cubiq
