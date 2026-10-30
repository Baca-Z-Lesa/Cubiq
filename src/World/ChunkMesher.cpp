#include "World/ChunkMesher.h"
#include "World/World.h"

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
}

ChunkMeshData buildChunkMesh(const World& world, const Chunk& chunk, int atlasTiles)
{
    ChunkMeshData data;

    const int baseX = chunk.coord().x * Chunk::SIZE_X;
    const int baseZ = chunk.coord().y * Chunk::SIZE_Z;

    // Sused: vo vnútri chunku priamo, na okraji cez svet (susedný chunk).
    auto neighbor = [&](int x, int y, int z) {
        if (Chunk::inBounds(x, y, z)) return chunk.get(x, y, z);
        return world.getBlock(baseX + x, y, baseZ + z);
    };

    for (int y = 0; y <= chunk.maxY(); ++y) {
        for (int z = 0; z < Chunk::SIZE_Z; ++z) {
            for (int x = 0; x < Chunk::SIZE_X; ++x) {
                BlockId id = chunk.get(x, y, z);
                if (id == BlockId::Air) continue;
                const BlockType& type = blockType(id);
                const bool liquid = type.liquid;
                MeshData& out = liquid ? data.liquid : data.opaque;

                // Hladina je o kúsok nižšie ako plný blok (len keď nad vodou nie je ďalšia voda).
                const bool surface = liquid && neighbor(x, y + 1, z) != id;
                const float topHeight = surface ? 0.875f : 1.0f;

                for (const FaceDef& face : FACES) {
                    int ny = y + face.normal.y;
                    if (ny < 0) continue; // spodok sveta nikto nevidí

                    BlockId n = neighbor(x + face.normal.x, ny, z + face.normal.z);
                    // Pevný blok: stena je vidno cez vzduch aj vodu.
                    // Voda: stena len do vzduchu (medzi dvoma vodnými blokmi nič).
                    if (isOpaque(n) || (liquid && n == id))
                        continue;

                    int tile = face.kind == FaceKind::Top    ? type.tileTop
                             : face.kind == FaceKind::Bottom ? type.tileBottom
                                                             : type.tileSide;
                    // Polovica texelu dovnútra, aby na hranách nepresakovala susedná dlaždica atlasu.
                    const float inset = 0.5f / (16.0f * atlasTiles);
                    float u0 = (float)tile / atlasTiles + inset;
                    float u1 = (float)(tile + 1) / atlasTiles - inset;
                    glm::vec3 p((float)x, (float)y, (float)z);

                    uint32_t base = (uint32_t)out.vertices.size();
                    for (int c = 0; c < 4; ++c) {
                        glm::vec3 corner = face.corners[c];
                        if (corner.y > 0.5f) corner.y = topHeight;
                        glm::vec2 uv(c == 0 || c == 3 ? u0 : u1, c < 2 ? 1.0f - inset * atlasTiles : inset * atlasTiles);
                        out.vertices.push_back({p + corner, uv, face.shade});
                    }
                    out.indices.insert(out.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
                }
            }
        }
    }

    return data;
}

} // namespace cubiq
