#pragma once
#include "Engine/Shader.h"

#include <glm/glm.hpp>

namespace cubiq {

// Čierny drôtený obrys bloku, na ktorý sa hráč pozerá, a praskliny počas kopania.
class BlockHighlight {
public:
    BlockHighlight();
    ~BlockHighlight();

    BlockHighlight(const BlockHighlight&) = delete;
    BlockHighlight& operator=(const BlockHighlight&) = delete;

    // progress 0..1 = ako ďaleko je kopanie (0 = bez prasklín).
    void draw(const glm::ivec3& block, const glm::mat4& viewProjection, float progress = 0.0f);

private:
    Shader m_shader;
    unsigned int m_vao = 0, m_vbo = 0;
    unsigned int m_crackVao = 0, m_crackVbo = 0;
};

} // namespace cubiq
