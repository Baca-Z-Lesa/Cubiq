#pragma once
#include "Engine/Shader.h"

#include <glm/glm.hpp>

namespace cubiq {

class EntityManager;
class World;
struct SkyState;

// Kreslí zvieratá ako kockaté modely (každá časť = posunutá, otočená a zväčšená kocka).
class EntityRenderer {
public:
    EntityRenderer();
    ~EntityRenderer();

    EntityRenderer(const EntityRenderer&) = delete;
    EntityRenderer& operator=(const EntityRenderer&) = delete;

    void draw(const EntityManager& entities, const World& world, const glm::mat4& viewProjection,
              const glm::vec3& cameraPos, const SkyState& sky, const glm::vec3& fogColor, float fogStart, float fogEnd);

private:
    Shader m_shader;
    unsigned int m_vao = 0, m_vbo = 0;
};

} // namespace cubiq
