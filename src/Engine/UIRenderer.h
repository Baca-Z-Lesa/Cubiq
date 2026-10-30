#pragma once
#include "Engine/Shader.h"

#include <glm/glm.hpp>
#include <vector>

namespace cubiq {

class Texture;

// Jednoduché 2D UI: všetko sa za snímku nazbiera do jedného bufferu a nakreslí jedným draw callom.
// Súradnice sú v pixeloch, (0,0) vľavo hore.
class UIRenderer {
public:
    UIRenderer();
    ~UIRenderer();

    UIRenderer(const UIRenderer&) = delete;
    UIRenderer& operator=(const UIRenderer&) = delete;

    void begin(int width, int height);
    void end(const Texture& atlas);

    void rect(float x, float y, float w, float h, const glm::vec4& color);
    void rectOutline(float x, float y, float w, float h, float thickness, const glm::vec4& color);

    // Ľubovoľný štvoruholník (body v smere hodinových ručičiek od ľavého horného).
    void quad(const glm::vec2 p[4], const glm::vec2 uv[4], const glm::vec4& color, bool textured);

    // Číslo z pixelového písma 3×5. Vráti šírku v pixeloch.
    float number(int value, float x, float y, float pixel, const glm::vec4& color);
    static float numberWidth(int value, float pixel);

    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    struct Vertex {
        glm::vec2 pos;
        glm::vec2 uv;
        glm::vec4 color;
        float textured;
    };

    Shader m_shader;
    unsigned int m_vao = 0, m_vbo = 0;
    std::vector<Vertex> m_vertices;
    int m_width = 0, m_height = 0;
};

} // namespace cubiq
