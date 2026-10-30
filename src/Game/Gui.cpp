#include "Game/Gui.h"
#include "Engine/Input.h"
#include "Engine/UIRenderer.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    const glm::vec4 WHITE(1.0f), GRAY(0.62f, 0.62f, 0.66f, 1.0f), SHADOW(0.0f, 0.0f, 0.0f, 0.65f);
    const glm::vec4 BUTTON(0.16f, 0.17f, 0.20f, 0.88f), BUTTON_HOVER(0.30f, 0.42f, 0.30f, 0.95f);
    const glm::vec4 BUTTON_OFF(0.12f, 0.12f, 0.13f, 0.6f), BORDER(1.0f, 1.0f, 1.0f, 0.22f);
    const glm::vec4 ACCENT(0.55f, 0.85f, 0.45f, 1.0f);
}

Gui::Gui(UIRenderer& ui, glm::vec2 mouse, bool clicked)
    : m_ui(ui), m_mouse(mouse), m_clicked(clicked), m_scale(std::max(1.0f, std::round(ui.height() / 360.0f)))
{
}

float Gui::width() const { return (float)m_ui.width(); }
float Gui::height() const { return (float)m_ui.height(); }

bool Gui::hover(float x, float y, float w, float h) const
{
    return m_mouse.x >= x && m_mouse.x < x + w && m_mouse.y >= y && m_mouse.y < y + h;
}

void Gui::text(const char* s, float x, float y, float size, const glm::vec4& color, bool centered, bool shadow)
{
    if (centered) x -= UIRenderer::textWidth(s, size) * 0.5f;
    float off = std::min(size, 2.0f * m_scale); // pri veľkom texte len jemný tieň
    if (shadow) m_ui.text(s, x + off, y + off, size, SHADOW);
    m_ui.text(s, x, y, size, color);
}

void Gui::panel(float x, float y, float w, float h, float alpha)
{
    m_ui.rect(x, y, w, h, glm::vec4(0.05f, 0.06f, 0.08f, alpha));
    m_ui.rectOutline(x, y, w, h, m_scale, BORDER);
}

void Gui::dim(float alpha) { m_ui.rect(0, 0, width(), height(), glm::vec4(0, 0, 0, alpha)); }

bool Gui::button(const char* label, float x, float y, float w, float h, bool enabled)
{
    bool over = enabled && hover(x, y, w, h);
    m_ui.rect(x, y, w, h, !enabled ? BUTTON_OFF : over ? BUTTON_HOVER : BUTTON);
    m_ui.rectOutline(x, y, w, h, m_scale, over ? glm::vec4(ACCENT.r, ACCENT.g, ACCENT.b, 0.9f) : BORDER);
    // Dlhý popis sa zmenší, aby sa zmestil do tlačidla.
    float size = m_scale * 2.0f;
    if (UIRenderer::textWidth(label, size) > w - 8.0f * m_scale) size = m_scale;
    text(label, x + w * 0.5f, y + (h - 7.0f * size) * 0.5f, size, enabled ? WHITE : GRAY, true);
    return over && m_clicked;
}

bool Gui::listItem(const char* title, const char* subtitle, float x, float y, float w, float h, bool selected)
{
    bool over = hover(x, y, w, h);
    glm::vec4 bg = selected ? glm::vec4(0.25f, 0.38f, 0.24f, 0.9f) : over ? glm::vec4(0.2f, 0.22f, 0.26f, 0.85f)
                                                                            : glm::vec4(0.1f, 0.11f, 0.13f, 0.75f);
    m_ui.rect(x, y, w, h, bg);
    if (selected) m_ui.rectOutline(x, y, w, h, m_scale, ACCENT);
    float pad = 4.0f * m_scale;
    text(title, x + pad, y + pad, m_scale * 2.0f, WHITE);
    text(subtitle, x + pad, y + pad + 18.0f * m_scale, m_scale, GRAY, false, false);
    return over && m_clicked;
}

bool Gui::textField(std::string& value, const char* placeholder, float x, float y, float w, float h, bool focused,
                    size_t maxLength)
{
    if (focused) {
        for (char c : Input::typedText())
            if (value.size() < maxLength) value += c;
        for (int i = 0; i < Input::backspaces() && !value.empty(); ++i) value.pop_back();
    }

    m_ui.rect(x, y, w, h, glm::vec4(0.0f, 0.0f, 0.0f, 0.75f));
    m_ui.rectOutline(x, y, w, h, m_scale, focused ? ACCENT : BORDER);
    float size = m_scale * 2.0f, pad = 4.0f * m_scale;
    float ty = y + (h - 7.0f * size) * 0.5f;
    if (value.empty() && !focused) {
        text(placeholder, x + pad, ty, size, GRAY, false, false);
    } else {
        text(value.c_str(), x + pad, ty, size, WHITE, false, false);
        // Blikajúci kurzor
        if (focused && std::fmod(glfwGetTime(), 1.0) < 0.55) {
            float cx = x + pad + (value.empty() ? 0.0f : UIRenderer::textWidth(value.c_str(), size) + size);
            m_ui.rect(cx, ty - size, size, 9.0f * size, WHITE);
        }
    }
    return hover(x, y, w, h) && m_clicked;
}

} // namespace cubiq
