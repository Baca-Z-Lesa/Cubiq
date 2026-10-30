#include "World/ChunkManager.h"
#include "World/ChunkMesher.h"
#include "World/TerrainGenerator.h"
#include "World/World.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace cubiq {

namespace {
    int distSq(glm::ivec2 a, glm::ivec2 b)
    {
        glm::ivec2 d = a - b;
        return d.x * d.x + d.y * d.y;
    }

    constexpr double MESH_BUDGET_MS = 4.0; // koľko času na snímku smie zabrať meshing
}

ChunkManager::ChunkManager(World& world, int renderDistance, int atlasTiles)
    : m_world(world), m_renderDistance(renderDistance), m_atlasTiles(atlasTiles)
{
    // Jedno vlákno nechávame hlavnej slučke.
    unsigned threads = std::max(1u, std::thread::hardware_concurrency() - 1);
    for (unsigned i = 0; i < threads; ++i)
        m_workers.emplace_back(&ChunkManager::workerLoop, this);
}

ChunkManager::~ChunkManager()
{
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    for (auto& t : m_workers) t.join();
}

void ChunkManager::setRenderDistance(int chunks)
{
    m_renderDistance = std::clamp(chunks, 2, 32);
    m_forceRequest = true;
}

size_t ChunkManager::pendingCount()
{
    std::lock_guard lock(m_mutex);
    return m_pending.size();
}

void ChunkManager::update(const glm::vec3& playerPos)
{
    glm::ivec2 center = World::chunkCoordOf((int)std::floor(playerPos.x), (int)std::floor(playerPos.z));

    integrateFinished();
    if (center != m_lastCenter || m_forceRequest) {
        m_lastCenter = center;
        m_forceRequest = false;
        unloadFar(center);
        requestMissing(center);
    }
    buildMeshes(center);
}

void ChunkManager::requestMissing(glm::ivec2 center)
{
    // +1 okraj: chunky na hrane sa generujú, aby mali susedov tie vo vnútri.
    const int r = m_renderDistance + 1;
    std::vector<glm::ivec2> wanted;
    for (int dx = -r; dx <= r; ++dx)
        for (int dz = -r; dz <= r; ++dz)
            if (dx * dx + dz * dz <= r * r)
                wanted.push_back(center + glm::ivec2(dx, dz));

    // Najbližšie chunky najskôr.
    std::sort(wanted.begin(), wanted.end(),
              [&](glm::ivec2 a, glm::ivec2 b) { return distSq(a, center) < distSq(b, center); });

    {
        std::lock_guard lock(m_mutex);
        // Starý front zahodíme (hráč sa pohol), rozrobené chunky ostávajú v m_pending.
        for (glm::ivec2 c : m_queue) m_pending.erase(World::key(c));
        m_queue.clear();

        for (glm::ivec2 c : wanted) {
            int64_t k = World::key(c);
            if (m_world.getChunk(c) || m_pending.count(k)) continue;
            m_pending.insert(k);
            m_queue.push_back(c);
        }
    }
    m_cv.notify_all();
}

void ChunkManager::integrateFinished()
{
    std::vector<std::unique_ptr<Chunk>> finished;
    {
        std::lock_guard lock(m_mutex);
        finished.swap(m_finished);
        for (auto& c : finished) m_pending.erase(World::key(c->coord()));
    }
    for (auto& c : finished) m_world.insertChunk(std::move(c));
}

void ChunkManager::unloadFar(glm::ivec2 center)
{
    // Rezerva +3, aby sa chunky neuvoľňovali a nenačítavali stále dookola na hranici.
    const int r = m_renderDistance + 3;
    std::vector<glm::ivec2> far;
    m_world.forEachChunk([&](Chunk& c) {
        if (distSq(c.coord(), center) > r * r) far.push_back(c.coord());
    });
    for (glm::ivec2 c : far) m_world.removeChunk(c);
}

void ChunkManager::buildMeshes(glm::ivec2 center)
{
    const int r = m_renderDistance;
    std::vector<Chunk*> todo;
    m_world.forEachChunk([&](Chunk& c) {
        if (c.dirty && distSq(c.coord(), center) <= r * r && m_world.hasAllNeighbors(c.coord()))
            todo.push_back(&c);
    });
    std::sort(todo.begin(), todo.end(), [&](Chunk* a, Chunk* b) {
        return distSq(a->coord(), center) < distSq(b->coord(), center);
    });

    // Do limitu rátame len CPU meshing. Prvé OpenGL volanie po swapBuffers často čaká
    // na VSync (~16 ms), takže čas nahrávania na GPU by limit hneď vyčerpal.
    double cpuMs = 0.0;
    for (Chunk* c : todo) {
        auto t0 = std::chrono::steady_clock::now();
        MeshData data = buildChunkMesh(m_world, *c, m_atlasTiles);
        auto tCpu = std::chrono::steady_clock::now();
        c->mesh = std::make_unique<Mesh>(data.vertices, data.indices);
        c->dirty = false;
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(tCpu - t0).count();
        cpuMs += ms;
        m_meshMsTotal += ms;
        m_uploadMsTotal += std::chrono::duration<double, std::milli>(t1 - tCpu).count();
        ++m_meshedTotal;
        if (cpuMs > MESH_BUDGET_MS) break;
    }
}

void ChunkManager::workerLoop()
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

        // Chunk ešte nie je vo svete, takže ho vlákno vlastní len ono – netreba zámok.
        auto chunk = std::make_unique<Chunk>(coord);
        generateTerrain(*chunk);

        std::lock_guard lock(m_mutex);
        m_finished.push_back(std::move(chunk));
    }
}

} // namespace cubiq
