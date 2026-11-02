#pragma once
#include "Game/Inventory.h"

#include <glm/vec3.hpp>
#include <array>
#include <vector>

namespace cubiq {

class World;
class DroppedItems;

// Kde sa dá recept vyrobiť: v inventári (2×2), na pracovnom stole (3×3), na runovom oltári (3×3).
enum class Station : uint8_t { Hand, Table, Altar };

// Prísada: jeden z niekoľkých predmetov (napr. "akékoľvek drevo").
struct Ingredient {
    std::array<Item, 4> options{};
    int count = 0;

    bool empty() const { return count == 0; }
    bool matches(Item item) const;
    Item first() const { return options[0]; }
};

struct Recipe {
    ItemStack result;
    Station station = Station::Hand;
    bool shaped = true;
    int width = 0, height = 0;           // pre tvarové recepty
    std::vector<Ingredient> ingredients; // tvarové: width*height buniek (prázdna = nič), inak zoznam
};

// Mriežka craftingu (až 3×3, riadok po riadku). size = 2 alebo 3.
struct CraftGrid {
    std::array<ItemStack, 9> cells{};
    int size = 2;
    Station station = Station::Hand;

    ItemStack& at(int x, int y) { return cells[y * 3 + x]; }
    const ItemStack& at(int x, int y) const { return cells[y * 3 + x]; }
};

const std::vector<Recipe>& allRecipes();

// Dá sa recept použiť na danej stanici? (ručné všade, stolové aj na oltári, oltárne len na oltári)
bool availableAt(const Recipe& recipe, Station station);
// Zmestí sa recept do mriežky size×size?
bool recipeFits(const Recipe& recipe, int size);
// Recept, ktorý zodpovedá obsahu mriežky, alebo nullptr.
const Recipe* matchRecipe(const CraftGrid& grid);
// Spotrebuje po jednom kuse z každej obsadenej bunky.
void consumeGrid(CraftGrid& grid);
// Kniha receptov: dá sa recept vyrobiť z inventára (+ toho, čo už je v mriežke)?
bool canCraftFrom(const Recipe& recipe, const Inventory& inventory, const CraftGrid& grid);
// Kniha receptov: vráti mriežku do inventára a naplní ju prísadami receptu z inventára.
bool fillGrid(const Recipe& recipe, Inventory& inventory, CraftGrid& grid);
// Vráti obsah mriežky do inventára (čo sa nezmestí, vypadne pri hráčovi).
void returnGrid(CraftGrid& grid, Inventory& inventory, DroppedItems& drops, const glm::vec3& at);

// ------------------------------------------------------------------ pec

// Výsledok tavenia (Air = nedá sa taviť) a doba horenia paliva v sekundách (0 = nie je palivo).
Item smeltResult(Item input);
float fuelTime(Item fuel);

struct Furnace {
    static constexpr float SMELT_TIME = 5.0f;

    glm::ivec3 position{0};
    ItemStack input, fuel, output;
    float burnLeft = 0.0f;  // koľko ešte horí aktuálne palivo
    float burnTotal = 0.0f; // na koľko vydržalo (pre ikonku plameňa)
    float progress = 0.0f;  // sekundy tavenia aktuálneho kusu

    bool burning() const { return burnLeft > 0.0f; }
    void update(float dt);
};

// Všetky pece vo svete (ukladajú sa do world.dat).
class Furnaces {
public:
    Furnace& at(const glm::ivec3& pos); // vytvorí, ak ešte neexistuje
    const Furnace* find(const glm::ivec3& pos) const;
    // Tavenie + zničená pec vysype obsah (len v načítaných chunkoch).
    void update(const World& world, DroppedItems& drops, float dt);

    std::vector<Furnace>& all() { return m_furnaces; }
    const std::vector<Furnace>& all() const { return m_furnaces; }

private:
    std::vector<Furnace> m_furnaces;
};

} // namespace cubiq
