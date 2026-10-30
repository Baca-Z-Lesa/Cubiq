#include "Game/Inventory.h"

#include <algorithm>
#include <utility>

namespace cubiq {

int Inventory::add(BlockId id, int count)
{
    for (auto& s : m_slots) {
        if (count == 0) return 0;
        if (s.id == id && s.count > 0 && s.count < MAX_STACK) {
            int n = std::min(count, MAX_STACK - s.count);
            s.count += n;
            count -= n;
        }
    }
    for (auto& s : m_slots) {
        if (count == 0) return 0;
        if (s.empty()) {
            int n = std::min(count, MAX_STACK);
            s.id = id;
            s.count = n;
            count -= n;
        }
    }
    return count;
}

void Inventory::clickSlot(int i, ItemStack& held)
{
    ItemStack& s = m_slots[i];
    if (!held.empty() && !s.empty() && held.id == s.id) {
        // Rovnaký blok → doplniť stack v slote.
        int n = std::min(held.count, MAX_STACK - s.count);
        s.count += n;
        held.count -= n;
        if (held.count == 0) held.clear();
    } else {
        std::swap(s, held);
    }
}

} // namespace cubiq
