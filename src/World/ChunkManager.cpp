#include "World/ChunkManager.h"
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
}

ChunkManager::ChunkManager(World& world, const TerrainGenerator& generator, int renderDistance, int atlasTiles)
    : m_world(world), m_generator(generator), m_renderDistance(renderDistance), m_atlasTiles(atlasTiles)
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
    uploadMeshes();
    if (center != m_lastCenter || m_forceRequest) {
        m_lastCenter = center;
        m_forceRequest = false;
        unloadFar(center);
        requestMissing(center);
    }
    scheduleMeshes(center);
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
        for (glm::ivec2 c : m_genQueue) m_pending.erase(World::key(c));
        m_genQueue.clear();

        for (glm::ivec2 c : wanted) {
            int64_t k = World::key(c);
            if (m_world.getChunk(c) || m_pending.count(k)) continue;
            m_pending.insert(k);
            m_genQueue.push_back(c);
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

void ChunkManager::scheduleMeshes(glm::ivec2 center)
{
    // Držíme frontu krátku, aby sa zmeny (kopanie, stavanie) nepostavili za stovky čakajúcich chunkov.
    const int maxInFlight = (int)m_workers.size() * 2;
    if (m_meshInFlight >= maxInFlight) return;

    const int r = m_renderDistance;
    std::vector<Chunk*> todo;
    m_world.forEachChunk([&](Chunk& c) {
        if (c.dirty && !c.meshing && distSq(c.coord(), center) <= r * r && m_world.hasAllNeighbors(c.coord()))
            todo.push_back(&c);
    });
    std::sort(todo.begin(), todo.end(), [&](Chunk* a, Chunk* b) {
        return distSq(a->coord(), center) < distSq(b->coord(), center);
    });

    std::vector<std::unique_ptr<MeshInput>> jobs;
    for (Chunk* c : todo) {
        if (m_meshInFlight + (int)jobs.size() >= maxInFlight) break;
        jobs.push_back(std::make_unique<MeshInput>(snapshotChunk(m_world, *c)));
        c->dirty = false;
        c->meshing = true;
    }
    if (jobs.empty()) return;

    {
        std::lock_guard lock(m_mutex);
        for (auto& j : jobs) m_meshQueue.push_back(std::move(j));
    }
    m_meshInFlight += (int)jobs.size();
    m_cv.notify_all();
}

void ChunkManager::uploadMeshes()
{
    std::vector<MeshResult> results;
    {
        std::lock_guard lock(m_mutex);
        results.swap(m_meshResults);
    }
    for (MeshResult& r : results) {
        --m_meshInFlight;
        m_meshMsTotal += r.ms;
        ++m_meshedTotal;
        // Chunk mohol byť medzitým uvoľnený – potom výsledok zahodíme.
        Chunk* c = m_world.getChunk(r.coord);
        if (!c) continue;
        c->mesh = std::make_unique<Mesh>(r.data.opaque.vertices, r.data.opaque.indices);
        c->liquidMesh = std::make_unique<Mesh>(r.data.liquid.vertices, r.data.liquid.indices);
        c->meshing = false;
    }
}

void ChunkManager::workerLoop()
{
    for (;;) {
        std::unique_ptr<MeshInput> meshJob;
        glm::ivec2 genCoord;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [&] { return m_stop || !m_meshQueue.empty() || !m_genQueue.empty(); });
            if (m_stop) return;
            if (!m_meshQueue.empty()) {
                meshJob = std::move(m_meshQueue.front());
                m_meshQueue.pop_front();
            } else {
                genCoord = m_genQueue.front();
                m_genQueue.pop_front();
            }
        }

        if (meshJob) {
            auto t0 = std::chrono::steady_clock::now();
            ChunkMeshData data = buildChunkMesh(*meshJob, m_atlasTiles);
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            std::lock_guard lock(m_mutex);
            m_meshResults.push_back({meshJob->coord, std::move(data), ms});
            continue;
        }

        // Chunk ešte nie je vo svete, takže ho vlákno vlastní len ono – netreba zámok.
        auto chunk = std::make_unique<Chunk>(genCoord);
        m_generator.generate(*chunk);

        std::lock_guard lock(m_mutex);
        m_finished.push_back(std::move(chunk));
    }
}

} // namespace cubiq
