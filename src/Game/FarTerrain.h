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
// z 2D výškovej mapy generátora – dlaždice 64×64 blokov s krokom 4 bloky, farba podľa biómu.
// Dlaždice sa počítajú vo vlastnom vlákne; vnútri dohľadu chunkov sa nekreslia (shader ich vystrihne).
class FarTerrain {
public:
    static constexpr int TILE = 64;
    static constexpr int STEP = 4;

    explicit FarTerrain(const TerrainGenerator& generator);
    ~FarTerrain();

    FarTerrain(const FarTerrain&) = delete;
    FarTerrain& operator=(const FarTerrain&) = delete;

    void update(const glm::vec3& cameraPos, float farRadius);
    void draw(const glm::mat4& viewProjection, const glm::vec3& cameraPos, const SkyState& sky,
              const glm::vec3& fogColor, float fogStart, float fogEnd, float nearRadius);

    size_t tileCount() const { return m_tiles.size(); }

private:
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec3 normal;
    };
    struct TileData {
        glm::ivec2 coord;
        std::vector<Vertex> vertices;
    };
    struct Tile {
        unsigned int vao = 0, vbo = 0;
        int vertexCount = 0;
    };

    TileData buildTile(glm::ivec2 coord) const;
    void workerLoop();
    static int64_t key(glm::ivec2 c) { return ((int64_t)c.x << 32) | (uint32_t)c.y; }

    const TerrainGenerator& m_generator;
    Shader m_shader;
    std::unordered_map<int64_t, Tile> m_tiles;
    glm::ivec2 m_lastCenter{INT32_MAX, INT32_MAX};

    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<glm::ivec2> m_queue;
    std::unordered_set<int64_t> m_pending;
    std::vector<TileData> m_done;
    bool m_stop = false;
};

} // namespace cubiq
