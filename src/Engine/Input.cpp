#include "Engine/Input.h"

#include <GLFW/glfw3.h>
#include <array>

namespace cubiq {

namespace {
    GLFWwindow* g_window = nullptr;
    std::array<bool, GLFW_KEY_LAST + 1> g_keyPressed{};
    std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> g_mousePressed{};
    glm::vec2 g_lastMouse{0.0f};
    glm::vec2 g_mouseDelta{0.0f};
    bool g_firstMouse = true;

    void keyCallback(GLFWwindow*, int key, int, int action, int)
    {
        if (key >= 0 && key <= GLFW_KEY_LAST && action == GLFW_PRESS)
            g_keyPressed[key] = true;
    }

    void mouseButtonCallback(GLFWwindow*, int button, int action, int)
    {
        if (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST && action == GLFW_PRESS)
            g_mousePressed[button] = true;
    }

    void cursorCallback(GLFWwindow*, double x, double y)
    {
        glm::vec2 pos{(float)x, (float)y};
        if (g_firstMouse) {
            g_lastMouse = pos;
            g_firstMouse = false;
        }
        g_mouseDelta += pos - g_lastMouse;
        g_lastMouse = pos;
    }
}

void Input::attach(GLFWwindow* window)
{
    g_window = window;
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
}

void Input::beginFrame()
{
    g_keyPressed.fill(false);
    g_mousePressed.fill(false);
    g_mouseDelta = glm::vec2(0.0f);
}

void Input::resetMouseDelta()
{
    g_firstMouse = true;
    g_mouseDelta = glm::vec2(0.0f);
}

bool Input::isKeyDown(int key) { return glfwGetKey(g_window, key) == GLFW_PRESS; }
bool Input::wasKeyPressed(int key) { return g_keyPressed[key]; }
bool Input::isMouseDown(int button) { return glfwGetMouseButton(g_window, button) == GLFW_PRESS; }
bool Input::wasMousePressed(int button) { return g_mousePressed[button]; }
glm::vec2 Input::mouseDelta() { return g_mouseDelta; }

} // namespace cubiq
