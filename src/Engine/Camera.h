#pragma once
#include <glm/glm.hpp>

namespace cubiq {

// FPS kamera (yaw/pitch v stupňoch).
class Camera {
public:
    explicit Camera(const glm::vec3& position = {0, 0, 0});

    void rotate(float yawDelta, float pitchDelta);

    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;

    glm::vec3 forward() const;
    glm::vec3 right() const;

    glm::vec3 position;
    float yaw = -90.0f;   // -90 = pozerá sa do -Z
    float pitch = 0.0f;
    float fov = 70.0f;
};

} // namespace cubiq
