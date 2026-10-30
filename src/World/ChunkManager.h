#pragma once
#include "World/Chunk.h"
#include "World/ChunkMesher.h"

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
class WorldStorage;

// Načítava chunky okolo hráča a uvoľňuje vzdialené.
// Generovanie aj stavba meshov bežia vo vláknach na pozadí; hlavné vlákno len
// pripraví kópiu dát (snapshot) a hotový mesh nahrá na GPU (OpenGL ide len z hlavného vlákna).
class ChunkManager {
public:
    // storage môže byť nullptr (svet sa neukladá – napr. pozadie menu).
    ChunkManager(World& world, const TerrainGenerator& generator, WorldStorage* storage, int renderDistance,
                 int atlasTiles);
    ~ChunkManager();

    ChunkManager(const ChunkManager&) = delete;
    ChunkManager& operator=(const ChunkManager&) = delete;

    void update(const glm::vec3& playerPos);

    int renderDistance() const { return m_renderDistance; }
    void setRenderDistance(int chunks);

    size_t pendingCount();

    // Uloží všetky zmenené načítané chunky a zapíše ich na disk.
    void saveAll();

    // Štatistika meshingu (pre debug v titulku).
    int meshedTotal() const { return m_meshedTotal; }
    double meshMsAverage() const { return m_meshedTotal ? m_meshMsTotal / m_meshedTotal : 0.0; }

private:
    struct MeshResult {
        glm::ivec2 coord;
        ChunkMeshData data;
        double ms;
    };

    void requestMissing(glm::ivec2 center);
    void integrateFinished();
    void unloadFar(glm::ivec2 center);
    void scheduleMeshes(glm::ivec2 center);
    void uploadMeshes();
    void workerLoop();

    World& m_world;
    const TerrainGenerator& m_generator;
    WorldStorage* m_storage;
    int m_renderDistance;
    int m_atlasTiles;
    glm::ivec2 m_lastCenter{INT32_MAX, INT32_MAX};
    bool m_forceRequest = true;
    int m_meshedTotal = 0;
    double m_meshMsTotal = 0.0;

    std::vector<std::thread> m_workers;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;

    // generovanie
    std::deque<glm::ivec2> m_genQueue;               // čaká na vlákno
    std::unordered_set<int64_t> m_pending;           // vo fronte alebo sa práve generuje
    std::vector<std::unique_ptr<Chunk>> m_finished;  // vygenerované, čaká na vloženie do sveta

    // meshing (má prednosť pred generovaním – úpravy sveta sa prejavia hneď)
    std::deque<std::unique_ptr<MeshInput>> m_meshQueue;
    std::vector<MeshResult> m_meshResults;
    int m_meshInFlight = 0;
};

} // namespace cubiq
