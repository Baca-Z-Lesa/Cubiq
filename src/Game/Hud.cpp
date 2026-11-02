#include "Game/Hud.h"
#include "Engine/UIRenderer.h"
#include "Game/BlockAtlas.h"
#include "Game/Vitals.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace cubiq {

namespace {
    const glm::vec4 SLOT_BG(0.0f, 0.0f, 0.0f, 0.45f);
    const glm::vec4 SLOT_BORDER(1.0f, 1.0f, 1.0f, 0.18f);
    const glm::vec4 WHITE(1.0f);
    const glm::vec4 SHADOW(0.0f, 0.0f, 0.0f, 0.8f);
    const glm::vec4 PANEL(0.12f, 0.12f, 0.14f, 0.92f);
    const glm::vec4 TITLE(0.75f, 0.7f, 1.0f, 1.0f);

    constexpr int CATALOG_COLUMNS = 15;
    constexpr int BOOK_COLUMNS = 4;

    // Veľkosť "UI pixelu" podľa výšky okna (720p → 3).
    float uiScale(int height) { return std::max(2.0f, std::round(height / 240.0f)); }

    struct Rect { float x, y, w, h; };

    bool inside(Rect r, glm::vec2 m) { return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; }

    float catalogHeight(float s)
    {
        int rows = ((int)Hud::catalog().size() + CATALOG_COLUMNS - 1) / CATALOG_COLUMNS;
        return rows * 16.0f * s;
    }

    bool showsCatalog(const UiState& st) { return st.creative && st.open == Container::Inventory; }
    bool showsCrafting(const UiState& st)
    {
        return st.grid && !showsCatalog(st) &&
               (st.open == Container::Inventory || st.open == Container::Table || st.open == Container::Altar);
    }

    // Rozloženie otvoreného UI: horná časť (katalóg alebo kontajner), pod ňou inventár,
    // naľavo kniha receptov.
    struct Layout {
        float s, slot;
        float invX, invY;   // ľavý horný roh hlavného inventára
        float topY;         // začiatok hornej časti
        float topH;

        Layout(int width, int height, const UiState& st)
        {
            s = uiScale(height);
            slot = 20.0f * s;
            const float invH = 4 * slot + 6 * s;
            topH = showsCatalog(st) ? catalogHeight(s) : st.open != Container::None ? 3 * slot : 0.0f;
            const float gap = topH > 0 ? 18 * s : 0.0f;
            invX = (width - 9 * slot) * 0.5f;
            invY = (height - invH + topH + gap) * 0.5f;
            topY = invY - gap - topH;
        }

        Rect inventorySlot(int i) const
        {
            if (i < Inventory::HOTBAR) return {invX + i * slot, invY + 3 * slot + 6 * s, slot, slot};
            int j = i - Inventory::HOTBAR;
            return {invX + (j % 9) * slot, invY + (j / 9) * slot, slot, slot};
        }
        Rect catalogSlot(int i, int width) const
        {
            float cs = 16.0f * s;
            float x0 = (width - CATALOG_COLUMNS * cs) * 0.5f;
            return {x0 + (i % CATALOG_COLUMNS) * cs, topY + (i / CATALOG_COLUMNS) * cs, cs, cs};
        }
        Rect craftCell(int x, int y, int size) const
        {
            float oy = topY + (3 - size) * slot * 0.5f;
            return {invX + slot * 1.5f + x * slot, oy + y * slot, slot, slot};
        }
        Rect result() const { return {invX + slot * 6.0f, topY + slot, slot, slot}; }
        Rect furnaceInput() const { return {invX + slot * 2.0f, topY, slot, slot}; }
        Rect furnaceFuel() const { return {invX + slot * 2.0f, topY + 2 * slot, slot, slot}; }
        Rect furnaceOutput() const { return {invX + slot * 6.0f, topY + slot, slot, slot}; }
        Rect bookSlot(int i) const
        {
            float bs = 16.0f * s;
            float x0 = invX - 10 * s - BOOK_COLUMNS * bs;
            return {x0 + (i % BOOK_COLUMNS) * bs, topY + 10 * s + (i / BOOK_COLUMNS) * bs, bs, bs};
        }
    };

    Rect hotbarSlotRect(int i, int width, int height)
    {
        float s = uiScale(height);
        float slot = 20.0f * s;
        float x0 = (width - 9 * slot) * 0.5f;
        float y0 = height - slot - 4 * s;
        return {x0 + i * slot, y0, slot, slot};
    }

    // Izometrická ikona bloku: vrch + dva boky, každá stena inak tieňovaná.
    void drawBlockIcon(UIRenderer& ui, BlockId id, glm::vec2 c, float r, float bright)
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
            shade *= bright;
            ui.quad(p, uv, glm::vec4(shade, shade, shade, alpha), true);
        };
        face(top, ur, c, ul, type.tileTop, 1.0f);
        face(ul, c, bottom, ll, type.tileSide, 0.75f);
        face(c, ur, lr, bottom, type.tileSide, 0.6f);
    }

    void drawItemShaded(UIRenderer& ui, const ItemStack& stack, float cx, float cy, float size, float bright)
    {
        if (stack.empty()) return;
        if (isBlockItem(stack.id)) {
            drawBlockIcon(ui, blockOf(stack.id), {cx, cy - size / 16.0f}, size * 0.44f, bright);
        } else {
            int tile = itemIconTile(stack.id);
            float u0 = (float)tile / ATLAS_TILES, u1 = (float)(tile + 1) / ATLAS_TILES;
            float h = size * 0.5f;
            const glm::vec2 p[4] = {{cx - h, cy - h}, {cx + h, cy - h}, {cx + h, cy + h}, {cx - h, cy + h}};
            const glm::vec2 uv[4] = {{u0, 0.0f}, {u1, 0.0f}, {u1, 1.0f}, {u0, 1.0f}};
            ui.quad(p, uv, glm::vec4(bright, bright, bright, 1.0f), true);
        }
    }

    void drawStack(UIRenderer& ui, const ItemStack& stack, Rect r, float s, float bright = 1.0f)
    {
        if (stack.empty()) return;
        if (bright < 1.0f) drawItemShaded(ui, stack, r.x + r.w * 0.5f, r.y + r.h * 0.5f, 16.0f * s * (r.w / (20.0f * s)), bright);
        else Hud::drawItem(ui, stack, r.x + r.w * 0.5f, r.y + r.h * 0.5f, 16.0f * s * (r.w / (20.0f * s)));
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

    // Šípka (výroba / tavenie), fill 0..1 = priebeh.
    void drawArrow(UIRenderer& ui, float x, float y, float w, float s, float fill, glm::vec4 color)
    {
        const glm::vec4 bg(1, 1, 1, 0.25f);
        ui.rect(x, y - s, w - 4 * s, 2 * s, bg);
        for (int i = 0; i < 4; ++i) ui.rect(x + w - 4 * s + i * s, y - (4 - i) * s, s, 2 * (4 - i) * s, bg);
        if (fill > 0.0f) ui.rect(x, y - s, (w - 4 * s) * std::min(fill, 1.0f), 2 * s, color);
    }

    const char* containerTitle(Container c)
    {
        switch (c) {
        case Container::Table: return "Pracovny stol";
        case Container::Altar: return "Runovy oltar";
        case Container::Furnace: return "Pec";
        default: return "Vyroba";
        }
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

std::vector<int> Hud::bookRecipes(const CraftGrid& grid)
{
    std::vector<int> out;
    const auto& recipes = allRecipes();
    for (int i = 0; i < (int)recipes.size(); ++i)
        if (availableAt(recipes[i], grid.station) && recipeFits(recipes[i], grid.size)) out.push_back(i);
    return out;
}

void Hud::drawItem(UIRenderer& ui, const ItemStack& stack, float cx, float cy, float size)
{
    if (stack.empty()) return;
    drawItemShaded(ui, stack, cx, cy, size, 1.0f);
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

void Hud::draw(UIRenderer& ui, const UiState& st, glm::vec2 mouse) const
{
    const int w = ui.width(), h = ui.height();
    const float s = uiScale(h);
    const Inventory& inventory = *st.inventory;

    if (st.open == Container::None) {
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

    const Layout L(w, h, st);
    const UiTarget target = targetAt(w, h, mouse, st);
    const float pad = 6 * s;
    std::string tooltip;
    auto hovered = [&](UiTarget::Kind kind, int index = 0) { return target.kind == kind && target.index == index; };

    // Obrazovka inventára
    ui.rect(0, 0, (float)w, (float)h, glm::vec4(0, 0, 0, 0.5f));
    {
        Rect first = L.inventorySlot(Inventory::HOTBAR), last = L.inventorySlot(Inventory::HOTBAR - 1);
        float top = showsCatalog(st) ? first.y : L.topY - 10 * s;
        ui.rect(first.x - pad, top - pad, 9 * L.slot + 2 * pad, last.y + last.h - top + 2 * pad, PANEL);
    }
    for (int i = 0; i < Inventory::SIZE; ++i) {
        Rect r = L.inventorySlot(i);
        drawSlot(ui, r, s, hovered(UiTarget::Slot, i));
        drawStack(ui, inventory.slot(i), r, s);
    }
    if (target.kind == UiTarget::Slot && !inventory.slot(target.index).empty()) tooltip = itemName(inventory.slot(target.index).id);

    if (showsCatalog(st)) {
        const auto& items = catalog();
        Rect c0 = L.catalogSlot(0, w);
        float cw = CATALOG_COLUMNS * c0.w, ch = catalogHeight(s);
        ui.rect(c0.x - pad, c0.y - pad - 10 * s, cw + 2 * pad, ch + 2 * pad + 10 * s, glm::vec4(0.1f, 0.09f, 0.16f, 0.92f));
        ui.text("Katalog (kreativny mod)", c0.x, c0.y - 10 * s, s, TITLE);
        for (int i = 0; i < (int)items.size(); ++i) {
            Rect r = L.catalogSlot(i, w);
            drawSlot(ui, r, s, hovered(UiTarget::Catalog, i));
            drawStack(ui, ItemStack::of(items[i]), r, s);
        }
        if (target.kind == UiTarget::Catalog) tooltip = itemName(target.item);
    } else {
        ui.text(containerTitle(st.open), L.invX, L.topY - 9 * s, s, TITLE);
    }

    // --- Crafting: mriežka, šípka, výsledok a kniha receptov.
    if (showsCrafting(st)) {
        const CraftGrid& grid = *st.grid;
        for (int y = 0; y < grid.size; ++y)
            for (int x = 0; x < grid.size; ++x) {
                Rect r = L.craftCell(x, y, grid.size);
                drawSlot(ui, r, s, hovered(UiTarget::Craft, y * 3 + x));
                drawStack(ui, grid.at(x, y), r, s);
                if (hovered(UiTarget::Craft, y * 3 + x) && !grid.at(x, y).empty()) tooltip = itemName(grid.at(x, y).id);
            }
        const Recipe* recipe = matchRecipe(grid);
        Rect res = L.result();
        float ax = L.craftCell(grid.size - 1, 0, grid.size).x + L.slot + 4 * s;
        drawArrow(ui, ax, res.y + res.h * 0.5f, res.x - ax - 4 * s, s, recipe ? 1.0f : 0.0f, glm::vec4(0.6f, 0.9f, 0.5f, 1.0f));
        drawSlot(ui, res, s, hovered(UiTarget::Result));
        ui.rectOutline(res.x - s, res.y - s, res.w + 2 * s, res.h + 2 * s, s * 0.5f, glm::vec4(1, 1, 1, 0.35f));
        if (recipe) {
            drawStack(ui, recipe->result, res, s);
            if (hovered(UiTarget::Result)) tooltip = itemName(recipe->result.id);
        }

        // Kniha receptov
        const std::vector<int> book = bookRecipes(grid);
        if (!book.empty()) {
            Rect b0 = L.bookSlot(0);
            int rows = ((int)book.size() + BOOK_COLUMNS - 1) / BOOK_COLUMNS;
            ui.rect(b0.x - pad, b0.y - pad - 10 * s, BOOK_COLUMNS * b0.w + 2 * pad, rows * b0.h + 2 * pad + 10 * s,
                    glm::vec4(0.16f, 0.11f, 0.08f, 0.94f));
            ui.text("Recepty", b0.x, b0.y - 10 * s, s, glm::vec4(1.0f, 0.85f, 0.6f, 1.0f));
            for (int i = 0; i < (int)book.size(); ++i) {
                const Recipe& r = allRecipes()[book[i]];
                const bool ok = canCraftFrom(r, inventory, grid);
                Rect rr = L.bookSlot(i);
                drawSlot(ui, rr, s, hovered(UiTarget::Book, i));
                if (ok) ui.rect(rr.x + s * 0.5f, rr.y + s * 0.5f, rr.w - s, rr.h - s, glm::vec4(0.3f, 0.6f, 0.25f, 0.35f));
                drawStack(ui, r.result, rr, s, ok ? 1.0f : 0.35f);
                if (hovered(UiTarget::Book, i)) {
                    tooltip = itemName(r.result.id);
                    tooltip += ok ? "  (klik = vlozit)" : "  (chybaju suroviny)";
                }
            }
        }
    }

    // --- Pec
    if (st.open == Container::Furnace && st.furnace) {
        const Furnace& f = *st.furnace;
        const struct { Rect r; const ItemStack& stack; UiTarget::Kind kind; } slots[3] = {
            {L.furnaceInput(), f.input, UiTarget::FurnaceInput},
            {L.furnaceFuel(), f.fuel, UiTarget::FurnaceFuel},
            {L.furnaceOutput(), f.output, UiTarget::FurnaceOutput},
        };
        for (const auto& sl : slots) {
            drawSlot(ui, sl.r, s, hovered(sl.kind));
            drawStack(ui, sl.stack, sl.r, s);
            if (hovered(sl.kind) && !sl.stack.empty()) tooltip = itemName(sl.stack.id);
        }
        // Plameň: výška podľa zostávajúceho paliva.
        Rect in = L.furnaceInput();
        float fx = in.x + 6 * s, fy = in.y + L.slot + 3 * s, fw = 8 * s, fh = L.slot - 6 * s;
        ui.rect(fx, fy, fw, fh, glm::vec4(1, 1, 1, 0.12f));
        if (f.burning()) {
            float k = f.burnLeft / std::max(f.burnTotal, 0.001f);
            ui.rect(fx, fy + fh * (1.0f - k), fw, fh * k, glm::vec4(1.0f, 0.55f, 0.12f, 1.0f));
            ui.rect(fx + 2 * s, fy + fh * (1.0f - k) + 2 * s, fw - 4 * s, std::max(0.0f, fh * k - 2 * s), glm::vec4(1.0f, 0.85f, 0.3f, 1.0f));
        }
        Rect out = L.furnaceOutput();
        float ax = in.x + L.slot + 6 * s;
        drawArrow(ui, ax, out.y + out.h * 0.5f, out.x - ax - 6 * s, s, f.progress / Furnace::SMELT_TIME, glm::vec4(1.0f, 0.75f, 0.3f, 1.0f));
        ui.text("palivo", in.x + L.slot + 3 * s, L.furnaceFuel().y + 7 * s, s, glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    }

    // Stack "v ruke" sa ťahá za myšou.
    if (st.held && !st.held->empty()) {
        float slot = 20.0f * s;
        drawStack(ui, *st.held, {mouse.x - slot * 0.5f, mouse.y - slot * 0.5f, slot, slot}, s);
    } else if (!tooltip.empty()) {
        drawTooltip(ui, tooltip.c_str(), mouse, s);
    }
}

UiTarget Hud::targetAt(int width, int height, glm::vec2 mouse, const UiState& st) const
{
    UiTarget t;
    if (st.open == Container::None) return t;
    const Layout L(width, height, st);
    for (int i = 0; i < Inventory::SIZE; ++i)
        if (inside(L.inventorySlot(i), mouse)) return {UiTarget::Slot, i};

    if (showsCatalog(st)) {
        const auto& items = catalog();
        for (int i = 0; i < (int)items.size(); ++i)
            if (inside(L.catalogSlot(i, width), mouse)) return {UiTarget::Catalog, i, items[i]};
    }
    if (showsCrafting(st)) {
        const CraftGrid& grid = *st.grid;
        for (int y = 0; y < grid.size; ++y)
            for (int x = 0; x < grid.size; ++x)
                if (inside(L.craftCell(x, y, grid.size), mouse)) return {UiTarget::Craft, y * 3 + x};
        if (inside(L.result(), mouse)) return {UiTarget::Result};
        const int count = (int)bookRecipes(grid).size();
        for (int i = 0; i < count; ++i)
            if (inside(L.bookSlot(i), mouse)) return {UiTarget::Book, i};
    }
    if (st.open == Container::Furnace) {
        if (inside(L.furnaceInput(), mouse)) return {UiTarget::FurnaceInput};
        if (inside(L.furnaceFuel(), mouse)) return {UiTarget::FurnaceFuel};
        if (inside(L.furnaceOutput(), mouse)) return {UiTarget::FurnaceOutput};
    }
    return t;
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

} // namespace cubiq
