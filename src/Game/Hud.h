#pragma once
#include "Game/Crafting.h"
#include "Game/Inventory.h"

#include <glm/vec2.hpp>
#include <vector>

namespace cubiq {

class UIRenderer;
struct Vitals;

// Čo je otvorené nad inventárom.
enum class Container : uint8_t { None, Inventory, Table, Altar, Furnace };

// Všetko, čo herné UI potrebuje nakresliť.
struct UiState {
    const Inventory* inventory = nullptr;
    const ItemStack* held = nullptr;
    Container open = Container::None;
    bool creative = false;
    const CraftGrid* grid = nullptr;    // pri Inventory/Table/Altar
    const Furnace* furnace = nullptr;   // pri Furnace
};

// Na čo myš ukazuje v otvorenom UI.
struct UiTarget {
    enum Kind { None, Slot, Craft, Result, FurnaceInput, FurnaceFuel, FurnaceOutput, Catalog, Book } kind = None;
    int index = 0;            // slot inventára / bunka mriežky / poradie receptu
    Item item = Item::Air;    // katalóg
};

// Herné UI: zameriavač, hotbar, inventár (E), crafting, pec, kniha receptov.
// V kreatívnom móde je nad inventárom katalóg všetkých predmetov.
class Hud {
public:
    void draw(UIRenderer& ui, const UiState& state, glm::vec2 mouse) const;

    // Srdiečka, hlad a bubliny nad hotbarom + červené bliknutie pri zranení (len survival).
    // eating 0..1 = priebeh jedenia.
    void drawVitals(UIRenderer& ui, const Vitals& vitals, float eating, float time) const;

    UiTarget targetAt(int width, int height, glm::vec2 mouse, const UiState& state) const;

    // Recepty v knihe pre danú mriežku (indexy do allRecipes()).
    static std::vector<int> bookRecipes(const CraftGrid& grid);

    // Všetky predmety v katalógu (bloky okrem vzduchu/vody/lávy/bedrocku + všetky predmety).
    static const std::vector<Item>& catalog();

    // Ikona predmetu (blok izometricky, ostatné ploché).
    static void drawItem(UIRenderer& ui, const ItemStack& stack, float x, float y, float size);
};

} // namespace cubiq
