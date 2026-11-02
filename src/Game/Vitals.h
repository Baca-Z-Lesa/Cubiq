#pragma once
#include <glm/vec3.hpp>

namespace cubiq {

class Player;
class World;

// Životy, hlad a dych hráča v survival móde.
// Hodnoty sú v "pol-ikonách" ako v HUD: 20 životov = 10 sŕdc, 20 hladu = 10 stehienok.
struct Vitals {
    static constexpr float MAX_HEALTH = 20.0f;
    static constexpr float MAX_HUNGER = 20.0f;
    static constexpr float MAX_AIR = 10.0f; // sekundy pod vodou

    float health = MAX_HEALTH;
    float hunger = MAX_HUNGER;
    float saturation = 5.0f;  // rezerva, ktorá sa míňa skôr než hlad
    float exhaustion = 0.0f;  // námaha; každé 4 body uberú saturáciu alebo hlad
    float air = MAX_AIR;

    float hurtTimer = 0.0f;   // červené bliknutie obrazovky
    float invulnerable = 0.0f;
    const char* lastDamage = "";

    bool dead() const { return health <= 0.0f; }

    // Pád, voda, láva, regenerácia a hladovanie. Volá sa každú snímku (nie v kreatívnom móde).
    void update(const World& world, const Player& player, float dt);

    // Utŕženie zranenia; vráti true, ak naozaj zranilo (nie počas krátkej nezraniteľnosti).
    bool damage(float amount, const char* cause);

    void eat(int food);
    void exhaust(float amount) { exhaustion += amount; }
    void reset() { *this = Vitals{}; }

    // Pre výpočet pádu.
    float fallStartY = 0.0f;
    bool falling = false;

private:
    float m_regenTimer = 0.0f;
    float m_starveTimer = 0.0f;
    float m_drownTimer = 0.0f;
    float m_lavaTimer = 0.0f;
    glm::vec3 m_lastPos{0.0f};
    bool m_hasLastPos = false;
};

} // namespace cubiq
