#pragma once
#include "World/Block.h"

#include <array>

namespace cubiq {

struct ItemStack {
    BlockId id = BlockId::Air;
    int count = 0;

    bool empty() const { return count <= 0 || id == BlockId::Air; }
    void clear() { id = BlockId::Air; count = 0; }
};

// 36 slotov: 0–8 hotbar, 9–35 hlavný inventár.
class Inventory {
public:
    static constexpr int HOTBAR = 9;
    static constexpr int SIZE = 36;
    static constexpr int MAX_STACK = 64;

    // Pridá predmety – najskôr do rozpracovaných stackov, potom do prázdnych slotov.
    // Vráti, koľko sa nezmestilo.
    int add(BlockId id, int count);

    ItemStack& slot(int i) { return m_slots[i]; }
    const ItemStack& slot(int i) const { return m_slots[i]; }

    int selected() const { return m_selected; }
    void select(int i) { m_selected = ((i % HOTBAR) + HOTBAR) % HOTBAR; }
    ItemStack& selectedStack() { return m_slots[m_selected]; }

    // Klik v obrazovke inventára: vezme / položí / spojí / vymení stack s tým v ruke.
    void clickSlot(int i, ItemStack& held);

private:
    std::array<ItemStack, SIZE> m_slots{};
    int m_selected = 0;
};

} // namespace cubiq
