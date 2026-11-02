#include "Game/Vitals.h"
#include "Game/Player.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    bool touchesBlock(const World& world, const AABB& box, BlockId id)
    {
        for (int x = (int)std::floor(box.min.x); x <= (int)std::floor(box.max.x - 0.001f); ++x)
            for (int y = (int)std::floor(box.min.y); y <= (int)std::floor(box.max.y - 0.001f); ++y)
                for (int z = (int)std::floor(box.min.z); z <= (int)std::floor(box.max.z - 0.001f); ++z)
                    if (world.getBlock(x, y, z) == id) return true;
        return false;
    }
}

bool Vitals::damage(float amount, const char* cause)
{
    if (dead() || amount <= 0.0f || invulnerable > 0.0f) return false;
    health = std::max(0.0f, health - amount);
    hurtTimer = 0.35f;
    invulnerable = 0.5f;
    lastDamage = cause;
    exhaust(0.1f);
    return true;
}

void Vitals::eat(int food)
{
    hunger = std::min(MAX_HUNGER, hunger + food);
    saturation = std::min(hunger, saturation + food * 0.6f);
}

void Vitals::update(const World& world, const Player& player, float dt)
{
    hurtTimer = std::max(0.0f, hurtTimer - dt);
    invulnerable = std::max(0.0f, invulnerable - dt);
    if (dead()) return;

    // --- Pád: výška od najvyššieho bodu letu po dopad, prvé 3 bloky sú zadarmo.
    const bool airborne = !player.onGround && !player.inWater && !player.flying;
    if (airborne) {
        if (!falling) { falling = true; fallStartY = player.position.y; }
        fallStartY = std::max(fallStartY, player.position.y);
    } else if (falling) {
        falling = false;
        float height = fallStartY - player.position.y;
        if (player.onGround && height > 3.5f) damage(std::floor(height - 3.0f), "pad z vysky");
    }
    if (player.inWater || player.flying) fallStartY = player.position.y;

    // --- Námaha z pohybu: šprint a skákanie míňajú viac.
    const glm::vec3 pos = player.position;
    if (m_hasLastPos) {
        float moved = glm::length(glm::vec2(pos.x - m_lastPos.x, pos.z - m_lastPos.z));
        if (moved < 2.0f) exhaust(moved * (player.sprinting ? 0.1f : player.inWater ? 0.015f : 0.01f));
        if (pos.y > m_lastPos.y + 0.05f && !player.inWater && !player.flying) exhaust((pos.y - m_lastPos.y) * 0.1f);
    }
    m_lastPos = pos;
    m_hasLastPos = true;
    exhaust(dt * 0.005f); // aj státie trochu vyhladne

    while (exhaustion >= 4.0f) {
        exhaustion -= 4.0f;
        if (saturation > 0.0f) saturation = std::max(0.0f, saturation - 1.0f);
        else hunger = std::max(0.0f, hunger - 1.0f);
    }

    // --- Regenerácia pri plnom žalúdku, hladovanie pri prázdnom.
    if (hunger >= 18.0f && health < MAX_HEALTH) {
        m_regenTimer += dt;
        float period = saturation > 0.0f && hunger >= MAX_HUNGER ? 0.6f : 3.0f;
        if (m_regenTimer >= period) {
            m_regenTimer = 0.0f;
            health = std::min(MAX_HEALTH, health + 1.0f);
            exhaust(1.5f);
        }
    } else {
        m_regenTimer = 0.0f;
    }
    if (hunger <= 0.0f) {
        m_starveTimer += dt;
        if (m_starveTimer >= 4.0f) {
            m_starveTimer = 0.0f;
            if (health > 1.0f) damage(1.0f, "hlad");
        }
    } else {
        m_starveTimer = 0.0f;
    }

    // --- Dych: hlava pod vodou.
    const glm::vec3 eye = player.eyePosition();
    const BlockId atEye = world.getBlock((int)std::floor(eye.x), (int)std::floor(eye.y), (int)std::floor(eye.z));
    if (atEye == BlockId::Water) {
        air = std::max(0.0f, air - dt);
        if (air <= 0.0f) {
            m_drownTimer += dt;
            if (m_drownTimer >= 1.0f) { m_drownTimer = 0.0f; damage(2.0f, "utopenie"); }
        }
    } else {
        air = std::min(MAX_AIR, air + dt * 5.0f);
        m_drownTimer = 0.0f;
    }

    // --- Láva páli.
    if (touchesBlock(world, player.bounds(), BlockId::Lava)) {
        m_lavaTimer += dt;
        if (m_lavaTimer >= 0.5f) { m_lavaTimer = 0.0f; invulnerable = 0.0f; damage(4.0f, "lava"); }
    } else {
        m_lavaTimer = 0.4f; // prvý dotyk páli takmer hneď
    }
}

} // namespace cubiq
