#include "Game/Item.h"
#include "Game/BlockAtlas.h"

namespace cubiq {

namespace {
    ItemInfo tool(const char* name, ToolType type, int tier, int damage)
    {
        static const float SPEED[5] = {1.0f, 2.0f, 4.0f, 6.0f, 9.0f};
        static const int DURABILITY[5] = {0, 60, 132, 250, 1500};
        ItemInfo i{name, 1, type, tier, SPEED[tier], damage, DURABILITY[tier], 0};
        return i;
    }

    const ItemInfo& infoTable(int index)
    {
        static const ItemInfo TABLE[ITEM_ICON_COUNT] = {
            {"Palica"},
            {"Uhlie"},
            {"Zelezny ingot"},
            {"Zlaty ingot"},
            {"Ulomok aetheritu"},
            {"Nebesky krystal"},
            {"Tienova esencia"},
            {"Surove maso", 64, ToolType::None, 0, 1.0f, 1, 0, 3},
            {"Pecene maso", 64, ToolType::None, 0, 1.0f, 1, 0, 8},
            {"Jablko", 64, ToolType::None, 0, 1.0f, 1, 0, 4},
            tool("Dreveny krompac", ToolType::Pickaxe, 1, 2), tool("Drevena sekera", ToolType::Axe, 1, 3),
            tool("Drevena lopata", ToolType::Shovel, 1, 2), tool("Dreveny mec", ToolType::Sword, 1, 4),
            tool("Kamenny krompac", ToolType::Pickaxe, 2, 3), tool("Kamenna sekera", ToolType::Axe, 2, 4),
            tool("Kamenna lopata", ToolType::Shovel, 2, 3), tool("Kamenny mec", ToolType::Sword, 2, 5),
            tool("Zelezny krompac", ToolType::Pickaxe, 3, 4), tool("Zelezna sekera", ToolType::Axe, 3, 5),
            tool("Zelezna lopata", ToolType::Shovel, 3, 4), tool("Zelezny mec", ToolType::Sword, 3, 6),
            tool("Aetheritovy krompac", ToolType::Pickaxe, 4, 5), tool("Aetheritovy mec", ToolType::Sword, 4, 8),
            {"Carovna palicka", 1, ToolType::Magic, 0, 1.0f, 2, 0, 0},
            {"Nebeska palica", 1, ToolType::Magic, 0, 1.0f, 4, 0, 0},
            {"Runa svetla", 1}, {"Runa ohna", 1}, {"Runa skoku", 1}, {"Runa levitacie", 1},
            {"Krystal many", 16},
        };
        return TABLE[index];
    }
}

const ItemInfo& itemInfo(Item item)
{
    static const ItemInfo BLOCK{"blok"};
    int v = (int)item;
    if (v >= (int)Item::FirstItem && v < (int)Item::End) return infoTable(v - (int)Item::FirstItem);
    return BLOCK;
}

const char* itemName(Item item)
{
    if (isBlockItem(item)) {
        static const char* NAMES[(int)BlockId::Count] = {
            "Vzduch", "Trava", "Hlina", "Kamen", "Piesok", "Voda", "Zasnezena trava", "Sneh", "Strk",
            "Dubove drevo", "Dubove listie", "Brezove drevo", "Brezove listie", "Smrekove drevo", "Smrekove ihlicie",
            "Ceresnove drevo", "Ceresnove listie", "Kaktus", "Podlozie", "Uhlova ruda", "Zelezna ruda", "Zlata ruda",
            "Aetheritova ruda", "Lava", "Cerveny piesok", "Terakota", "Cervena terakota", "Zlta terakota",
            "Biela terakota", "Hneda terakota", "Dubove dosky", "Smrekove dosky", "Dlazobny kamen",
            "Machova dlazba", "Kamenne tehly", "Machove tehly", "Popraskane tehly", "Sklo", "Lampas",
            "Pracovny stol", "Pec", "Runovy oltar", "Nebeska ruda",
        };
        return NAMES[(int)item];
    }
    return itemInfo(item).name;
}

int itemIconTile(Item item) { return TILE_COUNT + ((int)item - (int)Item::FirstItem); }

MiningInfo miningInfo(BlockId b)
{
    switch (b) {
    case BlockId::Air:
    case BlockId::Water:
    case BlockId::Lava:
    case BlockId::Bedrock: return {-1.0f, ToolType::None, 0};
    case BlockId::Grass:
    case BlockId::SnowyGrass:
    case BlockId::Gravel: return {0.6f, ToolType::Shovel, 0};
    case BlockId::Dirt:
    case BlockId::Sand:
    case BlockId::RedSand: return {0.5f, ToolType::Shovel, 0};
    case BlockId::Snow: return {0.2f, ToolType::Shovel, 0};
    case BlockId::OakLog:
    case BlockId::BirchLog:
    case BlockId::SpruceLog:
    case BlockId::CherryLog:
    case BlockId::OakPlanks:
    case BlockId::SprucePlanks: return {2.0f, ToolType::Axe, 0};
    case BlockId::CraftingTable: return {2.5f, ToolType::Axe, 0};
    case BlockId::OakLeaves:
    case BlockId::BirchLeaves:
    case BlockId::SpruceLeaves:
    case BlockId::CherryLeaves: return {0.2f, ToolType::Axe, 0};
    case BlockId::Cactus: return {0.4f, ToolType::None, 0};
    case BlockId::Glass: return {0.3f, ToolType::None, 0};
    case BlockId::Lantern: return {1.0f, ToolType::Pickaxe, 0};
    case BlockId::Stone:
    case BlockId::StoneBricks:
    case BlockId::MossyStoneBricks:
    case BlockId::CrackedStoneBricks: return {1.5f, ToolType::Pickaxe, 1};
    case BlockId::Cobblestone:
    case BlockId::MossyCobblestone: return {2.0f, ToolType::Pickaxe, 1};
    case BlockId::Terracotta:
    case BlockId::TerracottaRed:
    case BlockId::TerracottaYellow:
    case BlockId::TerracottaWhite:
    case BlockId::TerracottaBrown: return {1.25f, ToolType::Pickaxe, 1};
    case BlockId::Furnace: return {3.5f, ToolType::Pickaxe, 1};
    case BlockId::CoalOre: return {3.0f, ToolType::Pickaxe, 1};
    case BlockId::IronOre: return {3.0f, ToolType::Pickaxe, 2};
    case BlockId::GoldOre: return {3.0f, ToolType::Pickaxe, 3};
    case BlockId::AetheriteOre: return {4.0f, ToolType::Pickaxe, 3};
    case BlockId::RunicAltar: return {4.0f, ToolType::Pickaxe, 3};
    case BlockId::SkyCrystalOre: return {5.0f, ToolType::Pickaxe, 4};
    default: return {1.0f, ToolType::None, 0};
    }
}

float breakTime(BlockId block, Item held, bool& drops)
{
    MiningInfo m = miningInfo(block);
    drops = false;
    if (m.hardness < 0.0f) return -1.0f;
    const ItemInfo& tool = itemInfo(held);
    bool rightTool = m.tool != ToolType::None && tool.tool == m.tool;
    drops = m.minTier == 0 || (rightTool && tool.tier >= m.minTier);
    if (rightTool && tool.tier >= m.minTier) return m.hardness * 1.5f / tool.speed;
    // Bez vhodného nástroja ide všetko pomaly; tvrdé bloky ešte pomalšie a nič z nich nevypadne.
    return m.hardness * (drops ? 1.5f : 5.0f);
}

Item dropOf(BlockId block, float r)
{
    switch (block) {
    case BlockId::Grass:
    case BlockId::SnowyGrass: return itemOf(BlockId::Dirt);
    case BlockId::Stone: return itemOf(BlockId::Cobblestone);
    case BlockId::CoalOre: return Item::Coal;
    case BlockId::AetheriteOre: return Item::AetheriteShard;
    case BlockId::SkyCrystalOre: return Item::SkyCrystal;
    case BlockId::Glass:
    case BlockId::Water:
    case BlockId::Lava:
    case BlockId::Air: return Item::Air;
    case BlockId::OakLeaves:
    case BlockId::BirchLeaves:
    case BlockId::CherryLeaves: return r < 0.08f ? Item::Apple : Item::Air;
    case BlockId::SpruceLeaves: return Item::Air;
    default: return itemOf(block);
    }
}

} // namespace cubiq
