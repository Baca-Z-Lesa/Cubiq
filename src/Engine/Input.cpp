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
    float g_scroll = 0.0f;
    std::string g_typed;
    int g_backspaces = 0;

    void scrollCallback(GLFWwindow*, double, double y) { g_scroll += (float)y; }

    void charCallback(GLFWwindow*, unsigned int codepoint)
    {
        if (codepoint >= 32 && codepoint < 127) g_typed += (char)codepoint;
    }

    void keyCallback(GLFWwindow*, int key, int, int action, int)
    {
        if (key >= 0 && key <= GLFW_KEY_LAST && action == GLFW_PRESS)
            g_keyPressed[key] = true;
        if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT))
            ++g_backspaces;
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
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetCharCallback(window, charCallback);
}

const std::string& Input::typedText() { return g_typed; }
int Input::backspaces() { return g_backspaces; }

void Input::beginFrame()
{
    g_keyPressed.fill(false);
    g_mousePressed.fill(false);
    g_mouseDelta = glm::vec2(0.0f);
    g_scroll = 0.0f;
    g_typed.clear();
    g_backspaces = 0;
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
float Input::scrollDelta() { return g_scroll; }

glm::vec2 Input::mousePosition()
{
    double x, y;
    glfwGetCursorPos(g_window, &x, &y);
    return {(float)x, (float)y};
}

} // namespace cubiq
