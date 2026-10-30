#pragma once
#include "World/Chunk.h"

#include <glm/vec3.hpp>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace cubiq {

class World;
class TerrainGenerator;

// Načítava chunky okolo hráča a uvoľňuje vzdialené.
// Generovanie beží vo vláknach na pozadí, meshing na hlavnom vlákne (kvôli OpenGL)
// s časovým limitom na snímku, aby sa hra nesekala.
class ChunkManager {
public:
    ChunkManager(World& world, const TerrainGenerator& generator, int renderDistance, int atlasTiles);
    ~ChunkManager();

    ChunkManager(const ChunkManager&) = delete;
    ChunkManager& operator=(const ChunkManager&) = delete;

    void update(const glm::vec3& playerPos);

    int renderDistance() const { return m_renderDistance; }
    void setRenderDistance(int chunks);

    size_t pendingCount();

    // Štatistika meshingu (pre debug v titulku).
    int meshedTotal() const { return m_meshedTotal; }
    double meshMsAverage() const { return m_meshedTotal ? m_meshMsTotal / m_meshedTotal : 0.0; }
private:
    void requestMissing(glm::ivec2 center);
    void integrateFinished();
    void unloadFar(glm::ivec2 center);
    void buildMeshes(glm::ivec2 center);
    void workerLoop();

    World& m_world;
    const TerrainGenerator& m_generator;
    int m_renderDistance;
    int m_atlasTiles;
    glm::ivec2 m_lastCenter{INT32_MAX, INT32_MAX};
    bool m_forceRequest = true;
    int m_meshedTotal = 0;
    double m_meshMsTotal = 0.0;
    std::vector<std::thread> m_workers;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<glm::ivec2> m_queue;                  // čaká na vlákno
    std::unordered_set<int64_t> m_pending;           // vo fronte alebo sa práve generuje
    std::vector<std::unique_ptr<Chunk>> m_finished;  // vygenerované, čaká na vloženie do sveta
    bool m_stop = false;
};

} // namespace cubiq
