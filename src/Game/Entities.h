#pragma once
#include "World/Physics.h"

#include <glm/glm.hpp>
#include <cstdint>
#include <random>
#include <vector>

namespace cubiq {

class World;
class TerrainGenerator;

enum class MobType : uint8_t { Deer, Sheep, Frog, Count };

// Jedna časť kockatého modelu (v blokoch, voči stredu chodidiel, tvor otočený do +Z).
struct ModelPart {
    glm::vec3 center;
    glm::vec3 size;
    glm::vec3 color;
    int swing; // 0 = pevná, 1/-1 = noha kývajúca sa v protifáze, 2 = hlava (pokyvuje)
};

struct MobInfo {
    const char* name;
    float width, height;     // kolízny kváder
    float walkSpeed, fleeSpeed;
    int health;
    bool skittish;           // uteká pred hráčom, aj keď ho neudrie
    bool hops;               // pohybuje sa skokmi (žaba)
    std::vector<ModelPart> parts;
};

const MobInfo& mobInfo(MobType type);

struct Mob {
    uint32_t id;
    MobType type;
    glm::vec3 position;      // stred chodidiel
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;        // smer tela (radiány, 0 = +Z)
    bool onGround = false;
    int health;
    float hurtTimer = 0.0f;  // červené bliknutie po zásahu
    float walkPhase = 0.0f;  // animácia nôh

    enum class State { Idle, Wander, Flee } state = State::Idle;
    float stateTimer = 0.0f;
    std::vector<glm::ivec3> path;
    size_t pathIndex = 0;
    float repathTimer = 0.0f;
    float stuckTimer = 0.0f;
    uint32_t herd = 0;       // ovečky v stáde majú rovnaké číslo

    AABB bounds() const;
};

// Všetky zvieratá: spawn okolo hráča podľa biómu, AI (A*), fyzika, zásahy.
class EntityManager {
public:
    static constexpr int MAX_MOBS = 36;

    void update(const World& world, const TerrainGenerator& generator, const glm::vec3& playerPos, float dt);

    // Lúč z oka: najbližšie zviera do maxDistance (na úder). -1 = nič.
    int raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDistance, float& distance) const;
    // Úder: odhodí ho, zraní, a vyplaší (aj okolie).
    void hit(int index, const glm::vec3& fromDir);

    const std::vector<Mob>& mobs() const { return m_mobs; }

private:
    void trySpawn(const World& world, const TerrainGenerator& generator, const glm::vec3& playerPos);
    void think(Mob& m, const World& world, const glm::vec3& playerPos, float dt);
    void followPath(Mob& m, const World& world, float speed);
    bool planTo(Mob& m, const World& world, glm::ivec3 goal);

    std::vector<Mob> m_mobs;
    std::mt19937 m_rng{1234567u};
    uint32_t m_nextId = 1;
    float m_spawnTimer = 0.0f;
};

} // namespace cubiq
