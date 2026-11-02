#include "Game/EntityRenderer.h"
#include "Game/Entities.h"
#include "Game/Sky.h"
#include "World/World.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace cubiq {

EntityRenderer::EntityRenderer() : m_shader("assets/shaders/entity.vert", "assets/shaders/entity.frag")
{
    // Jednotková kocka -0.5..0.5: 6 stien × 2 trojuholníky, pozícia + normála.
    const float v[] = {
        // +Z
        -.5f,-.5f,.5f, 0,0,1,  .5f,-.5f,.5f, 0,0,1,  .5f,.5f,.5f, 0,0,1,  -.5f,-.5f,.5f, 0,0,1,  .5f,.5f,.5f, 0,0,1,  -.5f,.5f,.5f, 0,0,1,
        // -Z
        .5f,-.5f,-.5f, 0,0,-1,  -.5f,-.5f,-.5f, 0,0,-1,  -.5f,.5f,-.5f, 0,0,-1,  .5f,-.5f,-.5f, 0,0,-1,  -.5f,.5f,-.5f, 0,0,-1,  .5f,.5f,-.5f, 0,0,-1,
        // +X
        .5f,-.5f,.5f, 1,0,0,  .5f,-.5f,-.5f, 1,0,0,  .5f,.5f,-.5f, 1,0,0,  .5f,-.5f,.5f, 1,0,0,  .5f,.5f,-.5f, 1,0,0,  .5f,.5f,.5f, 1,0,0,
        // -X
        -.5f,-.5f,-.5f, -1,0,0,  -.5f,-.5f,.5f, -1,0,0,  -.5f,.5f,.5f, -1,0,0,  -.5f,-.5f,-.5f, -1,0,0,  -.5f,.5f,.5f, -1,0,0,  -.5f,.5f,-.5f, -1,0,0,
        // +Y
        -.5f,.5f,.5f, 0,1,0,  .5f,.5f,.5f, 0,1,0,  .5f,.5f,-.5f, 0,1,0,  -.5f,.5f,.5f, 0,1,0,  .5f,.5f,-.5f, 0,1,0,  -.5f,.5f,-.5f, 0,1,0,
        // -Y
        -.5f,-.5f,-.5f, 0,-1,0,  .5f,-.5f,-.5f, 0,-1,0,  .5f,-.5f,.5f, 0,-1,0,  -.5f,-.5f,-.5f, 0,-1,0,  .5f,-.5f,.5f, 0,-1,0,  -.5f,-.5f,.5f, 0,-1,0,
    };
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

EntityRenderer::~EntityRenderer()
{
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
}

void EntityRenderer::draw(const EntityManager& entities, const World& world, const glm::mat4& viewProjection,
                          const glm::vec3& cameraPos, const SkyState& sky, const glm::vec3& fogColor, float fogStart,
                          float fogEnd)
{
    if (entities.mobs().empty()) return;
    m_shader.bind();
    m_shader.setMat4("uViewProj", viewProjection);
    m_shader.setVec3("uCameraPos", cameraPos);
    m_shader.setVec3("uSunDir", sky.sunDir);
    m_shader.setVec3("uFogColor", fogColor);
    m_shader.setFloat("uFogStart", fogStart);
    m_shader.setFloat("uFogEnd", fogEnd);
    glBindVertexArray(m_vao);
    glDisable(GL_CULL_FACE);

    for (const Mob& m : entities.mobs()) {
        if (glm::length(m.position - cameraPos) > fogEnd) continue;
        const MobInfo& info = mobInfo(m.type);

        // Jednoduché osvetlenie: pod strechou / v jaskyni je tma, inak svetlo oblohy.
        int bx = (int)std::floor(m.position.x), bz = (int)std::floor(m.position.z), by = (int)std::floor(m.position.y + info.height);
        bool covered = false;
        for (int y = by + 1; y < by + 24 && !covered; ++y) covered = isOpaque(world.getBlock(bx, y, bz));
        glm::vec3 skyLight = sky.sunColor * sky.sunBrightness * (covered ? 0.35f : 1.0f);
        m_shader.setVec3("uLight", glm::max(skyLight, glm::vec3(0.06f)));
        m_shader.setFloat("uHurt", m.hurtTimer > 0.0f ? 1.0f : 0.0f);

        // Mátoha sa jemne vznáša.
        const float hover = info.hostile ? 0.12f + 0.08f * std::sin(m.walkPhase * 0.7f + m.id * 1.7f) : 0.0f;
        glm::mat4 base = glm::translate(glm::mat4(1.0f), m.position + glm::vec3(0.0f, hover, 0.0f));
        base = glm::rotate(base, m.yaw, glm::vec3(0, 1, 0));
        const float swing = std::sin(m.walkPhase) * 0.7f;

        for (const ModelPart& part : info.parts) {
            glm::mat4 model = base;
            if (part.swing == 1 || part.swing == -1) {
                // Noha sa kýve okolo horného konca.
                glm::vec3 pivot = part.center + glm::vec3(0.0f, part.size.y * 0.5f, 0.0f);
                model = glm::translate(model, pivot);
                model = glm::rotate(model, swing * (float)part.swing, glm::vec3(1, 0, 0));
                model = glm::translate(model, part.center - pivot);
            } else if (part.swing == 2) {
                // Hlava jemne pokyvuje (pri chôdzi viac).
                float nod = std::sin(m.walkPhase * 0.5f) * 0.08f;
                model = glm::translate(model, part.center);
                model = glm::rotate(model, nod, glm::vec3(1, 0, 0));
            } else {
                model = glm::translate(model, part.center);
            }
            model = glm::scale(model, part.size);
            m_shader.setMat4("uModel", model);
            m_shader.setVec3("uColor", part.color);
            m_shader.setFloat("uEmissive", part.swing == 3 ? 1.0f : 0.0f);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }
    glEnable(GL_CULL_FACE);
}

} // namespace cubiq
