#include "Game/BlockHighlight.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

namespace cubiq {

BlockHighlight::BlockHighlight()
    : m_shader("assets/shaders/line.vert", "assets/shaders/line.frag")
{
    // 12 hrán jednotkovej kocky ako GL_LINES.
    const float e[] = {
        0,0,0, 1,0,0,  1,0,0, 1,0,1,  1,0,1, 0,0,1,  0,0,1, 0,0,0, // spodok
        0,1,0, 1,1,0,  1,1,0, 1,1,1,  1,1,1, 0,1,1,  0,1,1, 0,1,0, // vrch
        0,0,0, 0,1,0,  1,0,0, 1,1,0,  1,0,1, 1,1,1,  0,0,1, 0,1,1, // zvislé
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(e), e, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

BlockHighlight::~BlockHighlight()
{
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
}

void BlockHighlight::draw(const glm::ivec3& block, const glm::mat4& viewProjection)
{
    // Trošku väčší ako blok, aby čiary neblikali so stenami (z-fighting).
    const float grow = 0.003f;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(block) - glm::vec3(grow));
    model = glm::scale(model, glm::vec3(1.0f + 2.0f * grow));

    m_shader.bind();
    m_shader.setMat4("uMVP", viewProjection * model);
    m_shader.setVec4("uColor", glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));

    glEnable(GL_BLEND);
    glLineWidth(2.0f);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, 24);
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
}

} // namespace cubiq
