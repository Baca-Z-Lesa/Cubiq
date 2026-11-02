#pragma once
#include "Engine/Shader.h"
#include "Game/Inventory.h"

#include <glm/glm.hpp>
#include <vector>

namespace cubiq {

class World;
class Texture;
struct SkyState;

// Predmet ležiaci na zemi: vypadne z vykopaného bloku, zo zvieraťa alebo pri smrti.
struct DroppedItem {
    ItemStack stack;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float age = 0.0f;
    float pickupDelay = 0.4f; // chvíľu sa nedá zdvihnúť (aby vyhodený predmet hneď neskočil späť)
    float spin = 0.0f;
};

class DroppedItems {
public:
    static constexpr float LIFETIME = 300.0f; // po 5 minútach zmizne
    static constexpr int MAX_ITEMS = 400;

    // Vyhodí predmet s malým náhodným odrazom.
    void spawn(const ItemStack& stack, const glm::vec3& position, const glm::vec3& velocity = glm::vec3(0.0f));

    // Fyzika, spájanie rovnakých kôp a zbieranie hráčom (priťahovanie do 3 blokov).
    // Vráti true, ak sa niečo zdvihlo.
    bool update(const World& world, Inventory& inventory, const glm::vec3& playerCenter, float dt);

    const std::vector<DroppedItem>& items() const { return m_items; }
    void clear() { m_items.clear(); }

private:
    std::vector<DroppedItem> m_items;
    uint32_t m_rng = 0x9e3779b9u;
    float random01();
};

// Kreslí ležiace predmety: bloky ako malé kocky, ostatné ako otáčajúce sa ploché ikony.
class DroppedItemRenderer {
public:
    DroppedItemRenderer();
    ~DroppedItemRenderer();
    DroppedItemRenderer(const DroppedItemRenderer&) = delete;
    DroppedItemRenderer& operator=(const DroppedItemRenderer&) = delete;

    void draw(const DroppedItems& items, const World& world, const glm::mat4& viewProjection, const glm::vec3& cameraPos,
              const SkyState& sky, const glm::vec3& fogColor, float fogStart, float fogEnd, const Texture& atlas);

private:
    Shader m_shader;
    unsigned int m_vao = 0, m_vbo = 0;
    std::vector<float> m_vertices;
};

} // namespace cubiq
