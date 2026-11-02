#include "Game/Hud.h"
#include "Engine/UIRenderer.h"
#include "Game/BlockAtlas.h"
#include "Game/Vitals.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    const glm::vec4 SLOT_BG(0.0f, 0.0f, 0.0f, 0.45f);
    const glm::vec4 SLOT_BORDER(1.0f, 1.0f, 1.0f, 0.18f);
    const glm::vec4 WHITE(1.0f);
    const glm::vec4 SHADOW(0.0f, 0.0f, 0.0f, 0.8f);

    constexpr int CATALOG_COLUMNS = 15;

    // Veľkosť "UI pixelu" podľa výšky okna (720p → 3).
    float uiScale(int height) { return std::max(2.0f, std::round(height / 240.0f)); }

    struct Rect { float x, y, w, h; };

    float catalogSlot(float s) { return 16.0f * s; }
    float catalogHeight(float s)
    {
        int rows = ((int)Hud::catalog().size() + CATALOG_COLUMNS - 1) / CATALOG_COLUMNS;
        return rows * catalogSlot(s);
    }

    // Pozícia slotu v otvorenom inventári: 3 riadky hlavného inventára, medzera, hotbar.
    // V kreatívnom móde je inventár posunutý nadol, nad ním je katalóg.
    Rect inventorySlotRect(int i, int width, int height, bool creative)
    {
        float s = uiScale(height);
        float slot = 20.0f * s;
        float gridW = 9 * slot;
        float gridH = 4 * slot + 6 * s;
        float x0 = (width - gridW) * 0.5f;
        float y0 = (height - gridH) * 0.5f;
        if (creative) y0 += (catalogHeight(s) + 14 * s) * 0.5f;

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

    Rect catalogSlotRect(int i, int width, int height)
    {
        float s = uiScale(height);
        float slot = catalogSlot(s);
        Rect inv = inventorySlotRect(Inventory::HOTBAR, width, height, true);
        float x0 = (width - CATALOG_COLUMNS * slot) * 0.5f;
        float y0 = inv.y - 14 * s - catalogHeight(s);
        return {x0 + (i % CATALOG_COLUMNS) * slot, y0 + (i / CATALOG_COLUMNS) * slot, slot, slot};
    }

    Rect hotbarSlotRect(int i, int width, int height)
    {
        float s = uiScale(height);
        float slot = 20.0f * s;
        float x0 = (width - 9 * slot) * 0.5f;
        float y0 = height - slot - 4 * s;
        return {x0 + i * slot, y0, slot, slot};
    }

    bool inside(Rect r, glm::vec2 m) { return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; }

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
        Hud::drawItem(ui, stack, r.x + r.w * 0.5f, r.y + r.h * 0.5f, 16.0f * s * (r.w / (20.0f * s)));
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

    void drawTooltip(UIRenderer& ui, const char* text, glm::vec2 mouse, float s)
    {
        float w = UIRenderer::textWidth(text, s) + 6 * s, h = 7 * s + 6 * s;
        float x = std::min(mouse.x + 8 * s, ui.width() - w - 2.0f), y = std::max(mouse.y - h - 4 * s, 2.0f);
        ui.rect(x, y, w, h, glm::vec4(0.08f, 0.05f, 0.14f, 0.94f));
        ui.rectOutline(x, y, w, h, s * 0.5f, glm::vec4(0.45f, 0.3f, 0.9f, 0.9f));
        ui.text(text, x + 3 * s, y + 3 * s, s, WHITE);
    }
}

const std::vector<Item>& Hud::catalog()
{
    static const std::vector<Item> items = [] {
        std::vector<Item> v;
        for (int b = 1; b < (int)BlockId::Count; ++b) {
            BlockId id = (BlockId)b;
            if (isLiquid(id) || id == BlockId::Bedrock) continue;
            v.push_back(itemOf(id));
        }
        for (int i = (int)Item::FirstItem; i < (int)Item::End; ++i) v.push_back((Item)i);
        return v;
    }();
    return items;
}

void Hud::drawItem(UIRenderer& ui, const ItemStack& stack, float cx, float cy, float size)
{
    if (stack.empty()) return;
    if (isBlockItem(stack.id)) {
        drawBlockIcon(ui, blockOf(stack.id), {cx, cy - size / 16.0f}, size * 0.44f);
    } else {
        int tile = itemIconTile(stack.id);
        float u0 = (float)tile / ATLAS_TILES, u1 = (float)(tile + 1) / ATLAS_TILES;
        float h = size * 0.5f;
        const glm::vec2 p[4] = {{cx - h, cy - h}, {cx + h, cy - h}, {cx + h, cy + h}, {cx - h, cy + h}};
        const glm::vec2 uv[4] = {{u0, 0.0f}, {u1, 0.0f}, {u1, 1.0f}, {u0, 1.0f}};
        ui.quad(p, uv, WHITE, true);
    }
    // Pruh opotrebenia nástroja (zelená → červená).
    const int maxDurability = itemInfo(stack.id).durability;
    if (maxDurability > 0 && stack.durability < maxDurability) {
        float f = std::clamp((float)stack.durability / maxDurability, 0.0f, 1.0f);
        float px = size / 16.0f;
        float w = size * 0.8f, x = cx - w * 0.5f, y = cy + size * 0.5f - 2.5f * px;
        ui.rect(x, y, w, 2 * px, glm::vec4(0, 0, 0, 0.9f));
        ui.rect(x, y, w * f, px, glm::vec4(1.0f - f, f, 0.1f, 1.0f));
    }
}

void Hud::draw(UIRenderer& ui, const Inventory& inventory, bool inventoryOpen, const ItemStack& held,
               glm::vec2 mouse, bool creative) const
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
    Rect first = inventorySlotRect(Inventory::HOTBAR, w, h, creative);
    Rect last = inventorySlotRect(Inventory::HOTBAR - 1, w, h, creative);
    float pad = 6 * s;
    ui.rect(first.x - pad, first.y - pad, 9 * first.w + 2 * pad, last.y + last.h - first.y + 2 * pad,
            glm::vec4(0.12f, 0.12f, 0.14f, 0.92f));

    const char* tooltip = nullptr;
    int hovered = slotAt(w, h, mouse, creative);
    for (int i = 0; i < Inventory::SIZE; ++i) {
        Rect r = inventorySlotRect(i, w, h, creative);
        drawSlot(ui, r, s, i == hovered);
        drawStack(ui, inventory.slot(i), r, s);
    }
    if (hovered >= 0 && !inventory.slot(hovered).empty()) tooltip = itemName(inventory.slot(hovered).id);

    if (creative) {
        const auto& items = catalog();
        Rect c0 = catalogSlotRect(0, w, h);
        float cw = CATALOG_COLUMNS * c0.w, ch = catalogHeight(s);
        ui.rect(c0.x - pad, c0.y - pad - 10 * s, cw + 2 * pad, ch + 2 * pad + 10 * s, glm::vec4(0.1f, 0.09f, 0.16f, 0.92f));
        ui.text("Katalog (kreativny mod)", c0.x, c0.y - 10 * s, s, glm::vec4(0.75f, 0.7f, 1.0f, 1.0f));
        for (int i = 0; i < (int)items.size(); ++i) {
            Rect r = catalogSlotRect(i, w, h);
            bool hover = inside(r, mouse);
            drawSlot(ui, r, s, hover);
            drawStack(ui, ItemStack::of(items[i]), r, s);
            if (hover) tooltip = itemName(items[i]);
        }
    }

    // Stack "v ruke" sa ťahá za myšou.
    if (!held.empty()) {
        float slot = 20.0f * s;
        drawStack(ui, held, {mouse.x - slot * 0.5f, mouse.y - slot * 0.5f, slot, slot}, s);
    } else if (tooltip) {
        drawTooltip(ui, tooltip, mouse, s);
    }
}

namespace {
    // Pixelové ikony 7×7 ('X' = farba, 'o' = svetlý odlesk / kosť).
    const char* HEART[7] = {".XX.XX.", "XoXXXXX", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X..."};
    const char* FOOD[7] = {"...XXX.", "..XXXXX", ".XXXXXX", ".XXXXX.", "..XXX..", ".o.....", "o.o...."};
    const char* BUBBLE[7] = {".......", "..XXX..", ".XoXXX.", ".XoXXX.", ".XXXXX.", "..XXX..", "......."};

    // columns = koľko stĺpcov kresliť (polovičná ikona = 4).
    void drawIcon(UIRenderer& ui, const char* const* pattern, float x, float y, float s, glm::vec4 color,
                  glm::vec4 light, int columns = 7)
    {
        for (int r = 0; r < 7; ++r)
            for (int c = 0; c < columns; ++c) {
                char ch = pattern[r][c];
                if (ch == '.') continue;
                ui.rect(x + c * s, y + r * s, s, s, ch == 'o' ? light : color);
            }
    }

    // Rad 10 ikon: plné, polovičné a prázdne (tmavý tieň). rightToLeft = hlad sa míňa sprava.
    void drawRow(UIRenderer& ui, const char* const* pattern, float value, float x, float y, float s, glm::vec4 color,
                 glm::vec4 light, bool rightToLeft, float shake)
    {
        const glm::vec4 empty(0.0f, 0.0f, 0.0f, 0.55f);
        for (int i = 0; i < 10; ++i) {
            float ix = rightToLeft ? x - (i + 1) * 8 * s : x + i * 8 * s;
            float iy = y + (shake > 0.0f ? std::sin(i * 2.1f + shake * 40.0f) * s : 0.0f);
            drawIcon(ui, pattern, ix + s * 0.5f, iy + s * 0.5f, s, empty, empty);
            float v = value - i * 2.0f;
            if (v >= 2.0f) drawIcon(ui, pattern, ix, iy, s, color, light);
            else if (v >= 1.0f) drawIcon(ui, pattern, ix, iy, s, color, light, 4);
        }
    }
}

void Hud::drawVitals(UIRenderer& ui, const Vitals& v, float eating, float time) const
{
    const int w = ui.width(), h = ui.height();
    const float s = uiScale(h);
    const Rect first = hotbarSlotRect(0, w, h), last = hotbarSlotRect(Inventory::HOTBAR - 1, w, h);
    const float y = first.y - 10 * s;

    if (v.hurtTimer > 0.0f) ui.rect(0, 0, (float)w, (float)h, glm::vec4(0.8f, 0.0f, 0.0f, v.hurtTimer * 0.6f));

    // Pri nízkych životoch sa srdiečka trasú.
    drawRow(ui, HEART, v.health, first.x, y, s, glm::vec4(0.86f, 0.12f, 0.14f, 1), glm::vec4(1.0f, 0.75f, 0.75f, 1),
            false, v.health <= 4.0f ? time : 0.0f);
    drawRow(ui, FOOD, v.hunger, last.x + last.w, y, s, glm::vec4(0.72f, 0.42f, 0.18f, 1), glm::vec4(0.95f, 0.92f, 0.85f, 1),
            true, 0.0f);
    if (v.air < Vitals::MAX_AIR) {
        float bubbles = std::ceil(v.air / Vitals::MAX_AIR * 20.0f);
        drawRow(ui, BUBBLE, bubbles, last.x + last.w, y - 9 * s, s, glm::vec4(0.35f, 0.65f, 1.0f, 1),
                glm::vec4(0.9f, 0.97f, 1.0f, 1), true, 0.0f);
    }
    if (eating > 0.0f) {
        float bw = 40 * s, bx = (w - bw) * 0.5f, by = h * 0.5f + 12 * s;
        ui.rect(bx, by, bw, 3 * s, glm::vec4(0, 0, 0, 0.6f));
        ui.rect(bx, by, bw * eating, 3 * s, glm::vec4(0.9f, 0.6f, 0.25f, 1.0f));
    }
}

int Hud::slotAt(int width, int height, glm::vec2 mouse, bool creative) const
{
    for (int i = 0; i < Inventory::SIZE; ++i)
        if (inside(inventorySlotRect(i, width, height, creative), mouse)) return i;
    return -1;
}

Item Hud::catalogAt(int width, int height, glm::vec2 mouse) const
{
    const auto& items = catalog();
    for (int i = 0; i < (int)items.size(); ++i)
        if (inside(catalogSlotRect(i, width, height), mouse)) return items[i];
    return Item::Air;
}

} // namespace cubiq
