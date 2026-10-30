#pragma once

namespace cubiq {

class Chunk;

// Zatiaľ jednoduché kopce zo sínusov. Vo fáze 5 ich nahradí Perlin noise.
void generateTerrain(Chunk& chunk);

} // namespace cubiq
