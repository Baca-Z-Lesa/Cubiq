#include "World/Features.h"
#include "World/Chunk.h"
#include "World/TerrainGenerator.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    // Malý rýchly generátor náhodných čísel (xorshift) – deterministický podľa seedu.
    struct Rng {
        uint32_t s;
        explicit Rng(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
        uint32_t next()
        {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            return s;
        }
        float f() { return (next() & 0xFFFFFF) / 16777216.0f; }
        int range(int a, int b) { return a + (int)(next() % (uint32_t)(b - a + 1)); }
    };

    uint32_t mix(int x, int y, int z, uint32_t seed)
    {
        uint32_t h = seed ^ (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)(z * 83492791);
        h = (h ^ (h >> 15)) * 2246822519u;
        return h ^ (h >> 13);
    }

    // Zapisuje bloky stromu len do aktuálneho chunku.
    struct Writer {
        Chunk& chunk;
        int baseX, baseZ;

        void put(int x, int y, int z, BlockId id, bool trunk)
        {
            int lx = x - baseX, lz = z - baseZ;
            if (!Chunk::inBounds(lx, y, lz)) return;
            BlockId cur = chunk.get(lx, y, lz);
            // Kmeň prerastie listy, listy vyplnia len vzduch.
            if (cur == BlockId::Air || (trunk && isLeaves(cur))) chunk.set(lx, y, lz, id);
        }
        void log(int x, int y, int z, BlockId id) { put(x, y, z, id, true); }
        void leaf(int x, int y, int z, BlockId id) { put(x, y, z, id, false); }
    };

    // Elipsoid listov s náhodnými dierami na okraji.
    void leafBlob(Writer& w, Rng& rng, glm::vec3 c, float rx, float ry, BlockId leaves)
    {
        int x0 = (int)std::floor(c.x - rx), x1 = (int)std::ceil(c.x + rx);
        int y0 = (int)std::floor(c.y - ry), y1 = (int)std::ceil(c.y + ry);
        int z0 = (int)std::floor(c.z - rx), z1 = (int)std::ceil(c.z + rx);
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x) {
                    float dx = (x + 0.5f - c.x) / rx, dy = (y + 0.5f - c.y) / ry, dz = (z + 0.5f - c.z) / rx;
                    float d = dx * dx + dy * dy + dz * dz;
                    if (d < 0.75f || (d < 1.0f && rng.f() < 0.6f)) w.leaf(x, y, z, leaves);
                }
    }

    void line(Writer& w, glm::vec3 a, glm::vec3 b, BlockId log)
    {
        int steps = (int)std::ceil(glm::length(b - a) * 2.0f);
        for (int i = 0; i <= steps; ++i) {
            glm::vec3 p = a + (b - a) * ((float)i / std::max(1, steps));
            w.log((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z), log);
        }
    }

    // Klasický listnatý strom (dub, breza).
    void broadleaf(Writer& w, Rng& rng, int x, int y, int z, BlockId log, BlockId leaves, int minH, int maxH)
    {
        int h = rng.range(minH, maxH);
        int top = y + h - 1;
        for (int ly = top - 2; ly <= top + 1; ++ly) {
            int r = ly <= top - 1 ? 2 : 1;
            for (int dz = -r; dz <= r; ++dz)
                for (int dx = -r; dx <= r; ++dx) {
                    bool corner = std::abs(dx) == r && std::abs(dz) == r;
                    if (corner && (ly == top + 1 || rng.f() < 0.5f)) continue;
                    w.leaf(x + dx, ly, z + dz, leaves);
                }
        }
        for (int ly = y; ly <= top; ++ly) w.log(x, ly, z, log);
    }

    // Smrek: úzky kužeľ so zubatými poschodiami.
    void spruce(Writer& w, Rng& rng, int x, int y, int z)
    {
        int h = rng.range(7, 12);
        int top = y + h;
        for (int ly = y + 2; ly <= top + 1; ++ly) {
            int k = top + 1 - ly;
            int r = std::min(3, 1 + k / 3);
            if (k == 0) r = 0;
            else if (k % 2 == 1) r = std::max(1, r - 1);
            for (int dz = -r; dz <= r; ++dz)
                for (int dx = -r; dx <= r; ++dx)
                    if (std::abs(dx) + std::abs(dz) <= r + (r > 1 ? 1 : 0)) w.leaf(x + dx, ly, z + dz, BlockId::SpruceLeaves);
        }
        for (int ly = y; ly < top; ++ly) w.log(x, ly, z, BlockId::SpruceLog);
    }

    // Čerešňa: zahnutý kmeň a široká plochá ružová koruna s visiacimi lupeňmi.
    void cherry(Writer& w, Rng& rng, int x, int y, int z)
    {
        int h = rng.range(5, 7);
        int dirX = rng.range(-1, 1), dirZ = rng.range(-1, 1);
        int cx = x, cz = z;
        for (int i = 0; i < h; ++i) {
            if (i == h / 2 || i == h - 2) { cx += dirX; cz += dirZ; }
            w.log(cx, y + i, cz, BlockId::CherryLog);
        }
        glm::vec3 c(cx + 0.5f, y + h + 0.5f, cz + 0.5f);
        leafBlob(w, rng, c, 4.2f, 2.0f, BlockId::CherryLeaves);
        // visiace lupene pod okrajom koruny
        for (int i = 0; i < 14; ++i) {
            float a = rng.f() * 6.2832f, r = 2.5f + rng.f() * 1.5f;
            int hx = (int)std::floor(c.x + std::cos(a) * r), hz = (int)std::floor(c.z + std::sin(a) * r);
            int len = rng.range(1, 2);
            for (int k = 1; k <= len; ++k) w.leaf(hx, (int)c.y - 2 - k, hz, BlockId::CherryLeaves);
        }
    }

    // Pradávny obor: kmeň 2×2, korene, konáre a obrovská koruna.
    void giantTree(Writer& w, Rng& rng, int x, int y, int z)
    {
        int h = rng.range(20, 28);
        int top = y + h;
        for (int ly = y - 1; ly < top; ++ly)
            for (int dz = 0; dz < 2; ++dz)
                for (int dx = 0; dx < 2; ++dx) w.log(x + dx, ly, z + dz, BlockId::OakLog);

        glm::vec3 trunkTop(x + 1.0f, (float)top, z + 1.0f);

        // korene
        for (int i = 0; i < 6; ++i) {
            float a = rng.f() * 6.2832f;
            glm::vec3 from(x + 1.0f, y + 2.0f, z + 1.0f);
            glm::vec3 to = from + glm::vec3(std::cos(a) * 3.5f, -2.5f, std::sin(a) * 3.5f);
            line(w, from, to, BlockId::OakLog);
        }

        // konáre s menšími korunami
        int branches = rng.range(3, 5);
        for (int i = 0; i < branches; ++i) {
            float a = (i + rng.f() * 0.6f) / branches * 6.2832f;
            float startY = top - rng.range(5, 10);
            glm::vec3 from(x + 1.0f, startY, z + 1.0f);
            float len = 5.0f + rng.f() * 3.0f;
            glm::vec3 to = from + glm::vec3(std::cos(a) * len, 3.0f + rng.f() * 2.0f, std::sin(a) * len);
            line(w, from, to, BlockId::OakLog);
            leafBlob(w, rng, to + glm::vec3(0.0f, 1.0f, 0.0f), 3.6f, 2.4f, BlockId::OakLeaves);
        }

        leafBlob(w, rng, trunkTop + glm::vec3(0.0f, 1.5f, 0.0f), 7.0f, 4.0f, BlockId::OakLeaves);
    }

    void cactus(Writer& w, Rng& rng, int x, int y, int z)
    {
        int h = rng.range(2, 4);
        for (int i = 0; i < h; ++i) w.log(x, y + i, z, BlockId::Cactus);
    }

    enum class TreeKind { None, Oak, Birch, Spruce, Cherry, Giant, Cactus };

    // Aký strom (ak vôbec) rastie v tomto stĺpci. r je náhodné 0..1 pre stĺpec.
    TreeKind chooseTree(const TerrainGenerator::Surface& s, float r, float r2)
    {
        bool soil = s.block == BlockId::Grass || s.block == BlockId::SnowyGrass || s.block == BlockId::Dirt;
        if (s.y < TerrainGenerator::SEA_LEVEL) return TreeKind::None;

        if (s.island) return soil && r < 0.03f ? (r2 < 0.5f ? TreeKind::Oak : TreeKind::Cherry) : TreeKind::None;

        switch (s.biome) {
        case Biome::Forest:
            if (!soil) return TreeKind::None;
            if (r < 0.0012f) return TreeKind::Giant;
            if (r < 0.045f) return r2 < 0.25f ? TreeKind::Birch : TreeKind::Oak;
            break;
        case Biome::BirchForest:
            if (soil && r < 0.045f) return TreeKind::Birch;
            break;
        case Biome::Plains:
            if (soil && r < 0.004f) return TreeKind::Oak;
            break;
        case Biome::CherryGrove:
            if (soil && r < 0.022f) return TreeKind::Cherry;
            break;
        case Biome::Tundra:
            if (soil && r < 0.03f) return TreeKind::Spruce;
            break;
        case Biome::Mountains:
            if (soil && r < 0.012f) return TreeKind::Spruce;
            break;
        case Biome::Desert:
            if (s.block == BlockId::Sand && r < 0.005f) return TreeKind::Cactus;
            break;
        default:
            break;
        }
        return TreeKind::None;
    }

    constexpr float MAX_TREE_CHANCE = 0.045f;
    constexpr int TREE_MARGIN = 9; // najväčší strom (obor) siaha max. 9 blokov od kmeňa
}

void placeOres(Chunk& chunk, uint32_t seed)
{
    struct Ore {
        BlockId id;
        int veins, size, minY, maxY;
    };
    static const Ore ORES[] = {
        {BlockId::CoalOre, 18, 9, 5, 140},
        {BlockId::IronOre, 12, 7, 5, 72},
        {BlockId::GoldOre, 4, 6, 5, 34},
        {BlockId::AetheriteOre, 2, 4, 3, 18},
    };

    Rng rng(mix(chunk.coord().x, 77, chunk.coord().y, seed));
    for (const Ore& ore : ORES) {
        for (int v = 0; v < ore.veins; ++v) {
            int x = rng.range(0, Chunk::SIZE_X - 1), z = rng.range(0, Chunk::SIZE_Z - 1);
            int y = rng.range(ore.minY, ore.maxY);
            // Žila = náhodná prechádzka; prepíše len kameň.
            for (int i = 0; i < ore.size; ++i) {
                if (Chunk::inBounds(x, y, z) && chunk.get(x, y, z) == BlockId::Stone) chunk.set(x, y, z, ore.id);
                switch (rng.range(0, 5)) {
                case 0: ++x; break;
                case 1: --x; break;
                case 2: ++y; break;
                case 3: --y; break;
                case 4: ++z; break;
                default: --z; break;
                }
            }
        }
    }
}

void placeTrees(Chunk& chunk, const TerrainGenerator& gen)
{
    const int baseX = chunk.coord().x * Chunk::SIZE_X;
    const int baseZ = chunk.coord().y * Chunk::SIZE_Z;
    Writer w{chunk, baseX, baseZ};

    for (int z = baseZ - TREE_MARGIN; z < baseZ + Chunk::SIZE_Z + TREE_MARGIN; ++z) {
        for (int x = baseX - TREE_MARGIN; x < baseX + Chunk::SIZE_X + TREE_MARGIN; ++x) {
            float r = gen.random01(x, z, 1);
            if (r >= MAX_TREE_CHANCE) continue; // väčšina stĺpcov skončí tu, bez drahých dopytov

            TerrainGenerator::Surface s = gen.surfaceAt(x, z);
            TreeKind kind = chooseTree(s, r, gen.random01(x, z, 2));
            if (kind == TreeKind::None) continue;

            Rng rng(mix(x, s.y, z, gen.seed()));
            int y = s.y + 1;
            switch (kind) {
            case TreeKind::Oak:    broadleaf(w, rng, x, y, z, BlockId::OakLog, BlockId::OakLeaves, 4, 6); break;
            case TreeKind::Birch:  broadleaf(w, rng, x, y, z, BlockId::BirchLog, BlockId::BirchLeaves, 5, 7); break;
            case TreeKind::Spruce: spruce(w, rng, x, y, z); break;
            case TreeKind::Cherry: cherry(w, rng, x, y, z); break;
            case TreeKind::Giant:  giantTree(w, rng, x, y, z); break;
            case TreeKind::Cactus: cactus(w, rng, x, y, z); break;
            default: break;
            }
        }
    }
}

} // namespace cubiq
