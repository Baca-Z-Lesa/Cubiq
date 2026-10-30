#pragma once
#include <glm/glm.hpp>
#include <string>

namespace cubiq {

class UIRenderer;

// Jednoduché "immediate mode" UI pre menu: prvok sa nakreslí a hneď vráti, či naň hráč klikol.
class Gui {
public:
    Gui(UIRenderer& ui, glm::vec2 mouse, bool clicked);

    float scale() const { return m_scale; }
    float width() const;
    float height() const;

    bool button(const char* label, float x, float y, float w, float h, bool enabled = true);
    // Riadok zoznamu (napr. svet): zvýraznený, ak je vybraný. Vráti true pri kliknutí.
    bool listItem(const char* title, const char* subtitle, float x, float y, float w, float h, bool selected);
    // Textové pole; ak je aktívne, prijíma písanie. Vráti true, keď naň hráč klikol.
    bool textField(std::string& text, const char* placeholder, float x, float y, float w, float h, bool focused,
                   size_t maxLength);

    void panel(float x, float y, float w, float h, float alpha = 0.55f);
    void text(const char* s, float x, float y, float size, const glm::vec4& color, bool centered = false, bool shadow = true);
    void dim(float alpha);

private:
    bool hover(float x, float y, float w, float h) const;

    UIRenderer& m_ui;
    glm::vec2 m_mouse;
    bool m_clicked;
    float m_scale;
};

} // namespace cubiq
