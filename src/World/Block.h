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
    TILE_COUNT
};

struct BlockType {
    const char* name;
    bool solid;     // dá sa doň naraziť (fyzika)
    bool opaque;    // úplne zakrýva steny susedov
    bool liquid;    // kreslí sa v priesvitnom pass-e
    int tileTop;
    int tileSide;
    int tileBottom;
};

const BlockType& blockType(BlockId id);

inline bool isSolid(BlockId id) { return blockType(id).solid; }
inline bool isOpaque(BlockId id) { return blockType(id).opaque; }
inline bool isLiquid(BlockId id) { return blockType(id).liquid; }

} // namespace cubiq
