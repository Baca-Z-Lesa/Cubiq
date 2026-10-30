#include "Engine/Window.h"
#include "Engine/Input.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace cubiq {

Window::Window(int width, int height, const std::string& title)
    : m_width(width), m_height(height)
{
    if (!glfwInit())
        throw std::runtime_error("Nepodarilo sa inicializovat GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window)
        throw std::runtime_error("Nepodarilo sa vytvorit okno (OpenGL 3.3?)");

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // VSync

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
        throw std::runtime_error("Nepodarilo sa nacitat OpenGL funkcie (GLEW)");

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, onResize);
    Input::attach(m_window);

    int fbw, fbh;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
}

Window::~Window()
{
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }
void Window::close() { glfwSetWindowShouldClose(m_window, GLFW_TRUE); }
void Window::swapBuffers() { glfwSwapBuffers(m_window); }

void Window::pollEvents()
{
    Input::beginFrame();
    glfwPollEvents();
}

void Window::setTitle(const std::string& title) { glfwSetWindowTitle(m_window, title.c_str()); }

void Window::setCursorLocked(bool locked)
{
    m_cursorLocked = locked;
    glfwSetInputMode(m_window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (locked && glfwRawMouseMotionSupported())
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    Input::resetMouseDelta();
}

void Window::toggleFullscreen()
{
    if (!m_fullscreen) {
        glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
        glfwGetWindowSize(m_window, &m_windowedW, &m_windowedH);

        // Monitor, na ktorom leží stred okna (nie vždy hlavný).
        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        GLFWmonitor* target = glfwGetPrimaryMonitor();
        int cx = m_windowedX + m_windowedW / 2, cy = m_windowedY + m_windowedH / 2;
        for (int i = 0; i < count; ++i) {
            int mx, my;
            glfwGetMonitorPos(monitors[i], &mx, &my);
            const GLFWvidmode* m = glfwGetVideoMode(monitors[i]);
            if (cx >= mx && cx < mx + m->width && cy >= my && cy < my + m->height) target = monitors[i];
        }

        const GLFWvidmode* mode = glfwGetVideoMode(target);
        glfwSetWindowMonitor(m_window, target, 0, 0, mode->width, mode->height, mode->refreshRate);
        m_fullscreen = true;
    } else {
        glfwSetWindowMonitor(m_window, nullptr, m_windowedX, m_windowedY, m_windowedW, m_windowedH, 0);
        m_fullscreen = false;
    }
    glfwSwapInterval(1); // niektoré ovládače pri prepnutí VSync zrušia
}

void Window::onResize(GLFWwindow* window, int width, int height)
{
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    self->m_width = width;
    self->m_height = height;
    glViewport(0, 0, width, height);
}

} // namespace cubiq
