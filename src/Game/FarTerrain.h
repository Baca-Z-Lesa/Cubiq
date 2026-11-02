#pragma once
#include "Engine/Shader.h"

#include <glm/glm.hpp>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cubiq {

class TerrainGenerator;
struct SkyState;

// Vzdialený terén (LOD): za hranicou načítaných chunkov sa kreslí zjednodušená krajina
// z 2D výškovej mapy generátora – štvorcové dlaždice 64×64 blokov, bližšie s krokom 4 bloky,
// ďalej (LOD 2) s krokom 8. Farba podľa biómu. Dlaždice sa počítajú vo vlastnom vlákne;
// vnútri dohľadu chunkov sa nekreslia (shader ich vystrihne).
class FarTerrain {
public:
    static constexpr int TILE = 64;
    static constexpr int FINE_STEP = 4;
    static constexpr int COARSE_STEP = 8;
    static constexpr int COARSE_FROM = 768; // od tejto vzdialenosti hrubšie dlaždice

    explicit FarTerrain(const TerrainGenerator& generator);
    ~FarTerrain();

    FarTerrain(const FarTerrain&) = delete;
    FarTerrain& operator=(const FarTerrain&) = delete;

    void update(const glm::vec3& cameraPos, float farRadius);
    void draw(const glm::mat4& viewProjection, const glm::vec3& cameraPos, const SkyState& sky,
              const glm::vec3& fogColor, float fogStart, float fogEnd, float nearRadius);

    size_t tileCount() const { return m_tiles.size(); }
    int drawnTiles() const { return m_drawn; }

private:
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec3 normal;
    };
    struct Request {
        glm::ivec2 coord;
        int step;
    };
    struct TileData {
        glm::ivec2 coord;
        int step;
        float minY, maxY;
        std::vector<Vertex> vertices;
    };
    struct Tile {
        unsigned int vao = 0, vbo = 0;
        int vertexCount = 0;
        int step = 0;
        float minY = 0, maxY = 0;
    };

    TileData buildTile(Request r) const;
    void workerLoop();
    int stepFor(glm::ivec2 tile, glm::ivec2 center) const;
    static int64_t key(glm::ivec2 c) { return ((int64_t)c.x << 32) | (uint32_t)c.y; }
    static void release(Tile& t);

    const TerrainGenerator& m_generator;
    Shader m_shader;
    std::unordered_map<int64_t, Tile> m_tiles;
    glm::ivec2 m_lastCenter{INT32_MAX, INT32_MAX};
    float m_lastRadius = 0.0f;
    int m_drawn = 0;

    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<Request> m_queue;
    std::unordered_set<int64_t> m_pending;
    std::vector<TileData> m_done;
    bool m_stop = false;
};

} // namespace cubiq
