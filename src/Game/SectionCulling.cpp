#include "Game/SectionCulling.h"
#include "Engine/Frustum.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    // Smer strany f ako posun (dx, dy, dz) – poradie ako v mesheri: +Z, -Z, +X, -X, +Y, -Y.
    const int DIR[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
}

void VisibleSections::compute(const World& world, const glm::vec3& camera, int radius, const Frustum& frustum)
{
    m_center = World::chunkCoordOf((int)std::floor(camera.x), (int)std::floor(camera.z));
    m_radius = radius;
    m_count = 0;
    const int W = 2 * radius + 1;
    m_masks.assign((size_t)W * W, 0);
    m_visited.assign((size_t)W * W * CHUNK_SECTIONS, 0);
    m_queue.clear();

    auto index = [&](int x, int y, int z) { return ((size_t)(z + radius) * W + (x + radius)) * CHUNK_SECTIONS + y; };
    const int camSection = std::clamp((int)std::floor(camera.y) / SECTION_HEIGHT, 0, CHUNK_SECTIONS - 1);

    m_queue.push_back({0, camSection, 0, -1, 0});
    m_visited[index(0, camSection, 0)] = 1;

    for (size_t head = 0; head < m_queue.size(); ++head) {
        const Node n = m_queue[head];
        m_masks[(size_t)(n.z + radius) * W + (n.x + radius)] |= (uint16_t)(1u << n.y);
        ++m_count;

        // Prepojenia strán tejto sekcie (chýbajúci chunk = priechodný).
        const Chunk* chunk = world.getChunk(m_center + glm::ivec2(n.x, n.z));
        const uint16_t links = chunk ? chunk->visibility[n.y] : ALL_CONNECTED;

        for (int d = 0; d < 6; ++d) {
            if (n.dirs & (1 << oppositeFace(d))) continue;                        // nie späť ku kamere
            if (n.entered >= 0 && !(links & facePairBit(n.entered, d))) continue; // stena v ceste

            int x = n.x + DIR[d][0], y = n.y + DIR[d][1], z = n.z + DIR[d][2];
            if (std::abs(x) > radius || std::abs(z) > radius || y < 0 || y >= CHUNK_SECTIONS) continue;
            size_t vi = index(x, y, z);
            if (m_visited[vi]) continue;

            // Sekcie mimo zorného poľa ďalej nepreskúmavame.
            glm::vec3 mn((m_center.x + x) * 16.0f, y * 16.0f, (m_center.y + z) * 16.0f);
            if (!frustum.intersects(mn, mn + glm::vec3(16.0f))) continue;

            m_visited[vi] = 1;
            m_queue.push_back({x, y, z, (int8_t)oppositeFace(d), (uint8_t)(n.dirs | (1 << d))});
        }
    }
}

uint16_t VisibleSections::mask(glm::ivec2 chunk) const
{
    glm::ivec2 d = chunk - m_center;
    if (std::abs(d.x) > m_radius || std::abs(d.y) > m_radius) return 0;
    const int W = 2 * m_radius + 1;
    return m_masks[(size_t)(d.y + m_radius) * W + (d.x + m_radius)];
}

} // namespace cubiq
