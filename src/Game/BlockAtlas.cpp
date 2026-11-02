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

    // Dosky: 4 vodorovné dosky so škárami a posunutými spojmi.
    auto planks = [&](int tile, Rgb c, int seed) {
        p.fill(tile, [&](int x, int y) {
            int board = y / 4;
            bool seam = y % 4 == 3 || (x == (board * 5 + 3) % 16);
            Rgb k = seam ? Rgb{c.r * 3 / 4, c.g * 3 / 4, c.b * 3 / 4} : c;
            p.put(tile, x, y, k, 7, seed);
        });
    };
    planks(TILE_OAK_PLANKS, {172, 138, 86}, 51);
    planks(TILE_SPRUCE_PLANKS, {112, 82, 50}, 52);

    // Kamenná dlažba: nepravidelné "kamene" s tmavými škárami.
    auto cobble = [&](int tile, bool mossy, int seed) {
        p.fill(tile, [&](int x, int y) {
            int cx = (x + (y / 5 % 2) * 3) / 5, cy = y / 5;
            bool seam = (x + (y / 5 % 2) * 3) % 5 == 0 || y % 5 == 0;
            int shade = (int)(hash(cx, cy, seed) % 30);
            Rgb c = seam ? Rgb{78, 78, 80} : Rgb{118 + shade, 118 + shade, 120 + shade};
            if (mossy && rand01(x / 2, y / 2, seed + 1) < 0.35f) c = {74, 112, 52};
            p.put(tile, x, y, c, 8, seed + 2);
        });
    };
    cobble(TILE_COBBLESTONE, false, 53);
    cobble(TILE_MOSSY_COBBLESTONE, true, 54);

    // Kamenné tehly: 2 rady tehál na dlaždicu, posunuté o pol tehly.
    auto bricks = [&](int tile, int variant, int seed) {
        p.fill(tile, [&](int x, int y) {
            int row = y / 8;
            int ox = (x + row * 4) % 8;
            bool seam = y % 8 == 0 || ox == 0;
            Rgb c = seam ? Rgb{84, 84, 86} : Rgb{136, 136, 138};
            if (variant == 1 && !seam && rand01(x / 2, y / 2, seed) < 0.3f) c = {78, 116, 56};      // mach
            if (variant == 2 && !seam && rand01(x, y, seed) < 0.12f) c = {70, 70, 72};              // praskliny
            p.put(tile, x, y, c, 6, seed + 1);
        });
    };
    bricks(TILE_STONE_BRICKS, 0, 55);
    bricks(TILE_MOSSY_STONE_BRICKS, 1, 56);
    bricks(TILE_CRACKED_STONE_BRICKS, 2, 57);

    // Sklo: priehľadné s rámom a odleskom.
    p.fill(TILE_GLASS, [&](int x, int y) {
        bool frame = x == 0 || y == 0 || x == 15 || y == 15;
        bool glint = (x + y == 5 || x + y == 6 || x + y == 9) && x > 1 && y > 1 && x < 10;
        if (frame) p.put(TILE_GLASS, x, y, {200, 222, 228}, 6, 58);
        else if (glint) p.put(TILE_GLASS, x, y, {236, 248, 252}, 0, 58);
        else p.put(TILE_GLASS, x, y, {0, 0, 0}, 0, 58, 0);
    });

    // Lampáš: tmavý kovový rám a žiariace teplé jadro.
    p.fill(TILE_LANTERN, [&](int x, int y) {
        bool frame = x <= 1 || x >= 14 || y <= 1 || y >= 14 || x == 7 || x == 8;
        float d = std::hypot(x - 7.5f, y - 7.5f);
        Rgb glow{255, (int)(236 - d * 9), (int)(150 - d * 12)};
        p.put(TILE_LANTERN, x, y, frame ? Rgb{58, 52, 48} : glow, frame ? 6 : 4, 59);
    });

    // --- Pracovné bloky ---
    p.fill(TILE_TABLE_TOP, [&](int x, int y) {
        bool grid = x == 0 || y == 0 || x == 15 || y == 15 || x == 5 || x == 10 || y == 5 || y == 10;
        p.put(TILE_TABLE_TOP, x, y, grid ? Rgb{96, 68, 38} : Rgb{176, 140, 88}, 7, 60);
    });
    p.fill(TILE_TABLE_SIDE, [&](int x, int y) {
        Rgb c = y < 3 ? Rgb{150, 110, 62} : (y % 4 == 3 ? Rgb{120, 92, 56} : Rgb{168, 134, 84});
        if (y >= 5 && y <= 11 && (x == 3 || x == 12)) c = {80, 80, 84};          // náradie na boku
        if (y == 5 && x >= 2 && x <= 4) c = {150, 150, 156};
        p.put(TILE_TABLE_SIDE, x, y, c, 6, 61);
    });
    p.fill(TILE_FURNACE_FRONT, [&](int x, int y) {
        int cx = (x + (y / 5 % 2) * 3) / 5, cy = y / 5;
        bool seam = (x + (y / 5 % 2) * 3) % 5 == 0 || y % 5 == 0;
        int shade = (int)(hash(cx, cy, 62) % 30);
        Rgb c = seam ? Rgb{78, 78, 80} : Rgb{118 + shade, 118 + shade, 120 + shade};
        if (x >= 4 && x <= 11 && y >= 8 && y <= 13) c = (y >= 11) ? Rgb{255, 150, 40} : Rgb{30, 26, 24}; // otvor so žeravými uhlíkmi
        p.put(TILE_FURNACE_FRONT, x, y, c, 6, 63);
    });
    p.fill(TILE_ALTAR_TOP, [&](int x, int y) {
        float d = std::hypot(x - 7.5f, y - 7.5f);
        Rgb c = (x % 8 == 0 || y % 8 == 0) ? Rgb{84, 84, 90} : Rgb{122, 120, 132};
        if (d > 4.5f && d < 5.8f) c = {80, 220, 240};                                  // žiariaci runový kruh
        if (d < 1.6f) c = {200, 250, 255};
        p.put(TILE_ALTAR_TOP, x, y, c, 5, 64);
    });
    p.fill(TILE_ALTAR_SIDE, [&](int x, int y) {
        int row = y / 8, ox = (x + row * 4) % 8;
        Rgb c = (y % 8 == 0 || ox == 0) ? Rgb{78, 76, 86} : Rgb{120, 118, 130};
        if ((x == 4 || x == 11) && y > 2 && y < 13 && (y % 3 != 0)) c = {70, 210, 235}; // runy
        p.put(TILE_ALTAR_SIDE, x, y, c, 5, 65);
    });
    p.fill(TILE_SKY_CRYSTAL_ORE, [&](int x, int y) {
        Rgb c{196, 204, 214};
        for (int s = 0; s < 4; ++s) {
            float cx = 3.0f + rand01(s, 0, 66) * 10.0f, cy = 3.0f + rand01(s, 1, 66) * 10.0f;
            if (std::abs(x - cx) + std::abs(y - cy) * 0.6f < 1.8f) c = {150, 245, 255};
        }
        p.put(TILE_SKY_CRYSTAL_ORE, x, y, c, 8, 67);
    });

    // --- Ikony predmetov (priehľadné pozadie) ---
    auto icon = [&](Item item) { return itemIconTile(item); };
    auto px = [&](int tile, int x, int y, Rgb c) {
        if (x >= 0 && y >= 0 && x < TILE_SIZE && y < TILE_SIZE) p.put(tile, x, y, c, 0, 0);
    };
    auto disc = [&](int tile, float cx, float cy, float r, Rgb c, Rgb edge) {
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy);
                if (d < r) px(tile, x, y, d > r - 1.0f ? edge : c);
            }
    };
    auto stamp = [&](int tile, float x, float y, Rgb c) { px(tile, (int)std::floor(x), (int)std::floor(y), c); };

    // Nástroje: rukoväť šikmo zľava dole doprava hore, hlava podľa druhu, farba podľa materiálu.
    const float dX = 0.7071f, dY = -0.7071f;   // smer rukoväte
    const float nX = 0.7071f, nY = 0.7071f;    // kolmica
    auto handle = [&](int tile, float from, float to, Rgb c) {
        for (float s = from; s <= to; s += 0.25f) stamp(tile, 3.0f + dX * s, 12.5f + dY * s, c);
    };
    auto toolIcon = [&](Item item, ToolType type, Rgb mat) {
        int t = icon(item);
        Rgb dark{mat.r * 3 / 5, mat.g * 3 / 5, mat.b * 3 / 5};
        const float topX = 3.0f + dX * 12.0f, topY = 12.5f + dY * 12.0f;
        switch (type) {
        case ToolType::Pickaxe:
            handle(t, 0.0f, 11.5f, {120, 86, 48});
            for (float u = -5.5f; u <= 5.5f; u += 0.2f) {
                float bend = u * u * 0.09f;
                for (float w = 0.0f; w <= 1.2f; w += 0.4f)
                    stamp(t, topX + nX * u - dX * (bend + w), topY + nY * u - dY * (bend + w), w > 0.8f ? dark : mat);
            }
            break;
        case ToolType::Axe:
            handle(t, 0.0f, 11.5f, {120, 86, 48});
            for (float s = -3.0f; s <= 1.0f; s += 0.25f)
                for (float u = 0.0f; u <= 4.6f - std::abs(s + 1.0f) * 0.6f; u += 0.25f)
                    stamp(t, topX + dX * s - nX * u, topY + dY * s - nY * u, u > 3.6f ? dark : mat);
            break;
        case ToolType::Shovel:
            handle(t, 0.0f, 8.5f, {120, 86, 48});
            for (float s = 8.0f; s <= 12.5f; s += 0.25f)
                for (float u = -1.8f; u <= 1.8f; u += 0.25f)
                    stamp(t, 3.0f + dX * s + nX * u, 12.5f + dY * s + nY * u, std::abs(u) > 1.4f ? dark : mat);
            break;
        default: // meč
            handle(t, 0.0f, 3.0f, {90, 64, 36});
            for (float u = -2.8f; u <= 2.8f; u += 0.25f) stamp(t, 3.0f + dX * 3.2f + nX * u, 12.5f + dY * 3.2f + nY * u, dark);
            for (float s = 3.5f; s <= 13.0f; s += 0.25f)
                for (float u = -0.9f; u <= 0.9f; u += 0.3f)
                    stamp(t, 3.0f + dX * s + nX * u, 12.5f + dY * s + nY * u, u > 0.3f ? dark : mat);
            break;
        }
    };
    const Rgb WOOD{166, 128, 76}, STONEC{136, 136, 140}, IRON{220, 220, 226}, AETH{90, 236, 246};
    toolIcon(Item::WoodPickaxe, ToolType::Pickaxe, WOOD);   toolIcon(Item::WoodAxe, ToolType::Axe, WOOD);
    toolIcon(Item::WoodShovel, ToolType::Shovel, WOOD);     toolIcon(Item::WoodSword, ToolType::Sword, WOOD);
    toolIcon(Item::StonePickaxe, ToolType::Pickaxe, STONEC); toolIcon(Item::StoneAxe, ToolType::Axe, STONEC);
    toolIcon(Item::StoneShovel, ToolType::Shovel, STONEC);   toolIcon(Item::StoneSword, ToolType::Sword, STONEC);
    toolIcon(Item::IronPickaxe, ToolType::Pickaxe, IRON);   toolIcon(Item::IronAxe, ToolType::Axe, IRON);
    toolIcon(Item::IronShovel, ToolType::Shovel, IRON);     toolIcon(Item::IronSword, ToolType::Sword, IRON);
    toolIcon(Item::AetheritePickaxe, ToolType::Pickaxe, AETH); toolIcon(Item::AetheriteSword, ToolType::Sword, AETH);

    handle(icon(Item::Stick), 1.0f, 12.0f, {150, 112, 64});

    // Uhlie: nepravidelná hrudka
    disc(icon(Item::Coal), 8.0f, 8.5f, 5.0f, {44, 44, 48}, {24, 24, 26});
    px(icon(Item::Coal), 6, 6, {90, 90, 96});
    px(icon(Item::Coal), 9, 7, {80, 80, 86});

    // Ingoty: lichobežníková tehlička
    auto ingot = [&](Item item, Rgb c) {
        int t = icon(item);
        for (int y = 6; y <= 11; ++y)
            for (int x = 2 + (11 - y) / 2; x <= 13 - (11 - y) / 2; ++x)
                px(t, x, y, y == 6 ? Rgb{std::min(255, c.r + 40), std::min(255, c.g + 40), std::min(255, c.b + 40)}
                            : (y == 11 ? Rgb{c.r * 2 / 3, c.g * 2 / 3, c.b * 2 / 3} : c));
    };
    ingot(Item::IronIngot, {200, 200, 206});
    ingot(Item::GoldIngot, {246, 200, 60});

    // Kryštály: kosoštvorec so žiarivým stredom
    auto crystal = [&](Item item, Rgb c, Rgb light, float size) {
        int t = icon(item);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                float d = std::abs(x + 0.5f - 8.0f) / (size * 0.6f) + std::abs(y + 0.5f - 8.0f) / size;
                if (d < 1.0f) px(t, x, y, d < 0.45f ? light : (x < 8 ? c : Rgb{c.r * 4 / 5, c.g * 4 / 5, c.b * 4 / 5}));
            }
    };
    crystal(Item::AetheriteShard, {60, 200, 230}, {200, 255, 255}, 6.5f);
    crystal(Item::SkyCrystal, {170, 230, 255}, {255, 255, 255}, 7.5f);
    crystal(Item::ManaCrystal, {70, 110, 240}, {170, 200, 255}, 6.5f);

    // Tieňová esencia: tmavofialová guľa s azúrovou iskrou
    disc(icon(Item::ShadowEssence), 8.0f, 8.0f, 5.5f, {70, 30, 100}, {30, 10, 50});
    disc(icon(Item::ShadowEssence), 7.0f, 7.0f, 2.0f, {120, 70, 170}, {120, 70, 170});
    px(icon(Item::ShadowEssence), 9, 9, {100, 240, 255});

    // Mäso a jablko
    auto meat = [&](Item item, Rgb c, Rgb edge) {
        int t = icon(item);
        disc(t, 7.0f, 8.5f, 5.0f, c, edge);
        for (int i = 0; i < 4; ++i) px(t, 11 + i / 2, 12 + i % 2, {236, 230, 216}); // kosť
    };
    meat(Item::RawMeat, {226, 110, 110}, {170, 60, 70});
    meat(Item::CookedMeat, {156, 96, 52}, {100, 60, 30});
    disc(icon(Item::Apple), 8.0f, 9.0f, 5.0f, {220, 40, 40}, {150, 20, 25});
    px(icon(Item::Apple), 6, 7, {255, 140, 140});
    px(icon(Item::Apple), 8, 3, {110, 70, 30});
    px(icon(Item::Apple), 8, 4, {110, 70, 30});
    px(icon(Item::Apple), 9, 3, {60, 160, 50});
    px(icon(Item::Apple), 10, 3, {60, 160, 50});

    // Čarovná palička a nebeská palica: rukoväť + zlatý krúžok + žiariaci kryštál
    auto staff = [&](Item item, Rgb wood, Rgb gem, float gemSize) {
        int t = icon(item);
        handle(t, 0.0f, 9.5f, wood);
        for (float u = -1.0f; u <= 1.0f; u += 0.5f) stamp(t, 3.0f + dX * 8.5f + nX * u, 12.5f + dY * 8.5f + nY * u, {250, 205, 70});
        disc(t, 3.0f + dX * 11.5f, 12.5f + dY * 11.5f, gemSize, gem, {255, 255, 255});
    };
    staff(Item::Wand, {140, 100, 60}, {80, 230, 245}, 2.6f);
    staff(Item::SkyStaff, {236, 230, 214}, {190, 240, 255}, 3.4f);

    // Runy: kamenná doštička s farebným znakom
    auto rune = [&](Item item, Rgb glyph, int kind) {
        int t = icon(item);
        for (int y = 2; y <= 13; ++y)
            for (int x = 3; x <= 12; ++x) {
                bool edge = x == 3 || x == 12 || y == 2 || y == 13;
                px(t, x, y, edge ? Rgb{86, 86, 94} : Rgb{136, 136, 146});
            }
        switch (kind) {
        case 0: // svetlo: slnko
            disc(t, 8.0f, 8.0f, 2.5f, glyph, glyph);
            for (int i = 0; i < 4; ++i) { px(t, 8, 3 + i * 0 + (i == 0 ? 1 : 0), glyph); }
            px(t, 8, 4, glyph); px(t, 8, 12, glyph); px(t, 4, 8, glyph); px(t, 11, 8, glyph);
            break;
        case 1: // oheň: plameň
            for (int y = 4; y <= 11; ++y)
                for (int x = 8 - (y - 4) / 3; x <= 8 + (y - 4) / 3; ++x) px(t, x, y, y > 8 ? Rgb{255, 220, 80} : glyph);
            break;
        case 2: // skok: šípka
            for (int x = 5; x <= 10; ++x) px(t, x, 8, glyph);
            px(t, 9, 6, glyph); px(t, 10, 7, glyph); px(t, 10, 9, glyph); px(t, 9, 10, glyph);
            break;
        default: // levitácia: šípka hore
            for (int y = 5; y <= 11; ++y) px(t, 8, y, glyph);
            px(t, 6, 7, glyph); px(t, 7, 6, glyph); px(t, 9, 6, glyph); px(t, 10, 7, glyph);
            break;
        }
    };
    rune(Item::RuneLight, {255, 236, 120}, 0);
    rune(Item::RuneFire, {255, 110, 40}, 1);
    rune(Item::RuneBlink, {190, 110, 255}, 2);
    rune(Item::RuneLevitation, {220, 250, 255}, 3);

    return img;
}

} // namespace cubiq
