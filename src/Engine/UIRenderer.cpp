#include "Engine/UIRenderer.h"
#include "Engine/Texture.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace cubiq {

namespace {
    // Číslice 0–9, každá 3×5 pixelov, riadok = 3 bity (bit 2 = ľavý stĺpec).
    const unsigned char DIGITS[10][5] = {
        {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
        {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
    };
}

UIRenderer::UIRenderer()
    : m_shader("assets/shaders/ui.vert", "assets/shaders/ui.frag")
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, textured));
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);
}

UIRenderer::~UIRenderer()
{
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
}

void UIRenderer::begin(int width, int height)
{
    m_width = width;
    m_height = height;
    m_vertices.clear();
}

void UIRenderer::end(const Texture& atlas)
{
    if (m_vertices.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_STREAM_DRAW);

    m_shader.bind();
    m_shader.setMat4("uProjection", glm::ortho(0.0f, (float)m_width, (float)m_height, 0.0f));
    m_shader.setInt("uAtlas", 0);
    atlas.bind(0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_vertices.size());
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void UIRenderer::quad(const glm::vec2 p[4], const glm::vec2 uv[4], const glm::vec4& color, bool textured)
{
    float t = textured ? 1.0f : 0.0f;
    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int i : order) m_vertices.push_back({p[i], uv[i], color, t});
}

void UIRenderer::rect(float x, float y, float w, float h, const glm::vec4& color)
{
    const glm::vec2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    const glm::vec2 uv[4] = {};
    quad(p, uv, color, false);
}

void UIRenderer::rectOutline(float x, float y, float w, float h, float t, const glm::vec4& color)
{
    rect(x, y, w, t, color);
    rect(x, y + h - t, w, t, color);
    rect(x, y + t, t, h - 2 * t, color);
    rect(x + w - t, y + t, t, h - 2 * t, color);
}

namespace {
    // Pixelové písmo 5×7 pre ASCII 32–95 (malé písmená sa kreslia ako veľké).
    // Každý riadok = 5 bitov, bit 4 je ľavý stĺpec.
    const unsigned char FONT[64][7] = {
        {0, 0, 0, 0, 0, 0, 0},                                             // ' '
        {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0, 0b00100},         // !
        {0b01010, 0b01010, 0b01010, 0, 0, 0, 0},                           // "
        {0b01010, 0b01010, 0b11111, 0b01010, 0b11111, 0b01010, 0b01010},   // #
        {0b00100, 0b01111, 0b10100, 0b01110, 0b00101, 0b11110, 0b00100},   // $
        {0b11000, 0b11001, 0b00010, 0b00100, 0b01000, 0b10011, 0b00011},   // %
        {0b01100, 0b10010, 0b10100, 0b01000, 0b10101, 0b10010, 0b01101},   // &
        {0b00100, 0b00100, 0b01000, 0, 0, 0, 0},                           // '
        {0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010},   // (
        {0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000},   // )
        {0, 0b00100, 0b10101, 0b01110, 0b10101, 0b00100, 0},               // *
        {0, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0},               // +
        {0, 0, 0, 0, 0b01100, 0b00100, 0b01000},                           // ,
        {0, 0, 0, 0b11111, 0, 0, 0},                                       // -
        {0, 0, 0, 0, 0, 0b01100, 0b01100},                                 // .
        {0b00001, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000},   // /
        {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110},   // 0
        {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},   // 1
        {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111},   // 2
        {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110},   // 3
        {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},   // 4
        {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110},   // 5
        {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},   // 6
        {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},   // 7
        {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},   // 8
        {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100},   // 9
        {0, 0b01100, 0b01100, 0, 0b01100, 0b01100, 0},                     // :
        {0, 0b01100, 0b01100, 0, 0b01100, 0b00100, 0b01000},               // ;
        {0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010},   // <
        {0, 0, 0b11111, 0, 0b11111, 0, 0},                                 // =
        {0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000},   // >
        {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0, 0b00100},         // ?
        {0b01110, 0b10001, 0b10111, 0b10101, 0b10111, 0b10000, 0b01110},   // @
        {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},   // A
        {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110},   // B
        {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110},   // C
        {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110},   // D
        {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111},   // E
        {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000},   // F
        {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111},   // G
        {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},   // H
        {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},   // I
        {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100},   // J
        {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001},   // K
        {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111},   // L
        {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001},   // M
        {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001},   // N
        {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},   // O
        {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000},   // P
        {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101},   // Q
        {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001},   // R
        {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110},   // S
        {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100},   // T
        {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},   // U
        {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100},   // V
        {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010},   // W
        {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001},   // X
        {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100},   // Y
        {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111},   // Z
        {0b01110, 0b01000, 0b01000, 0b01000, 0b01000, 0b01000, 0b01110},   // [
        {0b10000, 0b10000, 0b01000, 0b00100, 0b00010, 0b00001, 0b00001},   // '\'
        {0b01110, 0b00010, 0b00010, 0b00010, 0b00010, 0b00010, 0b01110},   // ]
        {0b00100, 0b01010, 0b10001, 0, 0, 0, 0},                           // ^
        {0, 0, 0, 0, 0, 0, 0b11111},                                       // _
    };

    const unsigned char* glyph(char c)
    {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c < 32 || c > 95) c = '?';
        return FONT[c - 32];
    }

    constexpr float GLYPH_ADVANCE = 6.0f; // 5 px znak + 1 px medzera
}

void UIRenderer::text(const char* str, float x, float y, float scale, const glm::vec4& color)
{
    float cx = x;
    for (const char* s = str; *s; ++s) {
        if (*s == '\n') {
            cx = x;
            y += lineHeight(scale);
            continue;
        }
        const unsigned char* g = glyph(*s);
        for (int row = 0; row < 7; ++row) {
            // Súvislé úseky pixelov v riadku spojíme do jedného obdĺžnika.
            int col = 0;
            while (col < 5) {
                if (!(g[row] & (0x10 >> col))) { ++col; continue; }
                int start = col;
                while (col < 5 && (g[row] & (0x10 >> col))) ++col;
                rect(cx + start * scale, y + row * scale, (col - start) * scale, scale, color);
            }
        }
        cx += GLYPH_ADVANCE * scale;
    }
}

float UIRenderer::textWidth(const char* str, float scale)
{
    return std::strlen(str) * GLYPH_ADVANCE * scale - scale;
}

float UIRenderer::numberWidth(int value, float pixel)
{
    return std::to_string(value).size() * 4.0f * pixel - pixel;
}

float UIRenderer::number(int value, float x, float y, float pixel, const glm::vec4& color)
{
    std::string s = std::to_string(value);
    float cx = x;
    for (char ch : s) {
        const unsigned char* d = DIGITS[ch - '0'];
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col)
                if (d[row] & (4 >> col))
                    rect(cx + col * pixel, y + row * pixel, pixel, pixel, color);
        cx += 4.0f * pixel;
    }
    return cx - x - pixel;
}

} // namespace cubiq
