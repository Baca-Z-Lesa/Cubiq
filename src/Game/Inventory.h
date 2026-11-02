#pragma once
#include "Game/Item.h"

#include <array>

namespace cubiq {

struct ItemStack {
    Item id = Item::Air;
    int count = 0;
    int durability = 0; // zostávajúce použitia nástroja (0 = nástroj nie je / nekazí sa)

    bool empty() const { return count <= 0 || id == Item::Air; }
    void clear() { *this = ItemStack{}; }
    int maxStack() const { return itemInfo(id).maxStack; }

    static ItemStack of(Item item, int count = 1)
    {
        return {item, count, itemInfo(item).durability};
    }
};

// 36 slotov: 0–8 hotbar, 9–35 hlavný inventár.
class Inventory {
public:
    static constexpr int HOTBAR = 9;
    static constexpr int SIZE = 36;
    static constexpr int MAX_STACK = 64;

    // Pridá predmety – najskôr do rozpracovaných stackov, potom do prázdnych slotov.
    // Vráti, koľko sa nezmestilo.
    int add(Item id, int count);
    int add(const ItemStack& stack);

    // Koľko kusov daného predmetu je v inventári a odobratie (pre recepty).
    int count(Item id) const;
    void remove(Item id, int count);

    ItemStack& slot(int i) { return m_slots[i]; }
    const ItemStack& slot(int i) const { return m_slots[i]; }

    int selected() const { return m_selected; }
    void select(int i) { m_selected = ((i % HOTBAR) + HOTBAR) % HOTBAR; }
    ItemStack& selectedStack() { return m_slots[m_selected]; }
    const ItemStack& selectedStack() const { return m_slots[m_selected]; }

    // Opotrebenie nástroja v ruke; pri nule sa zlomí.
    void damageSelected(int amount = 1);

private:
    std::array<ItemStack, SIZE> m_slots{};
    int m_selected = 0;
};

// Klik na slot (inventár, crafting, pec): vezme / položí / spojí / vymení stack s tým v ruke.
// Pravé tlačidlo: polovica zo slotu, alebo položí 1 kus.
void clickStack(ItemStack& slot, ItemStack& held, bool rightButton);

} // namespace cubiq
