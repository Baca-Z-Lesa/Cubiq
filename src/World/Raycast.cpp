#include "World/Raycast.h"
#include "World/World.h"

#include <glm/geometric.hpp>
#include <cmath>
#include <limits>

namespace cubiq {

RaycastHit raycast(const World& world, const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
    const float INF = std::numeric_limits<float>::infinity();
    glm::vec3 dir = glm::normalize(direction);

    glm::ivec3 block((int)std::floor(origin.x), (int)std::floor(origin.y), (int)std::floor(origin.z));
    glm::ivec3 step(dir.x > 0 ? 1 : -1, dir.y > 0 ? 1 : -1, dir.z > 0 ? 1 : -1);

    // tDelta: o koľko sa posunie t pri prechode cez celý blok v danej osi.
    // tMax: t, pri ktorom lúč prvýkrát pretne hranicu bloku v danej osi.
    glm::vec3 tDelta, tMax;
    for (int a = 0; a < 3; ++a) {
        if (dir[a] == 0.0f) {
            tDelta[a] = INF;
            tMax[a] = INF;
        } else {
            tDelta[a] = std::fabs(1.0f / dir[a]);
            float boundary = step[a] > 0 ? block[a] + 1.0f : (float)block[a];
            tMax[a] = (boundary - origin[a]) / dir[a];
        }
    }

    RaycastHit result;
    float t = 0.0f;
    glm::ivec3 normal(0);

    while (t <= maxDistance) {
        BlockId id = world.getBlock(block.x, block.y, block.z);
        if (id != BlockId::Air && !isLiquid(id)) {
            result.hit = true;
            result.block = block;
            result.normal = normal;
            result.id = id;
            result.distance = t;
            return result;
        }

        // Posun do najbližšieho bloku – v osi, ktorej hranicu lúč pretne skôr.
        int axis = tMax.x < tMax.y ? (tMax.x < tMax.z ? 0 : 2) : (tMax.y < tMax.z ? 1 : 2);
        t = tMax[axis];
        tMax[axis] += tDelta[axis];
        block[axis] += step[axis];
        normal = glm::ivec3(0);
        normal[axis] = -step[axis];
    }
    return result;
}

} // namespace cubiq
