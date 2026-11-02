#include "Game/Crafting.h"
#include "Game/DroppedItems.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace cubiq {

bool Ingredient::matches(Item item) const
{
    for (int i = 0; i < count; ++i)
        if (options[i] == item) return true;
    return false;
}

namespace {
    Ingredient ing(std::initializer_list<Item> items)
    {
        Ingredient g;
        for (Item i : items) g.options[g.count++] = i;
        return g;
    }
    Ingredient ing(BlockId b) { return ing({itemOf(b)}); }
    Ingredient ing(Item i) { return ing({i}); }

    const Ingredient LOG = ing({itemOf(BlockId::OakLog), itemOf(BlockId::BirchLog), itemOf(BlockId::SpruceLog), itemOf(BlockId::CherryLog)});
    const Ingredient PLANKS = ing({itemOf(BlockId::OakPlanks), itemOf(BlockId::SprucePlanks)});
    const Ingredient STONE = ing({itemOf(BlockId::Cobblestone), itemOf(BlockId::MossyCobblestone)});

    // Tvarový recept z riadkov ("XXX", ".S.", ...) a kľúča znak → prísada.
    Recipe shaped(ItemStack result, Station station, std::initializer_list<const char*> rows,
                  std::initializer_list<std::pair<char, Ingredient>> key)
    {
        Recipe r;
        r.result = result;
        r.station = station;
        r.shaped = true;
        r.height = (int)rows.size();
        for (const char* row : rows) r.width = std::max(r.width, (int)std::char_traits<char>::length(row));
        for (const char* row : rows)
            for (int x = 0; x < r.width; ++x) {
                char c = x < (int)std::char_traits<char>::length(row) ? row[x] : '.';
                Ingredient g;
                for (const auto& [k, v] : key)
                    if (k == c) g = v;
                r.ingredients.push_back(g);
            }
        return r;
    }

    Recipe shapeless(ItemStack result, Station station, std::initializer_list<Ingredient> list)
    {
        Recipe r;
        r.result = result;
        r.station = station;
        r.shaped = false;
        r.ingredients = list;
        return r;
    }

    void addTools(std::vector<Recipe>& out, const Ingredient& m, Item pick, Item axe, Item shovel, Item sword)
    {
        const Ingredient S = ing(Item::Stick);
        out.push_back(shaped(ItemStack::of(pick), Station::Table, {"MMM", ".S.", ".S."}, {{'M', m}, {'S', S}}));
        out.push_back(shaped(ItemStack::of(axe), Station::Table, {"MM", "MS", ".S"}, {{'M', m}, {'S', S}}));
        out.push_back(shaped(ItemStack::of(shovel), Station::Table, {"M", "S", "S"}, {{'M', m}, {'S', S}}));
        out.push_back(shaped(ItemStack::of(sword), Station::Table, {"M", "M", "S"}, {{'M', m}, {'S', S}}));
    }

    std::vector<Recipe> buildRecipes()
    {
        std::vector<Recipe> r;
        const Ingredient S = ing(Item::Stick);
        // --- ruka (2×2)
        r.push_back(shapeless(ItemStack::of(itemOf(BlockId::OakPlanks), 4), Station::Hand,
                              {ing({itemOf(BlockId::OakLog), itemOf(BlockId::BirchLog), itemOf(BlockId::CherryLog)})}));
        r.push_back(shapeless(ItemStack::of(itemOf(BlockId::SprucePlanks), 4), Station::Hand, {ing(BlockId::SpruceLog)}));
        r.push_back(shaped(ItemStack::of(Item::Stick, 4), Station::Hand, {"P", "P"}, {{'P', PLANKS}}));
        r.push_back(shaped(ItemStack::of(itemOf(BlockId::CraftingTable)), Station::Hand, {"PP", "PP"}, {{'P', PLANKS}}));
        r.push_back(shaped(ItemStack::of(itemOf(BlockId::Lantern), 2), Station::Hand, {"C", "S"}, {{'C', ing(Item::Coal)}, {'S', S}}));
        r.push_back(shaped(ItemStack::of(itemOf(BlockId::StoneBricks), 4), Station::Hand, {"KK", "KK"}, {{'K', ing(BlockId::Stone)}}));
        // --- pracovný stôl (3×3)
        r.push_back(shaped(ItemStack::of(itemOf(BlockId::Furnace)), Station::Table, {"CCC", "C.C", "CCC"}, {{'C', STONE}}));
        addTools(r, PLANKS, Item::WoodPickaxe, Item::WoodAxe, Item::WoodShovel, Item::WoodSword);
        addTools(r, STONE, Item::StonePickaxe, Item::StoneAxe, Item::StoneShovel, Item::StoneSword);
        addTools(r, ing(Item::IronIngot), Item::IronPickaxe, Item::IronAxe, Item::IronShovel, Item::IronSword);
        r.push_back(shaped(ItemStack::of(Item::AetheritePickaxe), Station::Table, {"AAA", ".S.", ".S."}, {{'A', ing(Item::AetheriteShard)}, {'S', S}}));
        r.push_back(shaped(ItemStack::of(Item::AetheriteSword), Station::Table, {"A", "A", "S"}, {{'A', ing(Item::AetheriteShard)}, {'S', S}}));
        r.push_back(shaped(ItemStack::of(Item::Wand), Station::Table, {"A", "G", "S"}, {{'A', ing(Item::AetheriteShard)}, {'G', ing(Item::GoldIngot)}, {'S', S}}));
        r.push_back(shaped(ItemStack::of(itemOf(BlockId::RunicAltar)), Station::Table, {"AGA", "BBB"},
                           {{'A', ing(Item::AetheriteShard)}, {'G', ing(Item::GoldIngot)}, {'B', ing(BlockId::StoneBricks)}}));
        // --- runový oltár
        const Ingredient E = ing(Item::ShadowEssence), A = ing(Item::AetheriteShard);
        r.push_back(shapeless(ItemStack::of(Item::RuneLight), Station::Altar, {E, A, ing(BlockId::Lantern)}));
        r.push_back(shapeless(ItemStack::of(Item::RuneFire), Station::Altar, {E, A, ing(Item::Coal), ing(Item::GoldIngot)}));
        r.push_back(shapeless(ItemStack::of(Item::RuneBlink), Station::Altar, {E, E, A, ing(Item::GoldIngot)}));
        r.push_back(shapeless(ItemStack::of(Item::RuneLevitation), Station::Altar, {E, E, A, ing(Item::SkyCrystal)}));
        r.push_back(shapeless(ItemStack::of(Item::ManaCrystal), Station::Altar, {A, A, A, A, ing(Item::GoldIngot)}));
        r.push_back(shaped(ItemStack::of(Item::SkyStaff), Station::Altar, {"KKK", ".W.", ".E."},
                           {{'K', ing(Item::SkyCrystal)}, {'W', ing(Item::Wand)}, {'E', E}}));
        return r;
    }

    // Ohraničenie obsadených buniek mriežky.
    bool bounds(const CraftGrid& g, int& x0, int& y0, int& x1, int& y1)
    {
        x0 = y0 = 3; x1 = y1 = -1;
        for (int y = 0; y < g.size; ++y)
            for (int x = 0; x < g.size; ++x)
                if (!g.at(x, y).empty()) {
                    x0 = std::min(x0, x); y0 = std::min(y0, y);
                    x1 = std::max(x1, x); y1 = std::max(y1, y);
                }
        return x1 >= 0;
    }

    bool matchShaped(const Recipe& r, const CraftGrid& g, bool mirror)
    {
        int x0, y0, x1, y1;
        if (!bounds(g, x0, y0, x1, y1)) return false;
        if (x1 - x0 + 1 != r.width || y1 - y0 + 1 != r.height) return false;
        for (int y = 0; y < r.height; ++y)
            for (int x = 0; x < r.width; ++x) {
                const Ingredient& need = r.ingredients[y * r.width + (mirror ? r.width - 1 - x : x)];
                const ItemStack& have = g.at(x0 + x, y0 + y);
                if (need.empty() != have.empty()) return false;
                if (!need.empty() && !need.matches(have.id)) return false;
            }
        return true;
    }

    bool matchShapeless(const Recipe& r, const CraftGrid& g)
    {
        std::vector<bool> used(r.ingredients.size(), false);
        int items = 0;
        for (int y = 0; y < g.size; ++y)
            for (int x = 0; x < g.size; ++x) {
                const ItemStack& s = g.at(x, y);
                if (s.empty()) continue;
                ++items;
                bool found = false;
                for (size_t i = 0; i < used.size() && !found; ++i)
                    if (!used[i] && r.ingredients[i].matches(s.id)) used[i] = found = true;
                if (!found) return false;
            }
        return items == (int)r.ingredients.size();
    }

}

const std::vector<Recipe>& allRecipes()
{
    static const std::vector<Recipe> recipes = buildRecipes();
    return recipes;
}

bool recipeFits(const Recipe& r, int size)
{
    return r.shaped ? r.width <= size && r.height <= size : (int)r.ingredients.size() <= size * size;
}

bool availableAt(const Recipe& r, Station s)
{
    if (r.station == Station::Hand) return true;
    if (r.station == Station::Table) return s != Station::Hand;
    return s == Station::Altar;
}

const Recipe* matchRecipe(const CraftGrid& grid)
{
    for (const Recipe& r : allRecipes()) {
        if (!availableAt(r, grid.station) || !recipeFits(r, grid.size)) continue;
        if (r.shaped ? (matchShaped(r, grid, false) || matchShaped(r, grid, true)) : matchShapeless(r, grid)) return &r;
    }
    return nullptr;
}

void consumeGrid(CraftGrid& grid)
{
    for (auto& c : grid.cells)
        if (!c.empty() && --c.count <= 0) c.clear();
}

bool canCraftFrom(const Recipe& r, const Inventory& inventory, const CraftGrid& grid)
{
    if (!availableAt(r, grid.station) || !recipeFits(r, grid.size)) return false;
    std::map<Item, int> pool;
    for (int i = 0; i < Inventory::SIZE; ++i)
        if (!inventory.slot(i).empty()) pool[inventory.slot(i).id] += inventory.slot(i).count;
    for (const auto& c : grid.cells)
        if (!c.empty()) pool[c.id] += c.count;
    for (const Ingredient& g : r.ingredients) {
        if (g.empty()) continue;
        bool ok = false;
        for (int i = 0; i < g.count && !ok; ++i)
            if (pool[g.options[i]] > 0) { --pool[g.options[i]]; ok = true; }
        if (!ok) return false;
    }
    return true;
}

bool fillGrid(const Recipe& r, Inventory& inventory, CraftGrid& grid)
{
    if (!canCraftFrom(r, inventory, grid)) return false;
    for (auto& c : grid.cells) {
        if (c.empty()) continue;
        int left = inventory.add(c);
        if (left > 0) { c.count = left; return false; } // inventár je plný
        c.clear();
    }
    auto take = [&](const Ingredient& g) {
        for (int i = 0; i < g.count; ++i)
            if (inventory.count(g.options[i]) > 0) {
                inventory.remove(g.options[i], 1);
                return ItemStack::of(g.options[i]);
            }
        return ItemStack{};
    };
    if (r.shaped) {
        for (int y = 0; y < r.height; ++y)
            for (int x = 0; x < r.width; ++x) {
                const Ingredient& g = r.ingredients[y * r.width + x];
                if (!g.empty()) grid.at(x, y) = take(g);
            }
    } else {
        for (size_t i = 0; i < r.ingredients.size(); ++i) grid.at((int)i % grid.size, (int)i / grid.size) = take(r.ingredients[i]);
    }
    return true;
}

void returnGrid(CraftGrid& grid, Inventory& inventory, DroppedItems& drops, const glm::vec3& at)
{
    for (auto& c : grid.cells) {
        if (c.empty()) continue;
        int left = inventory.add(c);
        if (left > 0) {
            ItemStack rest = c;
            rest.count = left;
            drops.spawn(rest, at);
        }
        c.clear();
    }
}

// ------------------------------------------------------------------ pec

Item smeltResult(Item in)
{
    switch (in) {
    case itemOf(BlockId::IronOre): return Item::IronIngot;
    case itemOf(BlockId::GoldOre): return Item::GoldIngot;
    case itemOf(BlockId::Sand):
    case itemOf(BlockId::RedSand): return itemOf(BlockId::Glass);
    case itemOf(BlockId::Cobblestone): return itemOf(BlockId::Stone);
    case itemOf(BlockId::OakLog):
    case itemOf(BlockId::BirchLog):
    case itemOf(BlockId::SpruceLog):
    case itemOf(BlockId::CherryLog): return Item::Coal; // drevené uhlie
    case Item::RawMeat: return Item::CookedMeat;
    default: return Item::Air;
    }
}

float fuelTime(Item f)
{
    switch (f) {
    case Item::Coal: return 40.0f;     // 8 kusov
    case itemOf(BlockId::OakLog):
    case itemOf(BlockId::BirchLog):
    case itemOf(BlockId::SpruceLog):
    case itemOf(BlockId::CherryLog):
    case itemOf(BlockId::OakPlanks):
    case itemOf(BlockId::SprucePlanks):
    case itemOf(BlockId::CraftingTable): return 7.5f;
    case Item::Stick: return 2.5f;
    default: return 0.0f;
    }
}

void Furnace::update(float dt)
{
    const Item result = input.empty() ? Item::Air : smeltResult(input.id);
    const bool room = output.empty() || (output.id == result && output.count < output.maxStack());
    const bool canSmelt = result != Item::Air && room;

    if (!burning() && canSmelt && !fuel.empty() && fuelTime(fuel.id) > 0.0f) {
        burnLeft = burnTotal = fuelTime(fuel.id);
        if (--fuel.count <= 0) fuel.clear();
    }
    if (burning()) {
        burnLeft = std::max(0.0f, burnLeft - dt);
        if (canSmelt) {
            progress += dt;
            if (progress >= SMELT_TIME) {
                progress = 0.0f;
                if (output.empty()) output = ItemStack::of(result, 1);
                else ++output.count;
                if (--input.count <= 0) input.clear();
            }
        } else {
            progress = 0.0f;
        }
    } else {
        progress = std::max(0.0f, progress - dt * 2.0f); // vychladne
    }
}

Furnace& Furnaces::at(const glm::ivec3& pos)
{
    for (auto& f : m_furnaces)
        if (f.position == pos) return f;
    Furnace f;
    f.position = pos;
    m_furnaces.push_back(f);
    return m_furnaces.back();
}

const Furnace* Furnaces::find(const glm::ivec3& pos) const
{
    for (const auto& f : m_furnaces)
        if (f.position == pos) return &f;
    return nullptr;
}

void Furnaces::update(const World& world, DroppedItems& drops, float dt)
{
    for (auto& f : m_furnaces) f.update(dt);
    m_furnaces.erase(std::remove_if(m_furnaces.begin(), m_furnaces.end(), [&](Furnace& f) {
        const glm::ivec3 p = f.position;
        if (!world.getChunk(World::chunkCoordOf(p.x, p.z))) return false; // chunk nie je načítaný
        if (world.getBlock(p.x, p.y, p.z) == BlockId::Furnace) return false;
        const glm::vec3 c = glm::vec3(p) + glm::vec3(0.5f);
        drops.spawn(f.input, c);
        drops.spawn(f.fuel, c);
        drops.spawn(f.output, c);
        return true;
    }), m_furnaces.end());
}

} // namespace cubiq
