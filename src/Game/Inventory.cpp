#include "Game/Inventory.h"

#include <algorithm>
#include <utility>

namespace cubiq {

int Inventory::add(Item id, int count) { return add(ItemStack::of(id, count)); }

int Inventory::add(const ItemStack& stack)
{
    if (stack.empty()) return 0;
    int count = stack.count;
    const int maxStack = stack.maxStack();
    // Nástroje (stack 1) si pamätajú opotrebenie – nespájame ich.
    if (maxStack > 1) {
        for (auto& s : m_slots) {
            if (count == 0) return 0;
            if (s.id == stack.id && s.count > 0 && s.count < maxStack) {
                int n = std::min(count, maxStack - s.count);
                s.count += n;
                count -= n;
            }
        }
    }
    for (auto& s : m_slots) {
        if (count == 0) return 0;
        if (s.empty()) {
            int n = std::min(count, maxStack);
            s = stack;
            s.count = n;
            count -= n;
        }
    }
    return count;
}

int Inventory::count(Item id) const
{
    int n = 0;
    for (const auto& s : m_slots)
        if (s.id == id) n += s.count;
    return n;
}

void Inventory::remove(Item id, int count)
{
    for (auto& s : m_slots) {
        if (count <= 0) return;
        if (s.id != id) continue;
        int n = std::min(count, s.count);
        s.count -= n;
        count -= n;
        if (s.count <= 0) s.clear();
    }
}

void Inventory::damageSelected(int amount)
{
    ItemStack& s = selectedStack();
    if (s.empty() || itemInfo(s.id).durability == 0) return;
    s.durability -= amount;
    if (s.durability <= 0) s.clear(); // nástroj sa zlomil
}

void clickStack(ItemStack& s, ItemStack& held, bool right)
{
    const bool same = !held.empty() && !s.empty() && held.id == s.id && s.maxStack() > 1;
    if (right) {
        if (held.empty() && !s.empty()) {
            // vziať polovicu
            held = s;
            held.count = (s.count + 1) / 2;
            s.count -= held.count;
            if (s.count <= 0) s.clear();
        } else if (!held.empty() && (s.empty() || (same && s.count < s.maxStack()))) {
            // položiť jeden kus
            if (s.empty()) { s = held; s.count = 0; }
            ++s.count;
            if (--held.count <= 0) held.clear();
        }
        return;
    }
    if (same) {
        int n = std::min(held.count, s.maxStack() - s.count);
        s.count += n;
        held.count -= n;
        if (held.count == 0) held.clear();
    } else {
        std::swap(s, held);
    }
}

} // namespace cubiq
