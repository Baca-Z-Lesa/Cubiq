#pragma once
#include "Game/Inventory.h"

#include <glm/vec2.hpp>

namespace cubiq {

class UIRenderer;

// Herné UI: zameriavač, hotbar a obrazovka inventára (E).
class Hud {
public:
    void draw(UIRenderer& ui, const Inventory& inventory, bool inventoryOpen, const ItemStack& held,
              glm::vec2 mouse) const;

    // Index slotu pod myšou v otvorenom inventári, alebo -1.
    int slotAt(int width, int height, glm::vec2 mouse) const;
};

} // namespace cubiq
