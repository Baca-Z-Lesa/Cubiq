#include "Game/Hud.h"
#include "Engine/UIRenderer.h"
#include "Game/BlockAtlas.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    const glm::vec4 SLOT_BG(0.0f, 0.0f, 0.0f, 0.45f);
    const glm::vec4 SLOT_BORDER(1.0f, 1.0f, 1.0f, 0.18f);
    const glm::vec4 WHITE(1.0f);
    const glm::vec4 SHADOW(0.0f, 0.0f, 0.0f, 0.8f);

    // Veľkosť "UI pixelu" podľa výšky okna (720p → 3).
    float uiScale(int height) { return std::max(2.0f, std::round(height / 240.0f)); }

    struct Rect { float x, y, w, h; };

    // Pozícia slotu v otvorenom inventári: 3 riadky hlavného inventára, medzera, hotbar.
    Rect inventorySlotRect(int i, int width, int height)
    {
        float s = uiScale(height);
        float slot = 20.0f * s;
        float gridW = 9 * slot;
        float gridH = 4 * slot + 6 * s;
        float x0 = (width - gridW) * 0.5f;
        float y0 = (height - gridH) * 0.5f;

        int col, row;
        float extra = 0.0f;
        if (i < Inventory::HOTBAR) {
            col = i;
            row = 3;
            extra = 6 * s;
        } else {
            col = (i - Inventory::HOTBAR) % 9;
            row = (i - Inventory::HOTBAR) / 9;
        }
        return {x0 + col * slot, y0 + row * slot + extra, slot, slot};
    }

    Rect hotbarSlotRect(int i, int width, int height)
    {
        float s = uiScale(height);
        float slot = 20.0f * s;
        float x0 = (width - 9 * slot) * 0.5f;
        float y0 = height - slot - 4 * s;
        return {x0 + i * slot, y0, slot, slot};
    }

    // Izometrická ikona bloku: vrch + dva boky, každá stena inak tieňovaná.
    void drawBlockIcon(UIRenderer& ui, BlockId id, glm::vec2 c, float r)
    {
        const BlockType& type = blockType(id);
        const float k = 0.866f * r; // cos(30°)
        const glm::vec2 top(c.x, c.y - r), ur(c.x + k, c.y - r * 0.5f), lr(c.x + k, c.y + r * 0.5f);
        const glm::vec2 bottom(c.x, c.y + r), ll(c.x - k, c.y + r * 0.5f), ul(c.x - k, c.y - r * 0.5f);

        auto face = [&](glm::vec2 a, glm::vec2 b, glm::vec2 cc, glm::vec2 d, int tile, float shade) {
            float u0 = (float)tile / ATLAS_TILES, u1 = (float)(tile + 1) / ATLAS_TILES;
            const glm::vec2 p[4] = {a, b, cc, d};
            const glm::vec2 uv[4] = {{u0, 0.0f}, {u1, 0.0f}, {u1, 1.0f}, {u0, 1.0f}};
            float alpha = type.liquid ? 0.8f : 1.0f;
            ui.quad(p, uv, glm::vec4(shade, shade, shade, alpha), true);
        };
        face(top, ur, c, ul, type.tileTop, 1.0f);
        face(ul, c, bottom, ll, type.tileSide, 0.75f);
        face(c, ur, lr, bottom, type.tileSide, 0.6f);
    }

    void drawStack(UIRenderer& ui, const ItemStack& stack, Rect r, float s)
    {
        if (stack.empty()) return;
        drawBlockIcon(ui, stack.id, {r.x + r.w * 0.5f, r.y + r.h * 0.5f - s}, 7.0f * s);
        if (stack.count > 1) {
            float pixel = s;
            float w = UIRenderer::numberWidth(stack.count, pixel);
            float x = r.x + r.w - w - 2 * s, y = r.y + r.h - 7 * s;
            ui.number(stack.count, x + pixel, y + pixel, pixel, SHADOW);
            ui.number(stack.count, x, y, pixel, WHITE);
        }
    }

    void drawSlot(UIRenderer& ui, Rect r, float s, bool highlight)
    {
        ui.rect(r.x + s * 0.5f, r.y + s * 0.5f, r.w - s, r.h - s, SLOT_BG);
        ui.rectOutline(r.x, r.y, r.w, r.h, s * 0.5f, SLOT_BORDER);
        if (highlight) ui.rect(r.x + s * 0.5f, r.y + s * 0.5f, r.w - s, r.h - s, glm::vec4(1, 1, 1, 0.15f));
    }
}

void Hud::draw(UIRenderer& ui, const Inventory& inventory, bool inventoryOpen, const ItemStack& held,
               glm::vec2 mouse) const
{
    const int w = ui.width(), h = ui.height();
    const float s = uiScale(h);

    if (!inventoryOpen) {
        // Zameriavač
        float len = 9 * s, t = s;
        ui.rect(w * 0.5f - len * 0.5f, h * 0.5f - t * 0.5f, len, t, glm::vec4(1, 1, 1, 0.85f));
        ui.rect(w * 0.5f - t * 0.5f, h * 0.5f - len * 0.5f, t, len, glm::vec4(1, 1, 1, 0.85f));

        // Hotbar
        for (int i = 0; i < Inventory::HOTBAR; ++i) {
            Rect r = hotbarSlotRect(i, w, h);
            drawSlot(ui, r, s, false);
            drawStack(ui, inventory.slot(i), r, s);
        }
        Rect sel = hotbarSlotRect(inventory.selected(), w, h);
        ui.rectOutline(sel.x - s, sel.y - s, sel.w + 2 * s, sel.h + 2 * s, 1.5f * s, WHITE);
        return;
    }

    // Obrazovka inventára
    ui.rect(0, 0, (float)w, (float)h, glm::vec4(0, 0, 0, 0.5f));
    Rect first = inventorySlotRect(Inventory::HOTBAR, w, h);
    Rect last = inventorySlotRect(Inventory::HOTBAR - 1, w, h);
    float pad = 6 * s;
    ui.rect(first.x - pad, first.y - pad, 9 * first.w + 2 * pad, last.y + last.h - first.y + 2 * pad,
            glm::vec4(0.12f, 0.12f, 0.14f, 0.92f));

    int hovered = slotAt(w, h, mouse);
    for (int i = 0; i < Inventory::SIZE; ++i) {
        Rect r = inventorySlotRect(i, w, h);
        drawSlot(ui, r, s, i == hovered);
        drawStack(ui, inventory.slot(i), r, s);
    }

    // Stack "v ruke" sa ťahá za myšou.
    if (!held.empty()) {
        float slot = 20.0f * s;
        drawStack(ui, held, {mouse.x - slot * 0.5f, mouse.y - slot * 0.5f, slot, slot}, s);
    }
}

int Hud::slotAt(int width, int height, glm::vec2 mouse) const
{
    for (int i = 0; i < Inventory::SIZE; ++i) {
        Rect r = inventorySlotRect(i, width, height);
        if (mouse.x >= r.x && mouse.x < r.x + r.w && mouse.y >= r.y && mouse.y < r.y + r.h)
            return i;
    }
    return -1;
}

} // namespace cubiq
