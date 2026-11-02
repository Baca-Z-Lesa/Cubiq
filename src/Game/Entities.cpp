#include "Game/Entities.h"
#include "Game/Pathfinder.h"
#include "World/TerrainGenerator.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    constexpr float GRAVITY = 32.0f;
    constexpr float DESPAWN_DISTANCE = 112.0f;
    constexpr float FLEE_RADIUS = 7.0f;   // plaché zvieratá utekajú, keď sa hráč priblíži
    constexpr float SAFE_RADIUS = 18.0f;  // ... kým nie sú takto ďaleko

    glm::ivec3 blockOf(const glm::vec3& p) { return {(int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z)}; }

    std::vector<MobInfo> buildInfos()
    {
        std::vector<MobInfo> v(3);
        // Jeleň: štíhle telo, dlhé nohy, krk, parohy.
        const glm::vec3 BROWN(0.55f, 0.38f, 0.22f), BELLY(0.78f, 0.66f, 0.50f), DARK(0.28f, 0.20f, 0.13f), ANTLER(0.86f, 0.80f, 0.66f);
        v[0] = {"Jelen", 0.7f, 1.6f, 1.7f, 6.5f, 6, true, false, {
            {{0.0f, 1.05f, 0.0f}, {0.5f, 0.5f, 1.05f}, BROWN, 0},
            {{0.0f, 0.86f, 0.0f}, {0.42f, 0.12f, 0.9f}, BELLY, 0},
            {{0.0f, 1.45f, 0.48f}, {0.24f, 0.5f, 0.24f}, BROWN, 2},          // krk
            {{0.0f, 1.72f, 0.62f}, {0.3f, 0.28f, 0.42f}, BROWN, 2},          // hlava
            {{0.0f, 1.66f, 0.86f}, {0.16f, 0.14f, 0.14f}, DARK, 2},          // nos
            {{0.12f, 2.0f, 0.55f}, {0.05f, 0.34f, 0.05f}, ANTLER, 2},        // parohy
            {{-0.12f, 2.0f, 0.55f}, {0.05f, 0.34f, 0.05f}, ANTLER, 2},
            {{0.2f, 2.12f, 0.55f}, {0.2f, 0.05f, 0.05f}, ANTLER, 2},
            {{-0.2f, 2.12f, 0.55f}, {0.2f, 0.05f, 0.05f}, ANTLER, 2},
            {{0.0f, 1.2f, -0.55f}, {0.12f, 0.16f, 0.1f}, BELLY, 0},          // chvost
            {{0.17f, 0.4f, 0.38f}, {0.12f, 0.8f, 0.12f}, DARK, 1},           // nohy
            {{-0.17f, 0.4f, 0.38f}, {0.12f, 0.8f, 0.12f}, DARK, -1},
            {{0.17f, 0.4f, -0.38f}, {0.12f, 0.8f, 0.12f}, DARK, -1},
            {{-0.17f, 0.4f, -0.38f}, {0.12f, 0.8f, 0.12f}, DARK, 1},
        }};
        // Ovečka: chlpaté telo, tmavá hlava a nožičky.
        const glm::vec3 WOOL(0.93f, 0.92f, 0.88f), FACE(0.25f, 0.23f, 0.22f);
        v[1] = {"Ovecka", 0.8f, 1.2f, 1.2f, 4.5f, 4, false, false, {
            {{0.0f, 0.75f, 0.0f}, {0.78f, 0.6f, 1.0f}, WOOL, 0},
            {{0.0f, 0.95f, 0.6f}, {0.38f, 0.38f, 0.36f}, FACE, 2},
            {{0.0f, 1.12f, 0.55f}, {0.44f, 0.12f, 0.3f}, WOOL, 2},           // čuprina
            {{0.24f, 0.22f, 0.32f}, {0.16f, 0.44f, 0.16f}, FACE, 1},
            {{-0.24f, 0.22f, 0.32f}, {0.16f, 0.44f, 0.16f}, FACE, -1},
            {{0.24f, 0.22f, -0.32f}, {0.16f, 0.44f, 0.16f}, FACE, -1},
            {{-0.24f, 0.22f, -0.32f}, {0.16f, 0.44f, 0.16f}, FACE, 1},
        }};
        // Žabka: plochá, veľké oči, skáče.
        const glm::vec3 GREEN(0.36f, 0.62f, 0.26f), LIGHT(0.72f, 0.82f, 0.48f), EYE(0.95f, 0.9f, 0.4f);
        v[2] = {"Zabka", 0.5f, 0.45f, 2.0f, 3.5f, 2, true, true, {
            {{0.0f, 0.2f, 0.0f}, {0.46f, 0.26f, 0.5f}, GREEN, 0},
            {{0.0f, 0.12f, 0.04f}, {0.4f, 0.1f, 0.42f}, LIGHT, 0},
            {{0.13f, 0.38f, 0.16f}, {0.13f, 0.13f, 0.13f}, EYE, 0},
            {{-0.13f, 0.38f, 0.16f}, {0.13f, 0.13f, 0.13f}, EYE, 0},
            {{0.22f, 0.07f, -0.16f}, {0.16f, 0.12f, 0.24f}, GREEN, 1},       // zadné nohy
            {{-0.22f, 0.07f, -0.16f}, {0.16f, 0.12f, 0.24f}, GREEN, 1},
        }};
        return v;
    }
}

const MobInfo& mobInfo(MobType type)
{
    static const std::vector<MobInfo> INFOS = buildInfos();
    return INFOS[(int)type];
}

AABB Mob::bounds() const
{
    const MobInfo& info = mobInfo(type);
    float hw = info.width * 0.5f;
    return {position - glm::vec3(hw, 0.0f, hw), position + glm::vec3(hw, info.height, hw)};
}

// ------------------------------------------------------------------ spawn

void EntityManager::trySpawn(const World& world, const TerrainGenerator& gen, const glm::vec3& player)
{
    if ((int)m_mobs.size() >= MAX_MOBS) return;
    std::uniform_real_distribution<float> angle(0.0f, 6.2832f), dist(24.0f, 64.0f), unit(0.0f, 1.0f);

    float a = angle(m_rng), r = dist(m_rng);
    int x = (int)std::floor(player.x + std::cos(a) * r), z = (int)std::floor(player.z + std::sin(a) * r);
    const Chunk* chunk = world.getChunk(World::chunkCoordOf(x, z));
    if (!chunk || !chunk->mesh) return; // len v načítanom a nakreslenom teréne

    // Najvyšší pevný blok v stĺpci
    int y = chunk->maxY();
    while (y > 0 && !isSolid(world.getBlock(x, y, z))) --y;
    BlockId ground = world.getBlock(x, y, z);
    if (isCutout(ground)) return; // na strome nie
    glm::ivec3 feet(x, y + 1, z);

    Biome biome = gen.biomeAt(x, z);
    MobType type;
    int group = 1;
    bool nearWater = isLiquid(world.getBlock(x + 2, y, z)) || isLiquid(world.getBlock(x - 2, y, z)) ||
                     isLiquid(world.getBlock(x, y, z + 2)) || isLiquid(world.getBlock(x, y, z - 2));
    if (nearWater && (ground == BlockId::Grass || ground == BlockId::Sand) && unit(m_rng) < 0.7f) {
        type = MobType::Frog;
        group = 1 + (int)(unit(m_rng) * 2);
    } else if ((biome == Biome::Forest || biome == Biome::BirchForest || biome == Biome::Tundra || biome == Biome::CherryGrove) &&
               (ground == BlockId::Grass || ground == BlockId::SnowyGrass)) {
        type = MobType::Deer;
        group = 1 + (int)(unit(m_rng) * 3);
    } else if ((biome == Biome::Plains || biome == Biome::Mountains || biome == Biome::CherryGrove) && ground == BlockId::Grass) {
        type = MobType::Sheep;
        group = 3 + (int)(unit(m_rng) * 3);
    } else {
        return;
    }

    Pathfinder::Options opt;
    opt.height = (int)std::ceil(mobInfo(type).height);
    uint32_t herd = m_nextId;
    for (int i = 0; i < group && (int)m_mobs.size() < MAX_MOBS; ++i) {
        glm::ivec3 p = feet + glm::ivec3((int)(unit(m_rng) * 5) - 2, 0, (int)(unit(m_rng) * 5) - 2);
        // dorovnať výšku podľa terénu
        int py = p.y + 2;
        while (py > p.y - 4 && !Pathfinder::standable(world, {p.x, py, p.z}, opt)) --py;
        if (!Pathfinder::standable(world, {p.x, py, p.z}, opt)) continue;

        Mob m;
        m.id = m_nextId++;
        m.type = type;
        m.position = glm::vec3(p.x + 0.5f, (float)py, p.z + 0.5f);
        m.yaw = angle(m_rng);
        m.health = mobInfo(type).health;
        m.herd = herd;
        m.stateTimer = unit(m_rng) * 3.0f;
        m_mobs.push_back(m);
    }
}

// ------------------------------------------------------------------ AI

bool EntityManager::planTo(Mob& m, const World& world, glm::ivec3 goal)
{
    Pathfinder::Options opt;
    opt.height = (int)std::ceil(mobInfo(m.type).height);
    m.path = Pathfinder::find(world, blockOf(m.position + glm::vec3(0.0f, 0.1f, 0.0f)), goal, opt);
    m.pathIndex = 1;
    m.stuckTimer = 0.0f;
    return !m.path.empty();
}

void EntityManager::think(Mob& m, const World& world, const glm::vec3& player, float dt)
{
    const MobInfo& info = mobInfo(m.type);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    m.stateTimer -= dt;
    m.repathTimer -= dt;

    glm::vec3 away = m.position - player;
    away.y = 0.0f;
    float playerDist = glm::length(away);

    // Plaché zvieratá sa splašia, keď sa hráč priblíži.
    if (info.skittish && playerDist < FLEE_RADIUS && m.state != Mob::State::Flee) {
        m.state = Mob::State::Flee;
        m.stateTimer = 6.0f;
        m.repathTimer = 0.0f;
    }

    switch (m.state) {
    case Mob::State::Idle:
        if (m.stateTimer <= 0.0f) {
            // Ovečky sa držia stáda: ak sú ďaleko od ostatných, idú k nim.
            glm::vec3 target = m.position;
            if (m.type == MobType::Sheep) {
                glm::vec3 sum(0.0f);
                int n = 0;
                for (const Mob& o : m_mobs)
                    if (o.herd == m.herd && o.id != m.id) { sum += o.position; ++n; }
                if (n) target = sum / (float)n;
            }
            float r = m.type == MobType::Frog ? 4.0f : 9.0f;
            glm::vec3 goal = target + glm::vec3((unit(m_rng) - 0.5f) * 2.0f * r, 0.0f, (unit(m_rng) - 0.5f) * 2.0f * r);
            if (planTo(m, world, blockOf(goal + glm::vec3(0.0f, 0.5f, 0.0f)))) {
                m.state = Mob::State::Wander;
                m.stateTimer = 12.0f;
            } else {
                m.stateTimer = 1.0f + unit(m_rng) * 2.0f;
            }
        }
        break;

    case Mob::State::Wander:
        if (m.pathIndex >= m.path.size() || m.stateTimer <= 0.0f || m.stuckTimer > 2.0f) {
            m.state = Mob::State::Idle;
            m.stateTimer = 2.0f + unit(m_rng) * 5.0f;
            m.path.clear();
        }
        break;

    case Mob::State::Flee:
        // Bež od hráča; cestu priebežne prepočítavaj.
        if (m.repathTimer <= 0.0f || m.pathIndex >= m.path.size()) {
            glm::vec3 dir = playerDist > 0.01f ? away / playerDist : glm::vec3(1, 0, 0);
            float a = (unit(m_rng) - 0.5f) * 1.0f; // trochu do strany, aby neutekali rovno ako po šnúrke
            glm::vec3 rotated(dir.x * std::cos(a) - dir.z * std::sin(a), 0.0f, dir.x * std::sin(a) + dir.z * std::cos(a));
            planTo(m, world, blockOf(m.position + rotated * 14.0f + glm::vec3(0.0f, 0.5f, 0.0f)));
            m.repathTimer = 1.2f;
        }
        if (playerDist > SAFE_RADIUS && m.stateTimer <= 0.0f) {
            m.state = Mob::State::Idle;
            m.stateTimer = 1.0f + unit(m_rng) * 2.0f;
            m.path.clear();
        }
        break;
    }
}

void EntityManager::followPath(Mob& m, const World& world, float speed)
{
    const MobInfo& info = mobInfo(m.type);
    glm::vec2 wish(0.0f);
    bool wantJump = false;

    while (m.pathIndex < m.path.size()) {
        glm::ivec3 node = m.path[m.pathIndex];
        glm::vec3 target(node.x + 0.5f, (float)node.y, node.z + 0.5f);
        glm::vec2 to(target.x - m.position.x, target.z - m.position.z);
        if (glm::length(to) < 0.35f && std::abs(target.y - m.position.y) < 1.1f) { ++m.pathIndex; continue; }
        wish = glm::normalize(to);
        wantJump = node.y > (int)std::floor(m.position.y + 0.1f);
        break;
    }

    if (glm::length(wish) > 0.0f) {
        m.velocity.x = wish.x * speed;
        m.velocity.z = wish.y * speed;
        // otáčanie tela plynulo k smeru pohybu
        float targetYaw = std::atan2(wish.x, wish.y);
        float d = std::remainder(targetYaw - m.yaw, 6.2832f);
        m.yaw += d * 0.25f;
    } else {
        m.velocity.x *= 0.8f;
        m.velocity.z *= 0.8f;
    }

    // Skok: na vyšší blok, žaba skáče stále, keď sa hýbe.
    if (m.onGround && (wantJump || (info.hops && glm::length(wish) > 0.0f)))
        m.velocity.y = info.hops ? 7.5f : 8.6f;
    (void)world;
}

void EntityManager::update(const World& world, const TerrainGenerator& gen, const glm::vec3& player, float dt)
{
    dt = std::min(dt, 0.05f);

    m_spawnTimer -= dt;
    if (m_spawnTimer <= 0.0f) {
        trySpawn(world, gen, player);
        m_spawnTimer = 0.4f;
    }

    for (Mob& m : m_mobs) {
        const Chunk* chunk = world.getChunk(World::chunkCoordOf((int)std::floor(m.position.x), (int)std::floor(m.position.z)));
        if (!chunk) continue; // chunk ešte nie je – zviera "spí"

        const MobInfo& info = mobInfo(m.type);
        think(m, world, player, dt);
        float speed = m.state == Mob::State::Flee ? info.fleeSpeed : m.state == Mob::State::Wander ? info.walkSpeed : 0.0f;
        followPath(m, world, speed);

        // Fyzika (rovnaká ako u hráča)
        bool inWater = touchesLiquid(world, m.bounds());
        m.velocity.y = std::max(m.velocity.y - (inWater ? 8.0f : GRAVITY) * dt, -40.0f);
        if (inWater) m.velocity.y = std::max(m.velocity.y, 1.5f); // vo vode plávajú hore
        AABB box = m.bounds();
        glm::vec3 before = m.velocity;
        MoveResult r = moveAndCollide(world, box, m.velocity, dt);
        m.position = glm::vec3((box.min.x + box.max.x) * 0.5f, box.min.y, (box.min.z + box.max.z) * 0.5f);
        m.onGround = r.onGround;
        if (r.hitWall && glm::length(glm::vec2(before.x, before.z)) > 0.1f) m.stuckTimer += dt;

        float horizontal = glm::length(glm::vec2(m.velocity.x, m.velocity.z));
        m.walkPhase += horizontal * dt * 4.5f;
        m.hurtTimer = std::max(0.0f, m.hurtTimer - dt);
    }

    // Uhynuté a príliš vzdialené zmiznú.
    m_mobs.erase(std::remove_if(m_mobs.begin(), m_mobs.end(),
                                [&](const Mob& m) {
                                    glm::vec2 d(m.position.x - player.x, m.position.z - player.z);
                                    return m.health <= 0 || glm::length(d) > DESPAWN_DISTANCE || m.position.y < -10.0f;
                                }),
                 m_mobs.end());
}

// ------------------------------------------------------------------ zásahy

int EntityManager::raycast(const glm::vec3& o, const glm::vec3& dir, float maxDistance, float& distance) const
{
    int best = -1;
    distance = maxDistance;
    for (int i = 0; i < (int)m_mobs.size(); ++i) {
        AABB b = m_mobs[i].bounds();
        // Slab test
        float t0 = 0.0f, t1 = distance;
        bool hit = true;
        for (int a = 0; a < 3 && hit; ++a) {
            if (std::abs(dir[a]) < 1e-6f) {
                if (o[a] < b.min[a] || o[a] > b.max[a]) hit = false;
                continue;
            }
            float ta = (b.min[a] - o[a]) / dir[a], tb = (b.max[a] - o[a]) / dir[a];
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) hit = false;
        }
        if (hit && t0 < distance) {
            distance = t0;
            best = i;
        }
    }
    return best;
}

void EntityManager::hit(int index, const glm::vec3& fromDir)
{
    Mob& m = m_mobs[index];
    m.health -= 1;
    m.hurtTimer = 0.35f;
    glm::vec3 push = glm::normalize(glm::vec3(fromDir.x, 0.0f, fromDir.z) + glm::vec3(0.0001f));
    m.velocity = push * 7.0f + glm::vec3(0.0f, 6.0f, 0.0f);
    m.state = Mob::State::Flee;
    m.stateTimer = 8.0f;
    m.repathTimer = 0.2f;
    // Splaší aj zvyšok stáda.
    for (Mob& o : m_mobs)
        if (o.herd == m.herd && o.id != m.id && glm::length(o.position - m.position) < 16.0f) {
            o.state = Mob::State::Flee;
            o.stateTimer = 6.0f;
            o.repathTimer = 0.0f;
        }
}

} // namespace cubiq
