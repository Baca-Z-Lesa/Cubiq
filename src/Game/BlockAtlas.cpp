#include "Game/BlockAtlas.h"

#include <algorithm>

namespace cubiq {

namespace {
    // Deterministický "šum" na pixel, aby textúra nebola jednofarebná.
    uint32_t hash(int x, int y, int seed)
    {
        uint32_t h = (uint32_t)(x * 374761393 + y * 668265263 + seed * 2147483647);
        h = (h ^ (h >> 13)) * 1274126177;
        return h ^ (h >> 16);
    }

    void putPixel(std::vector<uint8_t>& img, int tile, int x, int y, int r, int g, int b, int variation, int seed,
                  int alpha = 255)
    {
        int v = (int)(hash(x, y, seed) % (2 * variation + 1)) - variation;
        int i = (y * TILE_SIZE * ATLAS_TILES + tile * TILE_SIZE + x) * 4;
        img[i + 0] = (uint8_t)std::clamp(r + v, 0, 255);
        img[i + 1] = (uint8_t)std::clamp(g + v, 0, 255);
        img[i + 2] = (uint8_t)std::clamp(b + v, 0, 255);
        img[i + 3] = (uint8_t)alpha;
    }
}

std::vector<uint8_t> generateBlockAtlas()
{
    std::vector<uint8_t> img(TILE_SIZE * ATLAS_TILES * TILE_SIZE * 4);

    for (int y = 0; y < TILE_SIZE; ++y) {
        for (int x = 0; x < TILE_SIZE; ++x) {
            putPixel(img, TILE_GRASS_TOP, x, y, 95, 159, 53, 18, 1);
            putPixel(img, TILE_DIRT, x, y, 134, 96, 67, 16, 2);
            putPixel(img, TILE_STONE, x, y, 125, 125, 125, 14, 6);
            putPixel(img, TILE_SAND, x, y, 219, 207, 163, 10, 7);
            putPixel(img, TILE_WATER, x, y, 48, 98, 196, 8, 8, 170);
            putPixel(img, TILE_SNOW, x, y, 240, 244, 250, 5, 9);
            putPixel(img, TILE_GRAVEL, x, y, 128, 122, 118, 30, 10);

            // boky: hlina so zubatým pásom trávy / snehu hore (y = 0 je horný riadok)
            int grassDepth = 3 + (int)(hash(x, 0, 3) % 3);
            if (y < grassDepth)
                putPixel(img, TILE_GRASS_SIDE, x, y, 95, 159, 53, 18, 4);
            else
                putPixel(img, TILE_GRASS_SIDE, x, y, 134, 96, 67, 16, 5);

            int snowDepth = 2 + (int)(hash(x, 0, 11) % 3);
            if (y < snowDepth)
                putPixel(img, TILE_SNOW_SIDE, x, y, 240, 244, 250, 5, 12);
            else
                putPixel(img, TILE_SNOW_SIDE, x, y, 134, 96, 67, 16, 13);
        }
    }
    return img;
}

} // namespace cubiq
