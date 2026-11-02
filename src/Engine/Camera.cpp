#include "Engine/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace cubiq {

static const glm::vec3 WORLD_UP{0.0f, 1.0f, 0.0f};

Camera::Camera(const glm::vec3& position) : position(position) {}

void Camera::rotate(float yawDelta, float pitchDelta)
{
    yaw += yawDelta;
    pitch = glm::clamp(pitch + pitchDelta, -89.9f, 89.9f);
}

glm::vec3 Camera::forward() const
{
    float y = glm::radians(yaw), p = glm::radians(pitch);
    return glm::normalize(glm::vec3(cos(y) * cos(p), sin(p), sin(y) * cos(p)));
}

glm::vec3 Camera::right() const { return glm::normalize(glm::cross(forward(), WORLD_UP)); }

glm::mat4 Camera::view() const { return glm::lookAt(position, position + forward(), WORLD_UP); }

glm::mat4 Camera::projection(float aspect) const
{
    return glm::perspective(glm::radians(fov), aspect, 0.05f, 2500.0f); // ďaleko kvôli vzdialenému terénu
}

} // namespace cubiq
