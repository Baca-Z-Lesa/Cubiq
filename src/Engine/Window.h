#pragma once
#include <string>

struct GLFWwindow;

namespace cubiq {

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void close();
    void swapBuffers();
    void pollEvents();

    int width() const { return m_width; }
    int height() const { return m_height; }
    float aspect() const { return m_height > 0 ? (float)m_width / m_height : 1.0f; }
    GLFWwindow* handle() const { return m_window; }

    void setTitle(const std::string& title);
    void setCursorLocked(bool locked);
    bool isCursorLocked() const { return m_cursorLocked; }

    // Celá obrazovka na monitore, kde je okno; späť do okna na pôvodnom mieste.
    void toggleFullscreen();
    bool isFullscreen() const { return m_fullscreen; }

private:
    static void onResize(GLFWwindow* window, int width, int height);

    GLFWwindow* m_window = nullptr;
    int m_width, m_height;
    bool m_cursorLocked = false;
    bool m_fullscreen = false;
    int m_windowedX = 100, m_windowedY = 100, m_windowedW = 1280, m_windowedH = 720;
};

} // namespace cubiq
