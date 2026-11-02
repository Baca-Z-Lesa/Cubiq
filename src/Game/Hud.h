#pragma once
#include "Game/Inventory.h"

#include <glm/vec2.hpp>
#include <vector>

namespace cubiq {

class UIRenderer;

// Herné UI: zameriavač, hotbar a obrazovka inventára (E).
// V kreatívnom móde je nad inventárom katalóg všetkých predmetov.
class Hud {
public:
    void draw(UIRenderer& ui, const Inventory& inventory, bool inventoryOpen, const ItemStack& held,
              glm::vec2 mouse, bool creative) const;

    // Index slotu pod myšou v otvorenom inventári, alebo -1.
    int slotAt(int width, int height, glm::vec2 mouse, bool creative) const;

    // Predmet z kreatívneho katalógu pod myšou (Air = nič).
    Item catalogAt(int width, int height, glm::vec2 mouse) const;

    // Ikona predmetu (blok izometricky, ostatné ploché) – použije aj crafting a pec.
    static void drawItem(UIRenderer& ui, const ItemStack& stack, float x, float y, float size);

    // Všetky predmety v katalógu (bloky okrem vzduchu/vody/lávy/bedrocku + všetky predmety).
    static const std::vector<Item>& catalog();
};

} // namespace cubiq
