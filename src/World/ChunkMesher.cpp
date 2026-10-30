#include "World/ChunkMesher.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    enum class FaceKind { Top, Side, Bottom };

    struct FaceDef {
        glm::ivec3 normal;
        glm::vec3 corners[4]; // proti smeru hod. ručičiek pri pohľade zvonku
        FaceKind kind;
        float shade;
    };

    const FaceDef FACES[6] = {
        {{ 0, 0, 1}, {{0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}}, FaceKind::Side,   0.8f},
        {{ 0, 0,-1}, {{1,0,0}, {0,0,0}, {0,1,0}, {1,1,0}}, FaceKind::Side,   0.8f},
        {{ 1, 0, 0}, {{1,0,1}, {1,0,0}, {1,1,0}, {1,1,1}}, FaceKind::Side,   0.6f},
        {{-1, 0, 0}, {{0,0,0}, {0,0,1}, {0,1,1}, {0,1,0}}, FaceKind::Side,   0.6f},
        {{ 0, 1, 0}, {{0,1,1}, {1,1,1}, {1,1,0}, {0,1,0}}, FaceKind::Top,    1.0f},
        {{ 0,-1, 0}, {{0,0,0}, {1,0,0}, {1,0,1}, {0,0,1}}, FaceKind::Bottom, 0.5f},
    };

    // Jas rohu podľa počtu zakrývajúcich susedov (0 = najtmavší kút).
    const float AO_LEVELS[4] = {0.45f, 0.64f, 0.82f, 1.0f};

    bool occludes(BlockId id)
    {
        const BlockType& t = blockType(id);
        return t.opaque || (t.cutout && id != BlockId::Glass); // listy tienia, sklo nie
    }

    // Úroveň svetla 0–15 → jas. Exponenciálne ako ľudské oko; v úplnej tme ostane kúsok viditeľnosti.
    struct LightCurve {
        float v[16];
        LightCurve()
        {
            for (int i = 0; i < 16; ++i) v[i] = 0.05f + 0.95f * std::pow(0.8f, 15.0f - i);
        }
    };
    const LightCurve CURVE;

    // ------------------------------------------------------------------ svetlo

    // Úrovne svetla 0–15 pre každú bunku snapshotu: slnko + farebné svetlo zo zdrojov.
    struct LightGrid {
        std::vector<uint8_t> sky, r, g, b;
    };

    // Koľko svetla blok pohltí pri prechode (okrem nepriehľadných, tie ho zastavia úplne).
    int absorption(BlockId id)
    {
        const BlockType& t = blockType(id);
        if (t.liquid) return 2;
        if (t.cutout && id != BlockId::Glass) return 1; // listy
        return 0;
    }

    // Flood fill (BFS) svetla v celom snapshote.
    LightGrid computeLight(const MeshInput& in)
    {
        constexpr int PX = MeshInput::PX, PZ = MeshInput::PZ;
        const int H = in.height;
        const size_t N = (size_t)PX * PZ * H;
        LightGrid L;
        L.sky.assign(N, 0);
        L.r.assign(N, 0);
        L.g.assign(N, 0);
        L.b.assign(N, 0);

        const int strideZ = PX, strideY = PX * PZ;
        std::vector<uint32_t> queue;
        queue.reserve(N / 8);

        auto opaqueAt = [&](size_t i) { return isOpaque(in.blocks[i]); };

        // 1) Slnko: zhora rovno nadol bez straty, kým nenarazí na nepriehľadný blok.
        for (int z = 0; z < PZ; ++z)
            for (int x = 0; x < PX; ++x) {
                int level = 15;
                for (int y = H - 1; y >= 0; --y) {
                    size_t i = (size_t)y * strideY + z * strideZ + x;
                    if (opaqueAt(i)) { level = 0; continue; }
                    level = std::max(0, level - absorption(in.blocks[i]));
                    L.sky[i] = (uint8_t)level;
                }
            }

        // Štartovacie bunky pre šírenie do strán: tie, ktoré majú tmavšieho bočného suseda
        // (okraj tieňa pod previsom, vchod do jaskyne...).
        for (int y = 0; y < H; ++y)
            for (int z = 1; z < PZ - 1; ++z)
                for (int x = 1; x < PX - 1; ++x) {
                    size_t i = (size_t)y * strideY + z * strideZ + x;
                    int s = L.sky[i];
                    if (s < 2) continue;
                    if (L.sky[i - 1] < s - 1 || L.sky[i + 1] < s - 1 || L.sky[i - strideZ] < s - 1 || L.sky[i + strideZ] < s - 1)
                        queue.push_back((uint32_t)i);
                }

        auto neighbors = [&](uint32_t i, auto&& visit) {
            int x = (int)(i % PX), z = (int)((i / PX) % PZ), y = (int)(i / strideY);
            if (x > 0) visit(i - 1);
            if (x < PX - 1) visit(i + 1);
            if (z > 0) visit(i - strideZ);
            if (z < PZ - 1) visit(i + strideZ);
            if (y > 0) visit(i - strideY);
            if (y < H - 1) visit(i + strideY);
        };

        for (size_t head = 0; head < queue.size(); ++head) {
            uint32_t i = queue[head];
            int level = L.sky[i];
            neighbors(i, [&](uint32_t n) {
                if (opaqueAt(n)) return;
                int nl = level - 1 - absorption(in.blocks[n]);
                if (nl > L.sky[n]) {
                    L.sky[n] = (uint8_t)nl;
                    queue.push_back(n);
                }
            });
        }

        // 2) Farebné svetlo zo zdrojov (lampáš, láva, aetherit) – R, G, B sa šíria naraz.
        queue.clear();
        for (size_t i = 0; i < N; ++i) {
            const BlockType& t = blockType(in.blocks[i]);
            if (t.lightR | t.lightG | t.lightB) {
                L.r[i] = t.lightR;
                L.g[i] = t.lightG;
                L.b[i] = t.lightB;
                queue.push_back((uint32_t)i);
            }
        }
        for (size_t head = 0; head < queue.size(); ++head) {
            uint32_t i = queue[head];
            int r = L.r[i], g = L.g[i], b = L.b[i];
            neighbors(i, [&](uint32_t n) {
                if (opaqueAt(n)) return;
                int a = 1 + absorption(in.blocks[n]);
                int nr = r - a, ng = g - a, nb = b - a;
                bool brighter = false;
                if (nr > L.r[n]) { L.r[n] = (uint8_t)nr; brighter = true; }
                if (ng > L.g[n]) { L.g[n] = (uint8_t)ng; brighter = true; }
                if (nb > L.b[n]) { L.b[n] = (uint8_t)nb; brighter = true; }
                if (brighter) queue.push_back(n);
            });
        }
        return L;
    }
}

MeshInput snapshotChunk(const World& world, const Chunk& chunk)
{
    constexpr int M = MeshInput::MARGIN;
    MeshInput in;
    in.coord = chunk.coord();
    in.maxY = chunk.maxY();
    for (int z = 0; z < Chunk::SIZE_Z; ++z)
        for (int x = 0; x < Chunk::SIZE_X; ++x) in.tints[z * Chunk::SIZE_X + x] = chunk.tint(x, z);

    // 3×3 chunky okolo (stred + 8 susedov); chýbajúci = vzduch.
    const Chunk* n[3][3];
    int top = chunk.maxY();
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
            const Chunk* c = (dx == 0 && dz == 0) ? &chunk : world.getChunk(chunk.coord() + glm::ivec2(dx, dz));
            n[dz + 1][dx + 1] = c;
            if (c) top = std::max(top, c->maxY());
        }
    // O vrstvu viac, aby nad najvyšším blokom bolo plné slnko.
    in.height = std::min(top + 2, Chunk::SIZE_Y);
    in.blocks.assign((size_t)MeshInput::PX * MeshInput::PZ * in.height, BlockId::Air);

    // Kopírujeme po riadkoch: MARGIN blokov zľava, 16 zo stredu, MARGIN sprava.
    for (int y = 0; y < in.height; ++y) {
        for (int pz = -M; pz < Chunk::SIZE_Z + M; ++pz) {
            int dz = pz < 0 ? -1 : pz >= Chunk::SIZE_Z ? 1 : 0;
            int lz = pz - dz * Chunk::SIZE_Z;
            BlockId* dst = &in.blocks[MeshInput::index(-M, y, pz)];
            const Chunk* left = n[dz + 1][0];
            const Chunk* mid = n[dz + 1][1];
            const Chunk* right = n[dz + 1][2];
            if (left) std::copy_n(left->row(y, lz) + Chunk::SIZE_X - M, M, dst);
            if (mid) std::copy_n(mid->row(y, lz), Chunk::SIZE_X, dst + M);
            if (right) std::copy_n(right->row(y, lz), M, dst + M + Chunk::SIZE_X);
        }
    }
    return in;
}

ChunkMeshData buildChunkMesh(const MeshInput& in, int atlasTiles)
{
    ChunkMeshData data;
    const LightGrid light = computeLight(in);

    // Svetlo bunky ako (slnko, R, G, B) v rozsahu 0–15; nad snapshotom je plné slnko.
    auto lightAt = [&](const glm::ivec3& p) -> glm::ivec4 {
        if (p.y >= in.height) return {15, 0, 0, 0};
        if (p.y < 0) return {0, 0, 0, 0};
        int i = MeshInput::index(p.x, p.y, p.z);
        return {light.sky[i], light.r[i], light.g[i], light.b[i]};
    };

    for (int y = 0; y <= in.maxY; ++y) {
        for (int z = 0; z < Chunk::SIZE_Z; ++z) {
            for (int x = 0; x < Chunk::SIZE_X; ++x) {
                BlockId id = in.at(x, y, z);
                if (id == BlockId::Air) continue;
                const BlockType& type = blockType(id);
                const bool liquid = type.liquid;
                MeshData& out = liquid ? data.liquid : data.opaque;

                // Hladina je o kúsok nižšie ako plný blok (len keď nad ňou nie je ďalšia tekutina).
                const bool surface = liquid && in.at(x, y + 1, z) != id;
                const float topHeight = surface ? 0.875f : 1.0f;
                const glm::vec3 tint = in.tints[z * Chunk::SIZE_X + x];
                // Svietiace bloky majú steny osvetlené aspoň vlastným svetlom.
                const glm::vec3 selfGlow(CURVE.v[type.lightR], CURVE.v[type.lightG], CURVE.v[type.lightB]);
                const bool glows = type.lightR | type.lightG | type.lightB;

                for (const FaceDef& face : FACES) {
                    glm::ivec3 n = glm::ivec3(x, y, z) + face.normal;
                    BlockId nb = in.at(n.x, n.y, n.z);
                    // Pevný blok: stena je vidno cez vzduch, vodu aj listy.
                    // Voda a listy: stena nie je medzi dvoma rovnakými blokmi.
                    if (isOpaque(nb) || ((liquid || type.cutout) && nb == id))
                        continue;

                    int tile = face.kind == FaceKind::Top    ? type.tileTop
                             : face.kind == FaceKind::Bottom ? type.tileBottom
                                                             : type.tileSide;
                    // Polovica texelu dovnútra, aby na hranách nepresakovala susedná dlaždica atlasu.
                    const float inset = 0.5f / (16.0f * atlasTiles);
                    float u0 = (float)tile / atlasTiles + inset;
                    float u1 = (float)(tile + 1) / atlasTiles - inset;
                    float v0 = inset * atlasTiles, v1 = 1.0f - inset * atlasTiles;

                    // Pre každý roh: 2 boční susedia a 1 diagonálny vo vrstve pred stenou.
                    //  - ambient occlusion: kúty a štrbiny stmavnú
                    //  - plynulé svetlo: priemer svetla z buniek, ktoré nie sú plné
                    int axisN = face.normal.x ? 0 : face.normal.y ? 1 : 2;
                    int t1 = (axisN + 1) % 3, t2 = (axisN + 2) % 3;
                    int ao[4];
                    glm::vec4 cornerLight[4];
                    const glm::ivec4 own = lightAt(n);
                    for (int c = 0; c < 4; ++c) {
                        glm::ivec3 d1(0), d2(0);
                        d1[t1] = face.corners[c][t1] > 0.5f ? 1 : -1;
                        d2[t2] = face.corners[c][t2] > 0.5f ? 1 : -1;
                        glm::ivec3 a = n + d1, b = n + d2, cc = n + d1 + d2;
                        BlockId ba = in.at(a.x, a.y, a.z), bb = in.at(b.x, b.y, b.z), bc = in.at(cc.x, cc.y, cc.z);
                        bool s1 = occludes(ba), s2 = occludes(bb), sc = occludes(bc);
                        ao[c] = liquid ? 3 : (s1 && s2) ? 0 : 3 - (int)s1 - (int)s2 - (int)sc;

                        glm::vec4 sum(own);
                        int count = 1;
                        if (!isOpaque(ba)) { sum += glm::vec4(lightAt(a)); ++count; }
                        if (!isOpaque(bb)) { sum += glm::vec4(lightAt(b)); ++count; }
                        if (!isOpaque(bc) && !(isOpaque(ba) && isOpaque(bb))) { sum += glm::vec4(lightAt(cc)); ++count; }
                        glm::vec4 avg = sum / (float)count;
                        auto curve = [](float level) {
                            int lo = (int)level;
                            float t = level - lo;
                            return CURVE.v[lo] * (1.0f - t) + CURVE.v[std::min(lo + 1, 15)] * t;
                        };
                        cornerLight[c] = {curve(avg.x), curve(avg.y), curve(avg.z), curve(avg.w)};
                        if (glows) {
                            cornerLight[c].y = std::max(cornerLight[c].y, selfGlow.r);
                            cornerLight[c].z = std::max(cornerLight[c].z, selfGlow.g);
                            cornerLight[c].w = std::max(cornerLight[c].w, selfGlow.b);
                        }
                    }

                    glm::vec3 p((float)x, (float)y, (float)z);
                    uint32_t base = (uint32_t)out.vertices.size();
                    for (int c = 0; c < 4; ++c) {
                        glm::vec3 corner = face.corners[c];
                        if (corner.y > 0.5f) corner.y = topHeight;
                        glm::vec2 uv(c == 0 || c == 3 ? u0 : u1, c < 2 ? v1 : v0);
                        out.vertices.push_back({p + corner, uv, face.shade * AO_LEVELS[ao[c]], tint, cornerLight[c]});
                    }
                    // Uhlopriečku štvorca otočíme podľa jasu rohov, inak by tieň vytváral "pruhy".
                    auto bright = [&](int c) { return AO_LEVELS[ao[c]] * std::max(cornerLight[c].x, cornerLight[c].y); };
                    if (bright(0) + bright(2) >= bright(1) + bright(3))
                        out.indices.insert(out.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
                    else
                        out.indices.insert(out.indices.end(), {base + 1, base + 2, base + 3, base + 1, base + 3, base});
                }
            }
        }
    }

    return data;
}

} // namespace cubiq
