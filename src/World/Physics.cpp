#include "World/Physics.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    constexpr float EPS = 1e-4f;

    // Zavolá fn pre každý blok, do ktorého box zasahuje.
    template <typename F>
    bool anyBlock(const World& world, const AABB& box, F&& fn)
    {
        int x0 = (int)std::floor(box.min.x), x1 = (int)std::floor(box.max.x - EPS);
        int y0 = (int)std::floor(box.min.y), y1 = (int)std::floor(box.max.y - EPS);
        int z0 = (int)std::floor(box.min.z), z1 = (int)std::floor(box.max.z - EPS);
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x)
                    if (fn(world.getBlock(x, y, z))) return true;
        return false;
    }
}

bool collidesWithWorld(const World& world, const AABB& box)
{
    return anyBlock(world, box, [](BlockId id) { return isSolid(id); });
}

bool touchesLiquid(const World& world, const AABB& box)
{
    return anyBlock(world, box, [](BlockId id) { return isLiquid(id); });
}

MoveResult moveAndCollide(const World& world, AABB& box, glm::vec3& velocity, float dt)
{
    MoveResult result;
    const int axes[3] = {1, 0, 2};

    for (int axis : axes) {
        float delta = velocity[axis] * dt;
        if (delta == 0.0f) continue;

        // Kroky max. 0.45 bloku, aby rýchly pád neprešiel cez tenkú stenu.
        int steps = (int)std::ceil(std::fabs(delta) / 0.45f);
        float stepSize = delta / steps;

        for (int s = 0; s < steps; ++s) {
            box.min[axis] += stepSize;
            box.max[axis] += stepSize;
            if (!collidesWithWorld(world, box)) continue;

            // Narazili sme: zarovnať box k hranici bloku a zastaviť sa v tejto osi.
            float push = stepSize > 0.0f ? std::floor(box.max[axis] - EPS) - box.max[axis] - EPS
                                         : std::floor(box.min[axis]) + 1.0f - box.min[axis] + EPS;
            box.min[axis] += push;
            box.max[axis] += push;

            if (axis == 1) {
                if (stepSize < 0.0f) result.onGround = true;
                else result.hitCeiling = true;
            } else {
                result.hitWall = true;
            }
            velocity[axis] = 0.0f;
            break;
        }
    }
    return result;
}

} // namespace cubiq
