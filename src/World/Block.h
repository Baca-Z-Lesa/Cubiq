#pragma once
#include <cstdint>

namespace cubiq {

enum class BlockId : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Count
};

// Index dlaždice v texture atlase.
enum Tile : int {
    TILE_GRASS_TOP = 0,
    TILE_GRASS_SIDE,
    TILE_DIRT,
    TILE_STONE,
    TILE_COUNT
};

struct BlockType {
    const char* name;
    bool solid;     // zakrýva steny susedov
    int tileTop;
    int tileSide;
    int tileBottom;
};

const BlockType& blockType(BlockId id);

inline bool isSolid(BlockId id) { return blockType(id).solid; }

} // namespace cubiq
