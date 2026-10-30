#pragma once
#include <cstdint>

namespace cubiq {

enum class BlockId : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Water,
    SnowyGrass,
    Snow,
    Gravel,
    OakLog,
    OakLeaves,
    BirchLog,
    BirchLeaves,
    SpruceLog,
    SpruceLeaves,
    CherryLog,
    CherryLeaves,
    Cactus,
    Bedrock,
    CoalOre,
    IronOre,
    GoldOre,
    AetheriteOre, // vlastná ruda Cubiqu – žiariace azúrové kryštály hlboko pod zemou
    Lava,
    RedSand,
    Terracotta,
    TerracottaRed,
    TerracottaYellow,
    TerracottaWhite,
    TerracottaBrown,
    OakPlanks,
    SprucePlanks,
    Cobblestone,
    MossyCobblestone,
    StoneBricks,
    MossyStoneBricks,
    CrackedStoneBricks,
    Glass,
    Lantern,
    Count
};

// Index dlaždice v texture atlase.
enum Tile : int {
    TILE_GRASS_TOP = 0,
    TILE_GRASS_SIDE,
    TILE_DIRT,
    TILE_STONE,
    TILE_SAND,
    TILE_WATER,
    TILE_SNOW,
    TILE_SNOW_SIDE,
    TILE_GRAVEL,
    TILE_OAK_LOG,
    TILE_LOG_TOP,
    TILE_OAK_LEAVES,
    TILE_BIRCH_LOG,
    TILE_BIRCH_LEAVES,
    TILE_SPRUCE_LOG,
    TILE_SPRUCE_LEAVES,
    TILE_CHERRY_LOG,
    TILE_CHERRY_LEAVES,
    TILE_CACTUS_SIDE,
    TILE_CACTUS_TOP,
    TILE_BEDROCK,
    TILE_COAL_ORE,
    TILE_IRON_ORE,
    TILE_GOLD_ORE,
    TILE_AETHERITE_ORE,
    TILE_LAVA,
    TILE_RED_SAND,
    TILE_TERRACOTTA,
    TILE_TERRACOTTA_RED,
    TILE_TERRACOTTA_YELLOW,
    TILE_TERRACOTTA_WHITE,
    TILE_TERRACOTTA_BROWN,
    TILE_OAK_PLANKS,
    TILE_SPRUCE_PLANKS,
    TILE_COBBLESTONE,
    TILE_MOSSY_COBBLESTONE,
    TILE_STONE_BRICKS,
    TILE_MOSSY_STONE_BRICKS,
    TILE_CRACKED_STONE_BRICKS,
    TILE_GLASS,
    TILE_LANTERN,
    TILE_COUNT
};

struct BlockType {
    const char* name;
    bool solid;      // dá sa doň naraziť (fyzika)
    bool opaque;     // úplne zakrýva steny susedov
    bool liquid;     // kreslí sa v priesvitnom pass-e
    bool cutout;     // listy, sklo: dierky v textúre, nezakrývajú susedov
    bool breakable;
    int tileTop;
    int tileSide;
    int tileBottom;
    // Farba vyžarovaného svetla 0–15 na kanál (0,0,0 = nesvieti).
    uint8_t lightR = 0, lightG = 0, lightB = 0;
};

const BlockType& blockType(BlockId id);

inline bool isSolid(BlockId id) { return blockType(id).solid; }
inline bool isOpaque(BlockId id) { return blockType(id).opaque; }
inline bool isLiquid(BlockId id) { return blockType(id).liquid; }
inline bool isCutout(BlockId id) { return blockType(id).cutout; }
inline bool emitsLight(BlockId id)
{
    const BlockType& t = blockType(id);
    return t.lightR | t.lightG | t.lightB;
}

} // namespace cubiq
