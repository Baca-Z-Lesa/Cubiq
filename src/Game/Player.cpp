#include "Game/Player.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    // Hodnoty zhruba ako v Minecrafte (bloky / s).
    constexpr float GRAVITY = 32.0f;
    constexpr float TERMINAL_VELOCITY = 60.0f;
    constexpr float JUMP_VELOCITY = 9.0f;     // ~1.25 bloku vysoko
    constexpr float WALK_SPEED = 4.3f;
    constexpr float SPRINT_SPEED = 5.6f;
    constexpr float SNEAK_SPEED = 1.3f;
    constexpr float FLY_SPEED = 11.0f;
    constexpr float FLY_SPRINT_SPEED = 25.0f;

    constexpr float GROUND_ACCEL = 60.0f;     // ako rýchlo sa dosiahne cieľová rýchlosť
    constexpr float AIR_ACCEL = 12.0f;

    constexpr float WATER_GRAVITY = 6.0f;
    constexpr float WATER_SWIM_UP = 22.0f;
    constexpr float WATER_MAX_UP = 4.0f;
    constexpr float WATER_DRAG = 3.0f;        // spomalenie vo vode (exponenciálne)
    constexpr float WATER_SPEED_FACTOR = 0.55f;

    // Posun hodnoty k cieľu najviac o maxDelta.
    glm::vec2 approach(glm::vec2 current, glm::vec2 target, float maxDelta)
    {
        glm::vec2 d = target - current;
        float len = glm::length(d);
        if (len <= maxDelta || len == 0.0f) return target;
        return current + d / len * maxDelta;
    }
}

void Player::teleport(const glm::vec3& feet)
{
    position = feet;
    velocity = glm::vec3(0.0f);
}

AABB Player::bounds() const
{
    const float hw = WIDTH * 0.5f;
    return {position - glm::vec3(hw, 0.0f, hw), position + glm::vec3(hw, HEIGHT, hw)};
}

glm::vec3 Player::eyePosition() const { return position + glm::vec3(0.0f, m_eyeOffset, 0.0f); }

void Player::update(const World& world, const Controls& c, const glm::vec3& forward, const glm::vec3& right, float dt)
{
    // Pri nízkom FPS (alebo zaseknutí okna) nech hráč nepreletí cez zem.
    dt = std::min(dt, 0.05f);

    inWater = touchesLiquid(world, bounds());
    sneaking = c.sneak && !flying && !inWater;
    sprinting = c.sprint && c.move.y > 0.0f && !sneaking;

    // Smer pohybu po zemi
    glm::vec3 wish = forward * c.move.y + right * c.move.x;
    wish.y = 0.0f;
    if (glm::length(wish) > 0.0f) wish = glm::normalize(wish);

    float speed = flying    ? (sprinting ? FLY_SPRINT_SPEED : FLY_SPEED)
                : sneaking  ? SNEAK_SPEED
                : sprinting ? SPRINT_SPEED
                            : WALK_SPEED;
    if (inWater && !flying) speed *= WATER_SPEED_FACTOR;

    float accel = (onGround || flying || inWater) ? GROUND_ACCEL : AIR_ACCEL;
    glm::vec2 horiz = approach({velocity.x, velocity.z}, glm::vec2(wish.x, wish.z) * speed, accel * dt);
    velocity.x = horiz.x;
    velocity.z = horiz.y;

    if (flying) {
        float vertical = (c.jump ? 1.0f : 0.0f) - (c.sneak ? 1.0f : 0.0f);
        velocity.y = vertical * speed;
    } else if (inWater) {
        velocity.y -= WATER_GRAVITY * dt;
        if (c.jump) velocity.y = std::min(velocity.y + WATER_SWIM_UP * dt, WATER_MAX_UP);
        velocity *= std::exp(-WATER_DRAG * dt);
    } else {
        velocity.y = std::max(velocity.y - GRAVITY * dt, -TERMINAL_VELOCITY);
        if (c.jump && onGround) velocity.y = JUMP_VELOCITY;
    }

    AABB box = bounds();
    MoveResult r = moveAndCollide(world, box, velocity, dt);
    position = glm::vec3((box.min.x + box.max.x) * 0.5f, box.min.y, (box.min.z + box.max.z) * 0.5f);
    onGround = r.onGround;

    // Pri plávaní pri brehu: ak narazíš do steny a držíš Space, vyskoč von z vody.
    if (inWater && r.hitWall && c.jump) velocity.y = std::max(velocity.y, 5.0f);

    // Pristátie ukončí let.
    if (flying && r.onGround && c.sneak) flying = false;

    // Plynulé prikrčenie kamery.
    float targetEye = sneaking ? SNEAK_EYE_HEIGHT : EYE_HEIGHT;
    m_eyeOffset += (targetEye - m_eyeOffset) * std::min(1.0f, dt * 15.0f);
}

} // namespace cubiq
