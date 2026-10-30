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

MeshData buildChunkMesh(const World& world, const Chunk& chunk, int atlasTiles)
{
    MeshData data;
    auto& vertices = data.vertices;
    auto& indices = data.indices;

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

                for (const FaceDef& face : FACES) {
                    if (isSolid(neighbor(x + face.normal.x, y + face.normal.y, z + face.normal.z)))
                        continue;

                    int tile = face.kind == FaceKind::Top    ? type.tileTop
                             : face.kind == FaceKind::Bottom ? type.tileBottom
                                                             : type.tileSide;
                    float u0 = (float)tile / atlasTiles;
                    float u1 = (float)(tile + 1) / atlasTiles;
                    glm::vec3 p((float)x, (float)y, (float)z);

                    uint32_t base = (uint32_t)vertices.size();
                    vertices.push_back({p + face.corners[0], {u0, 1.0f}, face.shade});
                    vertices.push_back({p + face.corners[1], {u1, 1.0f}, face.shade});
                    vertices.push_back({p + face.corners[2], {u1, 0.0f}, face.shade});
                    vertices.push_back({p + face.corners[3], {u0, 0.0f}, face.shade});
                    indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
                }
            }
        }
    }

    return data;
}

} // namespace cubiq
