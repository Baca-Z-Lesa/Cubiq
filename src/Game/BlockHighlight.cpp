#include "Game/BlockHighlight.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace cubiq {

namespace {
    // Praskliny: na každej stene BRANCHES vetiev od stredu, každá STEPS úsečiek.
    // Úsečky sú zoradené po krokoch, takže prvých N úsečiek = praskliny rastú od stredu.
    constexpr int BRANCHES = 5, STEPS = 3, SEGMENTS_PER_FACE = BRANCHES * STEPS;
}

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

    // Praskliny pre 6 stien (pseudonáhodné, ale vždy rovnaké).
    const glm::vec3 origin[6] = {{0, 0, 1}, {1, 0, 0}, {1, 0, 1}, {0, 0, 0}, {0, 1, 1}, {0, 0, 0}};
    const glm::vec3 axisU[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}};
    const glm::vec3 axisV[6] = {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    uint32_t rng = 12345u;
    auto rnd = [&] { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0f; };
    std::vector<float> cracks;
    for (int f = 0; f < 6; ++f) {
        glm::vec2 tip[BRANCHES];
        float angle[BRANCHES];
        for (int b = 0; b < BRANCHES; ++b) {
            tip[b] = glm::vec2(0.5f + (rnd() - 0.5f) * 0.15f, 0.5f + (rnd() - 0.5f) * 0.15f);
            angle[b] = (b + rnd() * 0.6f) * 6.2832f / BRANCHES;
        }
        for (int step = 0; step < STEPS; ++step) {
            for (int b = 0; b < BRANCHES; ++b) {
                angle[b] += (rnd() - 0.5f) * 1.1f;
                glm::vec2 next = tip[b] + glm::vec2(std::cos(angle[b]), std::sin(angle[b])) * (0.12f + rnd() * 0.08f);
                next = glm::clamp(next, glm::vec2(0.02f), glm::vec2(0.98f));
                for (glm::vec2 q : {tip[b], next}) {
                    glm::vec3 p = origin[f] + axisU[f] * q.x + axisV[f] * q.y;
                    cracks.insert(cracks.end(), {p.x, p.y, p.z});
                }
                tip[b] = next;
            }
        }
    }
    glGenVertexArrays(1, &m_crackVao);
    glGenBuffers(1, &m_crackVbo);
    glBindVertexArray(m_crackVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_crackVbo);
    glBufferData(GL_ARRAY_BUFFER, cracks.size() * sizeof(float), cracks.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

BlockHighlight::~BlockHighlight()
{
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_crackVbo);
    glDeleteVertexArrays(1, &m_crackVao);
}

void BlockHighlight::draw(const glm::ivec3& block, const glm::mat4& viewProjection, float progress)
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

    if (progress > 0.0f) {
        const int segments = std::min(SEGMENTS_PER_FACE, (int)std::ceil(progress * SEGMENTS_PER_FACE));
        m_shader.setVec4("uColor", glm::vec4(0.05f, 0.04f, 0.03f, 0.55f + 0.35f * progress));
        glBindVertexArray(m_crackVao);
        for (int f = 0; f < 6; ++f) glDrawArrays(GL_LINES, f * SEGMENTS_PER_FACE * 2, segments * 2);
    }
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
}

} // namespace cubiq
