#pragma once
#include "World/Block.h"

#include <glm/vec3.hpp>

namespace cubiq {

class World;

struct RaycastHit {
    bool hit = false;
    glm::ivec3 block{0};   // zasiahnutý blok
    glm::ivec3 normal{0};  // strana, ktorou lúč vošiel (blok sa postaví na block + normal)
    BlockId id = BlockId::Air;
    float distance = 0.0f;
};

// Prechádza mriežku blok po bloku (DDA, Amanatides & Woo) – nikdy nepreskočí roh ani hranu.
// Tekutiny ignoruje, takže sa dá kopať aj stavať cez vodu.
RaycastHit raycast(const World& world, const glm::vec3& origin, const glm::vec3& direction, float maxDistance);

} // namespace cubiq
