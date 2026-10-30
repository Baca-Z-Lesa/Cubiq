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

private:
    static void onResize(GLFWwindow* window, int width, int height);

    GLFWwindow* m_window = nullptr;
    int m_width, m_height;
    bool m_cursorLocked = false;
};

} // namespace cubiq
