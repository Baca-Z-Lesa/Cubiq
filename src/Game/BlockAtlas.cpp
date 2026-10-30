#include "Game/BlockAtlas.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    // Deterministický "šum" na pixel, aby textúra nebola jednofarebná.
    uint32_t hash(int x, int y, int seed)
    {
        uint32_t h = (uint32_t)(x * 374761393 + y * 668265263 + seed * 2147483647);
        h = (h ^ (h >> 13)) * 1274126177;
        return h ^ (h >> 16);
    }

    float rand01(int x, int y, int seed) { return (hash(x, y, seed) & 0xFFFF) / 65535.0f; }

    struct Rgb { int r, g, b; };

    class Painter {
    public:
        explicit Painter(std::vector<uint8_t>& img) : m_img(img) {}

        // alpha 254 = pixel sa v shaderi tónuje farbou biómu (tráva, listy).
        void put(int tile, int x, int y, Rgb c, int variation, int seed, int alpha = 255)
        {
            int v = variation ? (int)(hash(x, y, seed) % (2 * variation + 1)) - variation : 0;
            int i = (y * TILE_SIZE * ATLAS_TILES + tile * TILE_SIZE + x) * 4;
            m_img[i + 0] = (uint8_t)std::clamp(c.r + v, 0, 255);
            m_img[i + 1] = (uint8_t)std::clamp(c.g + v, 0, 255);
            m_img[i + 2] = (uint8_t)std::clamp(c.b + v, 0, 255);
            m_img[i + 3] = (uint8_t)alpha;
        }

        template <typename F>
        void fill(int tile, F&& fn)
        {
            for (int y = 0; y < TILE_SIZE; ++y)
                for (int x = 0; x < TILE_SIZE; ++x) fn(x, y);
        }

    private:
        std::vector<uint8_t>& m_img;
    };

    constexpr int TINT = 254;
    const Rgb DIRT{134, 96, 67};
    const Rgb STONE{125, 125, 125};
}

std::vector<uint8_t> generateBlockAtlas()
{
    std::vector<uint8_t> img(TILE_SIZE * ATLAS_TILES * TILE_SIZE * 4);
    Painter p(img);

    auto stone = [&](int tile, int x, int y, int seed) {
        // kameň s tmavšími "prasklinami"
        Rgb c = rand01(x, y, seed + 99) < 0.08f ? Rgb{98, 98, 100} : STONE;
        p.put(tile, x, y, c, 12, seed);
    };

    // Ruda: kameň + niekoľko zhlukov farebných zŕn.
    auto ore = [&](int tile, Rgb color, int seed) {
        p.fill(tile, [&](int x, int y) {
            stone(tile, x, y, seed);
            for (int s = 0; s < 5; ++s) {
                float cx = 2.0f + rand01(s, 0, seed) * 12.0f, cy = 2.0f + rand01(s, 1, seed) * 12.0f;
                float d = std::hypot(x - cx, y - cy);
                if (d < 1.4f) p.put(tile, x, y, color, 14, seed + 1);
                else if (d < 1.9f && rand01(x, y, seed + 2) < 0.5f) p.put(tile, x, y, {color.r * 3 / 4, color.g * 3 / 4, color.b * 3 / 4}, 8, seed + 3);
            }
        });
    };

    auto logSide = [&](int tile, Rgb c, int seed) {
        p.fill(tile, [&](int x, int y) {
            int stripe = (x % 4 == 0 || (x + (int)(hash(x, 0, seed) % 3)) % 7 == 0) ? -22 : 0;
            p.put(tile, x, y, {c.r + stripe, c.g + stripe, c.b + stripe}, 8, seed);
        });
    };

    // Listy: dierky (alpha 0), voliteľne tónované.
    auto leaves = [&](int tile, Rgb c, int variation, float holes, bool tint, int seed) {
        p.fill(tile, [&](int x, int y) {
            if (rand01(x, y, seed + 7) < holes) { p.put(tile, x, y, c, 0, seed, 0); return; }
            float light = rand01(x, y, seed + 9) < 0.15f ? 1.18f : 1.0f;
            p.put(tile, x, y, {(int)(c.r * light), (int)(c.g * light), (int)(c.b * light)}, variation, seed, tint ? TINT : 255);
        });
    };

    p.fill(TILE_GRASS_TOP, [&](int x, int y) { p.put(TILE_GRASS_TOP, x, y, {190, 190, 190}, 26, 1, TINT); });
    p.fill(TILE_GRASS_SIDE, [&](int x, int y) {
        int depth = 3 + (int)(hash(x, 0, 3) % 3);
        if (y < depth) p.put(TILE_GRASS_SIDE, x, y, {185, 185, 185}, 22, 4, TINT);
        else p.put(TILE_GRASS_SIDE, x, y, DIRT, 16, 5);
    });
    p.fill(TILE_DIRT, [&](int x, int y) { p.put(TILE_DIRT, x, y, DIRT, 16, 2); });
    p.fill(TILE_STONE, [&](int x, int y) { stone(TILE_STONE, x, y, 6); });
    p.fill(TILE_SAND, [&](int x, int y) { p.put(TILE_SAND, x, y, {222, 210, 165}, 10, 7); });
    p.fill(TILE_WATER, [&](int x, int y) { p.put(TILE_WATER, x, y, {44, 104, 200}, 8, 8, 165); });
    p.fill(TILE_SNOW, [&](int x, int y) { p.put(TILE_SNOW, x, y, {240, 244, 250}, 5, 9); });
    p.fill(TILE_SNOW_SIDE, [&](int x, int y) {
        int depth = 2 + (int)(hash(x, 0, 11) % 3);
        if (y < depth) p.put(TILE_SNOW_SIDE, x, y, {240, 244, 250}, 5, 12);
        else p.put(TILE_SNOW_SIDE, x, y, DIRT, 16, 13);
    });
    p.fill(TILE_GRAVEL, [&](int x, int y) { p.put(TILE_GRAVEL, x, y, {128, 122, 118}, 30, 10); });

    logSide(TILE_OAK_LOG, {104, 78, 46}, 20);
    p.fill(TILE_LOG_TOP, [&](int x, int y) {
        float d = std::hypot(x - 7.5f, y - 7.5f);
        Rgb c = d > 6.6f ? Rgb{96, 72, 42} : ((int)d % 2 ? Rgb{168, 134, 84} : Rgb{140, 108, 64});
        p.put(TILE_LOG_TOP, x, y, c, 6, 21);
    });
    leaves(TILE_OAK_LEAVES, {150, 150, 150}, 34, 0.18f, true, 22);

    p.fill(TILE_BIRCH_LOG, [&](int x, int y) {
        bool mark = rand01(x / 3, y, 23) < 0.13f && (x % 3 != 2);
        p.put(TILE_BIRCH_LOG, x, y, mark ? Rgb{45, 42, 40} : Rgb{222, 220, 210}, 8, 24);
    });
    leaves(TILE_BIRCH_LEAVES, {118, 170, 76}, 22, 0.18f, false, 25);

    logSide(TILE_SPRUCE_LOG, {72, 52, 34}, 26);
    leaves(TILE_SPRUCE_LEAVES, {48, 92, 64}, 16, 0.14f, false, 27);

    logSide(TILE_CHERRY_LOG, {76, 44, 50}, 28);
    p.fill(TILE_CHERRY_LEAVES, [&](int x, int y) {
        if (rand01(x, y, 30) < 0.16f) { p.put(TILE_CHERRY_LEAVES, x, y, {0, 0, 0}, 0, 29, 0); return; }
        Rgb c = rand01(x, y, 31) < 0.2f ? Rgb{255, 214, 230} : Rgb{238, 160, 196};
        p.put(TILE_CHERRY_LEAVES, x, y, c, 14, 29);
    });

    p.fill(TILE_CACTUS_SIDE, [&](int x, int y) {
        Rgb c = (x % 5 == 0) ? Rgb{58, 112, 46} : Rgb{86, 148, 64};
        if (x % 5 == 2 && rand01(x, y, 32) < 0.25f) c = {214, 226, 170}; // tŕne
        p.put(TILE_CACTUS_SIDE, x, y, c, 8, 33);
    });
    p.fill(TILE_CACTUS_TOP, [&](int x, int y) {
        float d = std::hypot(x - 7.5f, y - 7.5f);
        p.put(TILE_CACTUS_TOP, x, y, d > 6.5f ? Rgb{58, 112, 46} : Rgb{104, 164, 78}, 8, 34);
    });

    p.fill(TILE_BEDROCK, [&](int x, int y) { p.put(TILE_BEDROCK, x, y, {62, 62, 64}, 36, 35); });

    ore(TILE_COAL_ORE, {34, 34, 36}, 40);
    ore(TILE_IRON_ORE, {216, 172, 142}, 41);
    ore(TILE_GOLD_ORE, {252, 214, 62}, 42);
    ore(TILE_AETHERITE_ORE, {70, 236, 244}, 43);

    p.fill(TILE_LAVA, [&](int x, int y) {
        float t = 0.5f + 0.5f * std::sin(x * 0.9f + std::sin(y * 0.7f) * 2.0f);
        Rgb c{(int)(214 + 40 * t), (int)(80 + 90 * t), (int)(18 + 30 * t)};
        p.put(TILE_LAVA, x, y, c, 10, 44);
    });

    p.fill(TILE_RED_SAND, [&](int x, int y) { p.put(TILE_RED_SAND, x, y, {192, 102, 48}, 12, 45); });
    p.fill(TILE_TERRACOTTA, [&](int x, int y) { p.put(TILE_TERRACOTTA, x, y, {164, 86, 40}, 6, 46); });
    p.fill(TILE_TERRACOTTA_RED, [&](int x, int y) { p.put(TILE_TERRACOTTA_RED, x, y, {146, 62, 46}, 6, 47); });
    p.fill(TILE_TERRACOTTA_YELLOW, [&](int x, int y) { p.put(TILE_TERRACOTTA_YELLOW, x, y, {190, 136, 36}, 6, 48); });
    p.fill(TILE_TERRACOTTA_WHITE, [&](int x, int y) { p.put(TILE_TERRACOTTA_WHITE, x, y, {212, 180, 162}, 6, 49); });
    p.fill(TILE_TERRACOTTA_BROWN, [&](int x, int y) { p.put(TILE_TERRACOTTA_BROWN, x, y, {80, 52, 36}, 6, 50); });

    return img;
}

} // namespace cubiq
