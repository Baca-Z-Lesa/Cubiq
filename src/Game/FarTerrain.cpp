#include "Game/FarTerrain.h"
#include "Engine/Frustum.h"
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
    int chebyshev(glm::ivec2 d) { return std::max(std::abs(d.x), std::abs(d.y)); }

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
    for (auto& [k, t] : m_tiles) release(t);
}

void FarTerrain::release(Tile& t)
{
    glDeleteBuffers(1, &t.vbo);
    glDeleteVertexArrays(1, &t.vao);
}

int FarTerrain::stepFor(glm::ivec2 tile, glm::ivec2 center) const
{
    return chebyshev(tile - center) * TILE >= COARSE_FROM ? COARSE_STEP : FINE_STEP;
}

FarTerrain::TileData FarTerrain::buildTile(Request r) const
{
    // Krajina z "veľkých kociek": každá bunka step×step blokov je stĺpec s rovným vrchom
    // a zvislými stenami tam, kde je sused nižšie – z diaľky vyzerá ako bloky, nie hladký terén.
    const int step = r.step;
    const int N = TILE / step; // bunky na stranu
    const int baseX = r.coord.x * TILE, baseZ = r.coord.y * TILE;
    const int sea = TerrainGenerator::SEA_LEVEL;

    // Výšky (s okrajom 1 bunky – kvôli stenám na hranici dlaždice) a farby vrchov.
    std::vector<float> height((N + 2) * (N + 2));
    std::vector<glm::vec3> color(N * N);
    auto H = [&](int i, int j) -> float& { return height[(j + 1) * (N + 2) + (i + 1)]; };
    for (int j = -1; j <= N; ++j)
        for (int i = -1; i <= N; ++i) {
            // vzorka zo stredu bunky
            TerrainGenerator::Column c = m_generator.columnAt(baseX + i * step + step / 2, baseZ + j * step + step / 2);
            H(i, j) = (float)std::max(c.height, sea) + 1.0f; // vrch bloku; voda = hladina
            if (i >= 0 && j >= 0 && i < N && j < N) color[j * N + i] = surfaceColor(c);
        }

    TileData t;
    t.coord = r.coord;
    t.step = step;
    t.minY = 1e9f;
    t.maxY = -1e9f;
    t.vertices.reserve(N * N * 6 * 3);

    auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 col, glm::vec3 n) {
        t.vertices.insert(t.vertices.end(), {{a, col, n}, {b, col, n}, {c, col, n}, {a, col, n}, {c, col, n}, {d, col, n}});
    };

    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i) {
            const float h = H(i, j);
            const float x0 = (float)(baseX + i * step), x1 = x0 + step;
            const float z0 = (float)(baseZ + j * step), z1 = z0 + step;
            const glm::vec3 top = color[j * N + i];
            // Boky sú tmavšie (hlina / skala pod trávou), voda má boky rovnaké.
            const glm::vec3 side = top.b > top.g && top.b > top.r ? top : top * 0.62f + glm::vec3(0.12f, 0.08f, 0.04f);
            t.minY = std::min(t.minY, h);
            t.maxY = std::max(t.maxY, h);

            quad({x0, h, z0}, {x1, h, z0}, {x1, h, z1}, {x0, h, z1}, top, {0, 1, 0});

            // Steny smerom k nižším susedom (až po ich výšku).
            float hn;
            if ((hn = H(i + 1, j)) < h) quad({x1, hn, z0}, {x1, h, z0}, {x1, h, z1}, {x1, hn, z1}, side, {1, 0, 0});
            if ((hn = H(i - 1, j)) < h) quad({x0, hn, z1}, {x0, h, z1}, {x0, h, z0}, {x0, hn, z0}, side, {-1, 0, 0});
            if ((hn = H(i, j + 1)) < h) quad({x1, hn, z1}, {x1, h, z1}, {x0, h, z1}, {x0, hn, z1}, side, {0, 0, 1});
            if ((hn = H(i, j - 1)) < h) quad({x0, hn, z0}, {x0, h, z0}, {x1, h, z0}, {x1, hn, z0}, side, {0, 0, -1});
            t.minY = std::min(t.minY, std::min({H(i + 1, j), H(i - 1, j), H(i, j + 1), H(i, j - 1)}));
        }
    return t;
}

void FarTerrain::workerLoop()
{
    for (;;) {
        Request req;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [&] { return m_stop || !m_queue.empty(); });
            if (m_stop) return;
            req = m_queue.front();
            m_queue.pop_front();
        }
        TileData data = buildTile(req);
        std::lock_guard lock(m_mutex);
        m_done.push_back(std::move(data));
    }
}

void FarTerrain::update(const glm::vec3& cameraPos, float farRadius)
{
    const glm::ivec2 center(floorDiv((int)std::floor(cameraPos.x), TILE), floorDiv((int)std::floor(cameraPos.z), TILE));
    const int r = (int)std::ceil(farRadius / TILE);

    // Hotové dlaždice → GPU (max. pár za snímku). Hrubšiu/jemnejšiu verziu nahradia na mieste.
    std::vector<TileData> done;
    {
        std::lock_guard lock(m_mutex);
        size_t n = std::min<size_t>(6, m_done.size());
        for (size_t i = 0; i < n; ++i) done.push_back(std::move(m_done[i]));
        m_done.erase(m_done.begin(), m_done.begin() + n);
        for (auto& d : done) m_pending.erase(key(d.coord));
    }
    for (TileData& d : done) {
        if (chebyshev(d.coord - center) > r + 1) continue; // medzitým odišla z dosahu
        Tile t;
        t.vertexCount = (int)d.vertices.size();
        t.step = d.step;
        t.minY = d.minY;
        t.maxY = d.maxY;
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
        auto it = m_tiles.find(key(d.coord));
        if (it != m_tiles.end()) release(it->second);
        m_tiles[key(d.coord)] = t;
    }

    if (center == m_lastCenter && farRadius == m_lastRadius) return;
    m_lastCenter = center;
    m_lastRadius = farRadius;

    // Uvoľniť vzdialené (štvorec, ako chunky)
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        glm::ivec2 c((int)(it->first >> 32), (int)(int32_t)(it->first & 0xFFFFFFFF));
        if (chebyshev(c - center) > r + 1) {
            release(it->second);
            it = m_tiles.erase(it);
        } else {
            ++it;
        }
    }

    // Vyžiadať chýbajúce alebo s nesprávnou úrovňou detailu, najbližšie najskôr.
    std::vector<glm::ivec2> wanted;
    for (int dz = -r; dz <= r; ++dz)
        for (int dx = -r; dx <= r; ++dx) wanted.push_back(center + glm::ivec2(dx, dz));
    std::sort(wanted.begin(), wanted.end(), [&](glm::ivec2 a, glm::ivec2 b) {
        glm::ivec2 da = a - center, db = b - center;
        return da.x * da.x + da.y * da.y < db.x * db.x + db.y * db.y;
    });
    {
        std::lock_guard lock(m_mutex);
        for (const Request& q : m_queue) m_pending.erase(key(q.coord));
        m_queue.clear();
        for (glm::ivec2 c : wanted) {
            int64_t k = key(c);
            int step = stepFor(c, center);
            auto it = m_tiles.find(k);
            if ((it != m_tiles.end() && it->second.step == step) || m_pending.count(k)) continue;
            m_pending.insert(k);
            m_queue.push_back({c, step});
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

    const Frustum frustum(viewProjection);
    glDisable(GL_CULL_FACE); // z diaľky je vidno len vrch, culling netreba riešiť
    // Skutočné chunky majú prednosť tam, kde sa s vzdialeným terénom prekrývajú.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 8.0f);
    m_drawn = 0;
    for (auto& [k, t] : m_tiles) {
        glm::ivec2 c((int)(k >> 32), (int)(int32_t)(k & 0xFFFFFFFF));
        glm::vec3 mn(c.x * TILE, t.minY - 1.0f, c.y * TILE), mx(mn.x + TILE, t.maxY + 1.0f, mn.z + TILE);
        if (!frustum.intersects(mn, mx)) continue;
        glBindVertexArray(t.vao);
        glDrawArrays(GL_TRIANGLES, 0, t.vertexCount);
        ++m_drawn;
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_CULL_FACE);
}

} // namespace cubiq
