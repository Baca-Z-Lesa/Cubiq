#pragma once
#include "World/Block.h"

#include <glm/vec3.hpp>

namespace cubiq {

class World;

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    bool intersects(const AABB& o) const
    {
        return min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y &&
               min.z < o.max.z && max.z > o.min.z;
    }

    static AABB block(const glm::ivec3& b) { return {glm::vec3(b), glm::vec3(b) + glm::vec3(1.0f)}; }
};

// Prekrýva sa box s nejakým pevným blokom?
bool collidesWithWorld(const World& world, const AABB& box);

// Je v boxe aspoň jeden blok tekutiny?
bool touchesLiquid(const World& world, const AABB& box);

struct MoveResult {
    bool onGround = false;   // narazil zospodu (stojí na zemi)
    bool hitCeiling = false;
    bool hitWall = false;
};

// Posunie box o velocity * dt a vyrieši kolízie postupne po osiach (Y, X, Z),
// takže sa dá kĺzať popri stenách. Zablokovaná zložka rýchlosti sa vynuluje.
MoveResult moveAndCollide(const World& world, AABB& box, glm::vec3& velocity, float dt);

} // namespace cubiq
