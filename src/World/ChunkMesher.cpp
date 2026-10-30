#include "World/ChunkMesher.h"
#include "World/World.h"

#include <algorithm>

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
        return t.opaque || t.leaves;
    }
}

MeshInput snapshotChunk(const World& world, const Chunk& chunk)
{
    MeshInput in;
    in.coord = chunk.coord();
    in.maxY = chunk.maxY();
    in.blocks.assign(MeshInput::PX * MeshInput::PZ * Chunk::SIZE_Y, BlockId::Air);
    for (int z = 0; z < Chunk::SIZE_Z; ++z)
        for (int x = 0; x < Chunk::SIZE_X; ++x) in.tints[z * Chunk::SIZE_X + x] = chunk.tint(x, z);

    // Stačí kopírovať po maxY + 1 (vyššie nie je čo kresliť ani čo by tienilo).
    const int topY = std::min(chunk.maxY() + 1, Chunk::SIZE_Y - 1);

    for (int pz = -1; pz <= Chunk::SIZE_Z; ++pz) {
        for (int px = -1; px <= Chunk::SIZE_X; ++px) {
            // Z ktorého chunku (stred alebo jeden z 8 susedov) je tento stĺpec.
            int dx = px < 0 ? -1 : px >= Chunk::SIZE_X ? 1 : 0;
            int dz = pz < 0 ? -1 : pz >= Chunk::SIZE_Z ? 1 : 0;
            const Chunk* src = (dx == 0 && dz == 0) ? &chunk : world.getChunk(chunk.coord() + glm::ivec2(dx, dz));
            if (!src) continue;
            int lx = px - dx * Chunk::SIZE_X, lz = pz - dz * Chunk::SIZE_Z;
            for (int y = 0; y <= topY; ++y)
                in.blocks[(y * MeshInput::PZ + (pz + 1)) * MeshInput::PX + (px + 1)] = src->get(lx, y, lz);
        }
    }
    return in;
}

ChunkMeshData buildChunkMesh(const MeshInput& in, int atlasTiles)
{
    ChunkMeshData data;

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

                for (const FaceDef& face : FACES) {
                    glm::ivec3 n = glm::ivec3(x, y, z) + face.normal;
                    BlockId nb = in.at(n.x, n.y, n.z);
                    // Pevný blok: stena je vidno cez vzduch, vodu aj listy.
                    // Voda a listy: stena nie je medzi dvoma rovnakými blokmi.
                    if (isOpaque(nb) || ((liquid || type.leaves) && nb == id))
                        continue;

                    int tile = face.kind == FaceKind::Top    ? type.tileTop
                             : face.kind == FaceKind::Bottom ? type.tileBottom
                                                             : type.tileSide;
                    // Polovica texelu dovnútra, aby na hranách nepresakovala susedná dlaždica atlasu.
                    const float inset = 0.5f / (16.0f * atlasTiles);
                    float u0 = (float)tile / atlasTiles + inset;
                    float u1 = (float)(tile + 1) / atlasTiles - inset;
                    float v0 = inset * atlasTiles, v1 = 1.0f - inset * atlasTiles;

                    // Ambient occlusion: pre každý roh pozri 2 bočných susedov a 1 diagonálneho
                    // vo vrstve pred stenou. Kúty a štrbiny tak prirodzene stmavnú.
                    int axisN = face.normal.x ? 0 : face.normal.y ? 1 : 2;
                    int t1 = (axisN + 1) % 3, t2 = (axisN + 2) % 3;
                    int ao[4];
                    for (int c = 0; c < 4; ++c) {
                        if (liquid) { ao[c] = 3; continue; }
                        glm::ivec3 d1(0), d2(0);
                        d1[t1] = face.corners[c][t1] > 0.5f ? 1 : -1;
                        d2[t2] = face.corners[c][t2] > 0.5f ? 1 : -1;
                        glm::ivec3 a = n + d1, b = n + d2, cc = n + d1 + d2;
                        bool s1 = occludes(in.at(a.x, a.y, a.z));
                        bool s2 = occludes(in.at(b.x, b.y, b.z));
                        bool sc = occludes(in.at(cc.x, cc.y, cc.z));
                        ao[c] = (s1 && s2) ? 0 : 3 - (int)s1 - (int)s2 - (int)sc;
                    }

                    glm::vec3 p((float)x, (float)y, (float)z);
                    uint32_t base = (uint32_t)out.vertices.size();
                    for (int c = 0; c < 4; ++c) {
                        glm::vec3 corner = face.corners[c];
                        if (corner.y > 0.5f) corner.y = topHeight;
                        glm::vec2 uv(c == 0 || c == 3 ? u0 : u1, c < 2 ? v1 : v0);
                        out.vertices.push_back({p + corner, uv, face.shade * AO_LEVELS[ao[c]], tint});
                    }
                    // Uhlopriečku štvorca otočíme podľa AO, inak by tieň vytváral "pruhy".
                    if (ao[0] + ao[2] >= ao[1] + ao[3])
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
