#pragma once
#include "World/Block.h"

#include <cstdint>
#include <vector>

namespace cubiq {

// Atlas dlaždíc vedľa seba v jednom riadku, poradie podľa enumu Tile.
constexpr int TILE_SIZE = 16;
constexpr int ATLAS_TILES = TILE_COUNT;

std::vector<uint8_t> generateBlockAtlas();

} // namespace cubiq
