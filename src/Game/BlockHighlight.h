#pragma once
#include "Engine/Shader.h"

#include <glm/glm.hpp>

namespace cubiq {

// Čierny drôtený obrys bloku, na ktorý sa hráč pozerá.
class BlockHighlight {
public:
    BlockHighlight();
    ~BlockHighlight();

    BlockHighlight(const BlockHighlight&) = delete;
    BlockHighlight& operator=(const BlockHighlight&) = delete;

    void draw(const glm::ivec3& block, const glm::mat4& viewProjection);

private:
    Shader m_shader;
    unsigned int m_vao = 0, m_vbo = 0;
};

} // namespace cubiq
