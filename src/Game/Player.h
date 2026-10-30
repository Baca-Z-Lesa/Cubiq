#pragma once
#include "World/Physics.h"

#include <glm/glm.hpp>

namespace cubiq {

class Camera;
class World;

// Hráč s fyzikou: pozícia = stred chodidiel, telo 0.6 × 1.8 bloku.
class Player {
public:
    static constexpr float WIDTH = 0.6f;
    static constexpr float HEIGHT = 1.8f;
    static constexpr float EYE_HEIGHT = 1.62f;
    static constexpr float SNEAK_EYE_HEIGHT = 1.32f;

    // Pohyb na túto snímku (z klávesnice). forward/right sú horizontálne smery kamery.
    struct Controls {
        glm::vec2 move{0.0f}; // x = doprava, y = dopredu
        bool jump = false;    // Space (držané)
        bool sneak = false;   // Shift
        bool sprint = false;  // Ctrl
    };

    void update(const World& world, const Controls& controls, const glm::vec3& forward, const glm::vec3& right,
                float dt);

    void teleport(const glm::vec3& feet);
    void toggleFlying() { flying = !flying; velocity = glm::vec3(0.0f); }

    glm::vec3 eyePosition() const;
    AABB bounds() const;

    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    bool onGround = false;
    bool inWater = false;
    bool flying = false;
    bool sprinting = false;
    bool sneaking = false;

private:
    float m_eyeOffset = EYE_HEIGHT; // plynulé prikrčenie
};

} // namespace cubiq
