#pragma once
#include <glm/vec2.hpp>

struct GLFWwindow;

namespace cubiq {

// Globálny stav klávesnice a myši. Kódy kláves sú GLFW_KEY_*.
class Input {
public:
    static void attach(GLFWwindow* window);
    static void beginFrame();
    static void resetMouseDelta();

    static bool isKeyDown(int key);
    static bool wasKeyPressed(int key);   // len v snímke, keď bola stlačená
    static bool isMouseDown(int button);
    static bool wasMousePressed(int button);
    static glm::vec2 mouseDelta();
};

} // namespace cubiq
