#include "World/Structures.h"
#include "World/Chunk.h"
#include "World/Random.h"
#include "World/TerrainGenerator.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    constexpr int VILLAGE_REGION = 320;
    constexpr int RUIN_REGION = 176;
    constexpr int TOWER_REGION = 400;
    constexpr int SEA = TerrainGenerator::SEA_LEVEL;

    int floorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }

    // ------------------------------------------------------------------ zápis do chunku

    // Zapisuje len bloky, ktoré ležia v aktuálnom chunku.
    struct Writer {
        Chunk& chunk;
        int baseX, baseZ;

        bool inside(int x, int z) const
        {
            int lx = x - baseX, lz = z - baseZ;
            return lx >= 0 && lx < Chunk::SIZE_X && lz >= 0 && lz < Chunk::SIZE_Z;
        }
        BlockId get(int x, int y, int z) const { return chunk.get(x - baseX, y, z - baseZ); }
        void set(int x, int y, int z, BlockId id)
        {
            if (!inside(x, z) || y < 1 || y >= Chunk::SIZE_Y) return;
            if (get(x, y, z) == BlockId::Bedrock) return;
            chunk.set(x - baseX, y, z - baseZ, id);
        }
        bool overlaps(int x0, int z0, int x1, int z1) const // x1, z1 vrátane
        {
            return x1 >= baseX && x0 < baseX + Chunk::SIZE_X && z1 >= baseZ && z0 < baseZ + Chunk::SIZE_Z;
        }
    };

    bool isGround(BlockId id) { return isSolid(id) && !isCutout(id); }

    // Základy: pod podlahou vyplní dieru až po pevnú zem (stavba nevisí nad svahom).
    void foundation(Writer& w, int x0, int z0, int width, int depth, int floorY, BlockId floor)
    {
        for (int z = z0; z < z0 + depth; ++z)
            for (int x = x0; x < x0 + width; ++x) {
                if (!w.inside(x, z)) continue;
                w.set(x, floorY - 1, z, floor);
                for (int y = floorY - 2; y > 0 && !isGround(w.get(x, y, z)); --y) w.set(x, y, z, BlockId::Cobblestone);
            }
    }

    void clear(Writer& w, int x0, int z0, int width, int depth, int y0, int y1)
    {
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z < z0 + depth; ++z)
                for (int x = x0; x < x0 + width; ++x) w.set(x, y, z, BlockId::Air);
    }

    // ------------------------------------------------------------------ stavby

    enum class Kind { House, Hall, Well, Lamp, Tower };

    struct Building {
        Kind kind;
        int x0, z0, w, d; // pôdorys
        int y;            // úroveň podlahy
        int door;         // strana dverí: 0 = -Z, 1 = +Z, 2 = -X, 3 = +X
        uint32_t seed;
    };

    struct Path {
        glm::ivec2 from, to; // os-rovnobežná úsečka, šírka 3
        int y;
    };

    void buildHouse(Writer& w, const Building& b, BlockId wall, BlockId roof)
    {
        const int wallH = b.kind == Kind::Hall ? 5 : 4;
        foundation(w, b.x0, b.z0, b.w, b.d, b.y, BlockId::OakPlanks);
        clear(w, b.x0 - 1, b.z0 - 1, b.w + 2, b.d + 2, b.y, b.y + wallH + 8);

        // Steny: rohy z kmeňov, spodný rad z kameňa, okná v strede stien.
        for (int y = b.y; y < b.y + wallH; ++y)
            for (int z = b.z0; z < b.z0 + b.d; ++z)
                for (int x = b.x0; x < b.x0 + b.w; ++x) {
                    bool edgeX = x == b.x0 || x == b.x0 + b.w - 1;
                    bool edgeZ = z == b.z0 || z == b.z0 + b.d - 1;
                    if (!edgeX && !edgeZ) continue;
                    BlockId id = wall;
                    if (edgeX && edgeZ) id = BlockId::OakLog;
                    else if (y == b.y) id = BlockId::Cobblestone;
                    else if (y == b.y + 2) {
                        int along = edgeZ ? x - b.x0 : z - b.z0;
                        int len = edgeZ ? b.w : b.d;
                        if (along > 0 && along < len - 1 && along % 2 == 0) id = BlockId::Glass;
                    }
                    w.set(x, y, z, id);
                }

        // Dvere uprostred steny smerom k ceste.
        int dx = b.x0 + b.w / 2, dz = b.z0 + b.d / 2;
        if (b.door == 0) dz = b.z0;
        if (b.door == 1) dz = b.z0 + b.d - 1;
        if (b.door == 2) dx = b.x0;
        if (b.door == 3) dx = b.x0 + b.w - 1;
        w.set(dx, b.y, dz, BlockId::Air);
        w.set(dx, b.y + 1, dz, BlockId::Air);

        // Valbová strecha s presahom 1 bloku.
        for (int k = 0;; ++k) {
            int x0 = b.x0 - 1 + k, x1 = b.x0 + b.w - k, z0 = b.z0 - 1 + k, z1 = b.z0 + b.d - k;
            if (x0 > x1 || z0 > z1) break;
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x) w.set(x, b.y + wallH + k, z, roof);
        }

        // Lampáš pod stropom.
        w.set(b.x0 + b.w / 2, b.y + wallH - 1, b.z0 + b.d / 2, BlockId::Lantern);
    }

    void buildWell(Writer& w, const Building& b)
    {
        foundation(w, b.x0, b.z0, b.w, b.d, b.y, BlockId::Cobblestone);
        clear(w, b.x0, b.z0, b.w, b.d, b.y, b.y + 5);
        for (int z = b.z0; z < b.z0 + 5; ++z)
            for (int x = b.x0; x < b.x0 + 5; ++x) {
                bool inner = x > b.x0 && x < b.x0 + 4 && z > b.z0 && z < b.z0 + 4;
                bool core = x == b.x0 + 2 && z == b.z0 + 2;
                bool corner = (x == b.x0 + 1 || x == b.x0 + 3) && (z == b.z0 + 1 || z == b.z0 + 3);
                if (inner && !corner) {
                    // šachta s vodou
                    for (int y = b.y - 4; y < b.y; ++y) w.set(x, y, z, BlockId::Water);
                    if (!core) w.set(x, b.y, z, BlockId::Cobblestone); // obruba
                } else if (corner) {
                    for (int y = b.y; y < b.y + 3; ++y) w.set(x, y, z, BlockId::OakLog);
                }
                w.set(x, b.y + 3, z, BlockId::SprucePlanks);
            }
        w.set(b.x0 + 2, b.y, b.z0 + 2, BlockId::Water);
        w.set(b.x0 + 2, b.y + 2, b.z0 + 2, BlockId::Lantern);
    }

    void buildLamp(Writer& w, const Building& b)
    {
        foundation(w, b.x0, b.z0, 1, 1, b.y, BlockId::Cobblestone);
        for (int y = b.y; y < b.y + 3; ++y) w.set(b.x0, y, b.z0, BlockId::OakLog);
        w.set(b.x0, b.y + 3, b.z0, BlockId::Lantern);
    }

    // Kamenná strážna veža 5×5 s točitým schodiskom vo vnútri a ochodzou s cimburím.
    void buildTower(Writer& w, const Building& b, int height)
    {
        foundation(w, b.x0, b.z0, 5, 5, b.y, BlockId::StoneBricks);
        clear(w, b.x0 - 1, b.z0 - 1, 7, 7, b.y, b.y + height + 3);

        auto brick = [&](int x, int y, int z) {
            uint32_t h = hashCoords(x, y, z, b.seed) % 10;
            return h < 2 ? BlockId::MossyStoneBricks : h < 3 ? BlockId::CrackedStoneBricks : BlockId::StoneBricks;
        };

        for (int y = b.y; y < b.y + height; ++y)
            for (int z = b.z0; z < b.z0 + 5; ++z)
                for (int x = b.x0; x < b.x0 + 5; ++x) {
                    bool wall = x == b.x0 || x == b.x0 + 4 || z == b.z0 || z == b.z0 + 4;
                    if (wall) w.set(x, y, z, brick(x, y, z));
                }

        // Točité schodisko okolo stredového stĺpa (8 stupňov na otočku).
        const glm::ivec2 ring[8] = {{1, 1}, {2, 1}, {3, 1}, {3, 2}, {3, 3}, {2, 3}, {1, 3}, {1, 2}};
        for (int y = b.y; y < b.y + height; ++y) {
            w.set(b.x0 + 2, y, b.z0 + 2, BlockId::StoneBricks);
            glm::ivec2 s = ring[(y - b.y) % 8];
            w.set(b.x0 + s.x, y, b.z0 + s.y, BlockId::StoneBricks);
        }

        // Vchod a okná (strieľne).
        w.set(b.x0 + 2, b.y, b.z0, BlockId::Air);
        w.set(b.x0 + 2, b.y + 1, b.z0, BlockId::Air);
        for (int y = b.y + 4; y < b.y + height - 2; y += 4) {
            w.set(b.x0 + 2, y, b.z0 + 4, BlockId::Air);
            w.set(b.x0, y, b.z0 + 2, BlockId::Air);
            w.set(b.x0 + 4, y, b.z0 + 2, BlockId::Air);
        }

        // Ochodza 7×7 s cimburím a lampášom.
        int top = b.y + height;
        for (int z = b.z0 - 1; z < b.z0 + 6; ++z)
            for (int x = b.x0 - 1; x < b.x0 + 6; ++x) {
                w.set(x, top, z, brick(x, top, z));
                bool edge = x == b.x0 - 1 || x == b.x0 + 5 || z == b.z0 - 1 || z == b.z0 + 5;
                if (edge && ((x + z) % 2 == 0)) w.set(x, top + 1, z, brick(x, top + 1, z));
            }
        w.set(b.x0 + 2, top, b.z0 + 2, BlockId::Air); // otvor zo schodiska
        w.set(b.x0 + 4, top + 1, b.z0 + 4, BlockId::Lantern);
    }

    void buildPath(Writer& w, const Path& p)
    {
        int x0 = std::min(p.from.x, p.to.x) - 1, x1 = std::max(p.from.x, p.to.x) + 1;
        int z0 = std::min(p.from.y, p.to.y) - 1, z1 = std::max(p.from.y, p.to.y) + 1;
        if (!w.overlaps(x0, z0, x1, z1)) return;
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x) {
                if (!w.inside(x, z)) continue;
                for (int y = std::min(p.y + 10, Chunk::SIZE_Y - 1); y > p.y - 10 && y > 0; --y) {
                    BlockId b = w.get(x, y, z);
                    if (b == BlockId::Air || isCutout(b)) continue;
                    bool natural = b == BlockId::Grass || b == BlockId::Dirt || b == BlockId::SnowyGrass ||
                                   b == BlockId::Snow || b == BlockId::Sand || b == BlockId::RedSand;
                    if (natural) w.set(x, y, z, BlockId::Gravel);
                    break;
                }
            }
    }

    // ------------------------------------------------------------------ dediny

    struct Site {
        bool valid = false;
        glm::ivec2 pos{0};
        int y = 0;
        Biome biome = Biome::Plains;
        uint32_t seed = 0;
    };

    Site villageSite(int rx, int rz, const TerrainGenerator& gen)
    {
        Site s;
        if (gen.random01(rx, rz, 101) > 0.6f) return s;
        s.pos = {rx * VILLAGE_REGION + 70 + (int)(gen.random01(rx, rz, 102) * (VILLAGE_REGION - 140)),
                 rz * VILLAGE_REGION + 70 + (int)(gen.random01(rx, rz, 103) * (VILLAGE_REGION - 140))};

        TerrainGenerator::Surface c = gen.surfaceAt(s.pos.x, s.pos.y);
        bool goodBiome = c.biome == Biome::Plains || c.biome == Biome::Forest || c.biome == Biome::BirchForest ||
                         c.biome == Biome::CherryGrove || c.biome == Biome::Desert || c.biome == Biome::Tundra;
        if (c.island || !goodBiome || c.y < SEA + 2) return s;

        // Dosť rovné miesto?
        const glm::ivec2 probes[4] = {{26, 0}, {-26, 0}, {0, 26}, {0, -26}};
        for (glm::ivec2 p : probes) {
            int h = gen.heightAt(s.pos.x + p.x, s.pos.y + p.y);
            if (std::abs(h - c.y) > 7 || h < SEA + 1) return s;
        }

        s.valid = true;
        s.y = c.y;
        s.biome = c.biome;
        s.seed = hashCoords(rx, 7, rz, gen.seed());
        return s;
    }

    bool intersects(const Building& a, int x0, int z0, int w, int d, int gap)
    {
        return a.x0 - gap < x0 + w && x0 < a.x0 + a.w + gap && a.z0 - gap < z0 + d && z0 < a.z0 + a.d + gap;
    }

    // Plán dediny: studňa v strede, 4 cesty, domy popri cestách, lampy, strážna veža.
    void villageLayout(const Site& site, const TerrainGenerator& gen, std::vector<Building>& out, std::vector<Path>& paths)
    {
        Rng rng(site.seed);
        const glm::ivec2 c = site.pos;
        out.push_back({Kind::Well, c.x - 2, c.y - 2, 5, 5, site.y + 1, 0, site.seed});

        const glm::ivec2 dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        int towerRoad = rng.range(0, 3);
        for (int di = 0; di < 4; ++di) {
            glm::ivec2 dir = dirs[di];
            glm::ivec2 perp(-dir.y, dir.x);
            int len = rng.range(20, 34);
            paths.push_back({c, c + dir * len, site.y});

            for (int t = 9; t < len - 2; t += rng.range(9, 12)) {
                glm::ivec2 along = c + dir * t;
                for (int side = -1; side <= 1; side += 2) {
                    if (rng.f() < 0.25f) continue;
                    bool hall = rng.f() < 0.2f;
                    int lenAlong = hall ? 9 : rng.range(5, 7);
                    int lenAcross = hall ? 7 : rng.range(5, 7);
                    glm::ivec2 center = along + perp * side * (3 + lenAcross / 2);
                    int w = dir.x != 0 ? lenAlong : lenAcross;
                    int d = dir.x != 0 ? lenAcross : lenAlong;
                    int x0 = center.x - w / 2, z0 = center.y - d / 2;

                    bool blocked = false;
                    for (const Building& b : out) blocked |= intersects(b, x0, z0, w, d, 2);
                    if (blocked) continue;

                    TerrainGenerator::Surface s = gen.surfaceAt(center.x, center.y);
                    if (s.island || s.y < SEA || std::abs(s.y - site.y) > 8) continue;

                    // Dvere smerom k ceste.
                    glm::ivec2 toRoad = -perp * side;
                    int door = toRoad.y < 0 ? 0 : toRoad.y > 0 ? 1 : toRoad.x < 0 ? 2 : 3;
                    out.push_back({hall ? Kind::Hall : Kind::House, x0, z0, w, d, s.y + 1, door,
                                   hashCoords(x0, 3, z0, site.seed)});
                }

                // Lampa na okraji cesty.
                if ((t / 9) % 2 == 0) {
                    glm::ivec2 l = along + perp * 2;
                    TerrainGenerator::Surface s = gen.surfaceAt(l.x, l.y);
                    out.push_back({Kind::Lamp, l.x, l.y, 1, 1, s.y + 1, 0, 0});
                }
            }

            // Strážna veža na konci jednej z ciest.
            if (di == towerRoad) {
                glm::ivec2 tc = c + dir * (len + 4);
                int x0 = tc.x - 2, z0 = tc.y - 2;
                bool blocked = false;
                for (const Building& b : out) blocked |= intersects(b, x0, z0, 5, 5, 2);
                TerrainGenerator::Surface s = gen.surfaceAt(tc.x, tc.y);
                if (!blocked && !s.island && s.y >= SEA)
                    out.push_back({Kind::Tower, x0, z0, 5, 5, s.y + 1, 0, hashCoords(x0, 9, z0, site.seed)});
            }
        }
    }

    // Materiály podľa biómu: v púšti pieskovcové tóny, v tundre smrekové drevo.
    void villageMaterials(Biome biome, BlockId& wall, BlockId& roof)
    {
        wall = BlockId::OakPlanks;
        roof = BlockId::SprucePlanks;
        if (biome == Biome::Desert) { wall = BlockId::Terracotta; roof = BlockId::TerracottaWhite; }
        if (biome == Biome::Tundra) { wall = BlockId::SprucePlanks; roof = BlockId::Cobblestone; }
        if (biome == Biome::CherryGrove) roof = BlockId::TerracottaRed;
    }

    void placeVillages(Writer& w, const TerrainGenerator& gen)
    {
        const int margin = 60;
        int rx0 = floorDiv(w.baseX - margin, VILLAGE_REGION), rx1 = floorDiv(w.baseX + Chunk::SIZE_X + margin, VILLAGE_REGION);
        int rz0 = floorDiv(w.baseZ - margin, VILLAGE_REGION), rz1 = floorDiv(w.baseZ + Chunk::SIZE_Z + margin, VILLAGE_REGION);
        for (int rz = rz0; rz <= rz1; ++rz)
            for (int rx = rx0; rx <= rx1; ++rx) {
                // Rýchly test: zasahuje oblasť dediny do chunku? (pred drahým plánovaním)
                if (gen.random01(rx, rz, 101) > 0.6f) continue;
                Site site = villageSite(rx, rz, gen);
                if (!site.valid) continue;
                if (!w.overlaps(site.pos.x - margin, site.pos.y - margin, site.pos.x + margin, site.pos.y + margin)) continue;

                std::vector<Building> buildings;
                std::vector<Path> paths;
                villageLayout(site, gen, buildings, paths);

                BlockId wall, roof;
                villageMaterials(site.biome, wall, roof);
                for (const Path& p : paths) buildPath(w, p);
                for (const Building& b : buildings) {
                    if (!w.overlaps(b.x0 - 2, b.z0 - 2, b.x0 + b.w + 2, b.z0 + b.d + 2)) continue;
                    switch (b.kind) {
                    case Kind::House:
                    case Kind::Hall: buildHouse(w, b, wall, roof); break;
                    case Kind::Well: buildWell(w, b); break;
                    case Kind::Lamp: buildLamp(w, b); break;
                    case Kind::Tower: buildTower(w, b, 13); break;
                    }
                }
            }
    }

    // ------------------------------------------------------------------ ruiny a osamelé veže

    // Rozpadnuté hradby: obvod obdĺžnika so stenami náhodnej výšky a dierami, mach a praskliny.
    void placeRuins(Writer& w, const TerrainGenerator& gen)
    {
        int rx0 = floorDiv(w.baseX - 16, RUIN_REGION), rx1 = floorDiv(w.baseX + Chunk::SIZE_X + 16, RUIN_REGION);
        int rz0 = floorDiv(w.baseZ - 16, RUIN_REGION), rz1 = floorDiv(w.baseZ + Chunk::SIZE_Z + 16, RUIN_REGION);
        for (int rz = rz0; rz <= rz1; ++rz)
            for (int rx = rx0; rx <= rx1; ++rx) {
                if (gen.random01(rx, rz, 201) > 0.35f) continue;
                int cx = rx * RUIN_REGION + 20 + (int)(gen.random01(rx, rz, 202) * (RUIN_REGION - 40));
                int cz = rz * RUIN_REGION + 20 + (int)(gen.random01(rx, rz, 203) * (RUIN_REGION - 40));
                int ww = 9 + (int)(gen.random01(rx, rz, 204) * 6), dd = 9 + (int)(gen.random01(rx, rz, 205) * 6);
                int x0 = cx - ww / 2, z0 = cz - dd / 2;
                if (!w.overlaps(x0 - 1, z0 - 1, x0 + ww, z0 + dd)) continue;

                TerrainGenerator::Surface s = gen.surfaceAt(cx, cz);
                if (s.island || s.y < SEA + 1 || s.biome == Biome::Ocean || s.biome == Biome::River ||
                    s.biome == Biome::SnowyPeaks)
                    continue;
                // Nie v dedine.
                if (!villageCentersNear(cx, cz, cx, cz, gen).empty()) continue;

                uint32_t seed = hashCoords(rx, 11, rz, gen.seed());
                int y = s.y;
                for (int z = z0; z < z0 + dd; ++z)
                    for (int x = x0; x < x0 + ww; ++x) {
                        if (!w.inside(x, z)) continue;
                        bool edge = x == x0 || x == x0 + ww - 1 || z == z0 || z == z0 + dd - 1;
                        uint32_t h = hashCoords(x, 0, z, seed);
                        // dlažba vo vnútri (miestami chýba)
                        if (!edge) {
                            if (h % 3 != 0) w.set(x, y, z, h % 5 == 0 ? BlockId::MossyCobblestone : BlockId::Cobblestone);
                            continue;
                        }
                        int wallH = (int)(h % 6) - 1; // -1..4, nula a menej = diera v hradbe
                        if (wallH <= 0) continue;
                        foundation(w, x, z, 1, 1, y + 1, BlockId::StoneBricks);
                        for (int k = 1; k <= wallH; ++k) {
                            uint32_t m = hashCoords(x, y + k, z, seed) % 10;
                            BlockId id = m < 3 ? BlockId::MossyStoneBricks : m < 5 ? BlockId::CrackedStoneBricks : BlockId::StoneBricks;
                            w.set(x, y + k, z, id);
                        }
                    }
            }
    }

    void placeLoneTowers(Writer& w, const TerrainGenerator& gen)
    {
        int rx0 = floorDiv(w.baseX - 8, TOWER_REGION), rx1 = floorDiv(w.baseX + Chunk::SIZE_X + 8, TOWER_REGION);
        int rz0 = floorDiv(w.baseZ - 8, TOWER_REGION), rz1 = floorDiv(w.baseZ + Chunk::SIZE_Z + 8, TOWER_REGION);
        for (int rz = rz0; rz <= rz1; ++rz)
            for (int rx = rx0; rx <= rx1; ++rx) {
                if (gen.random01(rx, rz, 301) > 0.4f) continue;
                int cx = rx * TOWER_REGION + 30 + (int)(gen.random01(rx, rz, 302) * (TOWER_REGION - 60));
                int cz = rz * TOWER_REGION + 30 + (int)(gen.random01(rx, rz, 303) * (TOWER_REGION - 60));
                if (!w.overlaps(cx - 4, cz - 4, cx + 4, cz + 4)) continue;

                TerrainGenerator::Surface s = gen.surfaceAt(cx, cz);
                bool good = s.biome == Biome::Mountains || s.biome == Biome::Plains || s.biome == Biome::Tundra ||
                            s.biome == Biome::Forest || s.biome == Biome::Badlands;
                if (s.island || !good || s.y < SEA + 2) continue;
                if (!villageCentersNear(cx, cz, cx, cz, gen).empty()) continue;

                Building b{Kind::Tower, cx - 2, cz - 2, 5, 5, s.y + 1, 0, hashCoords(rx, 13, rz, gen.seed())};
                buildTower(w, b, 20);
            }
    }
}

std::vector<glm::ivec2> villageCentersNear(int minX, int minZ, int maxX, int maxZ, const TerrainGenerator& gen)
{
    std::vector<glm::ivec2> result;
    int rx0 = floorDiv(minX - VILLAGE_RADIUS, VILLAGE_REGION), rx1 = floorDiv(maxX + VILLAGE_RADIUS, VILLAGE_REGION);
    int rz0 = floorDiv(minZ - VILLAGE_RADIUS, VILLAGE_REGION), rz1 = floorDiv(maxZ + VILLAGE_RADIUS, VILLAGE_REGION);
    for (int rz = rz0; rz <= rz1; ++rz)
        for (int rx = rx0; rx <= rx1; ++rx) {
            if (gen.random01(rx, rz, 101) > 0.6f) continue;
            Site s = villageSite(rx, rz, gen);
            if (!s.valid) continue;
            if (s.pos.x + VILLAGE_RADIUS < minX || s.pos.x - VILLAGE_RADIUS > maxX ||
                s.pos.y + VILLAGE_RADIUS < minZ || s.pos.y - VILLAGE_RADIUS > maxZ)
                continue;
            result.push_back(s.pos);
        }
    return result;
}

void placeStructures(Chunk& chunk, const TerrainGenerator& gen)
{
    Writer w{chunk, chunk.coord().x * Chunk::SIZE_X, chunk.coord().y * Chunk::SIZE_Z};
    placeRuins(w, gen);
    placeLoneTowers(w, gen);
    placeVillages(w, gen);
}

} // namespace cubiq
