#include "Game/FarTerrain.h"
#include "Game/Sky.h"
#include "World/TerrainGenerator.h"

#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <cstddef>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace cubiq {

namespace {
    int floorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }

    // Priemerná farba povrchu z diaľky (zodpovedá priemeru textúr blokov).
    glm::vec3 surfaceColor(const TerrainGenerator::Column& c)
    {
        const int sea = TerrainGenerator::SEA_LEVEL;
        const glm::vec3 WATER(0.17f, 0.34f, 0.60f), SAND(0.84f, 0.80f, 0.62f), SNOW(0.90f, 0.93f, 0.97f);
        const glm::vec3 STONE(0.47f, 0.47f, 0.49f), MESA(0.70f, 0.40f, 0.22f);

        if (c.height < sea) return WATER;
        if (c.height >= c.snowLine) return SNOW;
        switch (c.biome) {
        case Biome::Ocean:
        case Biome::River: return WATER;
        case Biome::Beach:
        case Biome::Desert: return SAND;
        case Biome::Badlands: return MESA;
        case Biome::Tundra: return SNOW * 0.92f;
        case Biome::SnowyPeaks: return SNOW;
        case Biome::Mountains:
            if (c.height > 128) return STONE;
            return c.cold ? SNOW * 0.9f : c.tint * 0.72f;
        case Biome::Forest:
        case Biome::BirchForest: return c.tint * 0.55f; // koruny stromov sú tmavšie ako tráva
        case Biome::CherryGrove: return glm::mix(c.tint * 0.7f, glm::vec3(0.93f, 0.62f, 0.76f), 0.45f);
        default: return c.tint * 0.72f;
        }
    }
}

FarTerrain::FarTerrain(const TerrainGenerator& generator)
    : m_generator(generator), m_shader("assets/shaders/far.vert", "assets/shaders/far.frag")
{
    m_worker = std::thread(&FarTerrain::workerLoop, this);
#ifdef _WIN32
    SetThreadPriority((HANDLE)m_worker.native_handle(), THREAD_PRIORITY_LOWEST);
#endif
}

FarTerrain::~FarTerrain()
{
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    m_worker.join();
    for (auto& [k, t] : m_tiles) {
        glDeleteBuffers(1, &t.vbo);
        glDeleteVertexArrays(1, &t.vao);
    }
}

FarTerrain::TileData FarTerrain::buildTile(glm::ivec2 coord) const
{
    constexpr int N = TILE / STEP + 1; // vrcholy na stranu
    const int baseX = coord.x * TILE, baseZ = coord.y * TILE;
    const int sea = TerrainGenerator::SEA_LEVEL;

    // Výšky s okrajom 1 (kvôli normálam) a farby.
    float height[N + 2][N + 2];
    glm::vec3 color[N][N];
    for (int j = -1; j <= N; ++j)
        for (int i = -1; i <= N; ++i) {
            TerrainGenerator::Column c = m_generator.columnAt(baseX + i * STEP, baseZ + j * STEP);
            height[j + 1][i + 1] = (float)std::max(c.height, sea) + 1.0f; // vrch bloku; voda = hladina
            if (i >= 0 && j >= 0 && i < N && j < N) color[j][i] = surfaceColor(c);
        }

    auto vertex = [&](int i, int j) {
        float h = height[j + 1][i + 1];
        glm::vec3 n = glm::normalize(glm::vec3(height[j + 1][i] - height[j + 1][i + 2], 2.0f * STEP,
                                               height[j][i + 1] - height[j + 2][i + 1]));
        return Vertex{glm::vec3(baseX + i * STEP, h, baseZ + j * STEP), color[j][i], n};
    };

    TileData t;
    t.coord = coord;
    t.vertices.reserve((N - 1) * (N - 1) * 6);
    for (int j = 0; j < N - 1; ++j)
        for (int i = 0; i < N - 1; ++i) {
            Vertex a = vertex(i, j), b = vertex(i + 1, j), c = vertex(i + 1, j + 1), d = vertex(i, j + 1);
            // Proti smeru hodinových ručičiek pri pohľade zhora.
            t.vertices.insert(t.vertices.end(), {a, d, c, a, c, b});
        }
    return t;
}

void FarTerrain::workerLoop()
{
    for (;;) {
        glm::ivec2 coord;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [&] { return m_stop || !m_queue.empty(); });
            if (m_stop) return;
            coord = m_queue.front();
            m_queue.pop_front();
        }
        TileData data = buildTile(coord);
        std::lock_guard lock(m_mutex);
        m_done.push_back(std::move(data));
    }
}

void FarTerrain::update(const glm::vec3& cameraPos, float farRadius)
{
    const glm::ivec2 center(floorDiv((int)std::floor(cameraPos.x), TILE), floorDiv((int)std::floor(cameraPos.z), TILE));
    const int r = (int)std::ceil(farRadius / TILE) + 1;

    // Hotové dlaždice → GPU (max. pár za snímku).
    std::vector<TileData> done;
    {
        std::lock_guard lock(m_mutex);
        size_t n = std::min<size_t>(4, m_done.size());
        for (size_t i = 0; i < n; ++i) done.push_back(std::move(m_done[i]));
        m_done.erase(m_done.begin(), m_done.begin() + n);
        for (auto& d : done) m_pending.erase(key(d.coord));
    }
    for (TileData& d : done) {
        glm::ivec2 dc = d.coord - center;
        if (dc.x * dc.x + dc.y * dc.y > (r + 1) * (r + 1)) continue; // medzitým odišla z dosahu
        Tile t;
        t.vertexCount = (int)d.vertices.size();
        glGenVertexArrays(1, &t.vao);
        glGenBuffers(1, &t.vbo);
        glBindVertexArray(t.vao);
        glBindBuffer(GL_ARRAY_BUFFER, t.vbo);
        glBufferData(GL_ARRAY_BUFFER, d.vertices.size() * sizeof(Vertex), d.vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
        m_tiles[key(d.coord)] = t;
    }

    if (center == m_lastCenter) return;
    m_lastCenter = center;

    // Uvoľniť vzdialené
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        glm::ivec2 c((int)(it->first >> 32), (int)(int32_t)(it->first & 0xFFFFFFFF));
        glm::ivec2 d = c - center;
        if (d.x * d.x + d.y * d.y > (r + 2) * (r + 2)) {
            glDeleteBuffers(1, &it->second.vbo);
            glDeleteVertexArrays(1, &it->second.vao);
            it = m_tiles.erase(it);
        } else {
            ++it;
        }
    }

    // Vyžiadať chýbajúce, najbližšie najskôr
    std::vector<glm::ivec2> wanted;
    for (int dz = -r; dz <= r; ++dz)
        for (int dx = -r; dx <= r; ++dx)
            if (dx * dx + dz * dz <= r * r) wanted.push_back(center + glm::ivec2(dx, dz));
    std::sort(wanted.begin(), wanted.end(), [&](glm::ivec2 a, glm::ivec2 b) {
        glm::ivec2 da = a - center, db = b - center;
        return da.x * da.x + da.y * da.y < db.x * db.x + db.y * db.y;
    });
    {
        std::lock_guard lock(m_mutex);
        for (glm::ivec2 c : m_queue) m_pending.erase(key(c));
        m_queue.clear();
        for (glm::ivec2 c : wanted) {
            int64_t k = key(c);
            if (m_tiles.count(k) || m_pending.count(k)) continue;
            m_pending.insert(k);
            m_queue.push_back(c);
        }
    }
    m_cv.notify_all();
}

void FarTerrain::draw(const glm::mat4& viewProjection, const glm::vec3& cameraPos, const SkyState& sky,
                      const glm::vec3& fogColor, float fogStart, float fogEnd, float nearRadius)
{
    m_shader.bind();
    m_shader.setMat4("uViewProj", viewProjection);
    m_shader.setVec3("uCameraPos", cameraPos);
    m_shader.setVec3("uSunDir", sky.sunDir);
    m_shader.setVec3("uSunColor", sky.sunColor);
    m_shader.setFloat("uSunBrightness", sky.sunBrightness);
    m_shader.setVec3("uFogColor", fogColor);
    m_shader.setFloat("uFogStart", fogStart);
    m_shader.setFloat("uFogEnd", fogEnd);
    m_shader.setFloat("uNearRadius", nearRadius);
    glDisable(GL_CULL_FACE); // z diaľky je vidno len vrch, culling netreba riešiť
    for (auto& [k, t] : m_tiles) {
        glBindVertexArray(t.vao);
        glDrawArrays(GL_TRIANGLES, 0, t.vertexCount);
    }
    glEnable(GL_CULL_FACE);
}

} // namespace cubiq
