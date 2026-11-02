#pragma once
#include "Game/Item.h"
#include "World/Block.h"

#include <cstdint>
#include <vector>

namespace cubiq {

// Atlas dlaždíc vedľa seba v jednom riadku: najprv bloky (poradie podľa enumu Tile),
// za nimi ikony predmetov (nástroje, jedlo, magické predmety).
constexpr int TILE_SIZE = 16;
constexpr int ATLAS_TILES = TILE_COUNT + ITEM_ICON_COUNT;

std::vector<uint8_t> generateBlockAtlas();

} // namespace cubiq
