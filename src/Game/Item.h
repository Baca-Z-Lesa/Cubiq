#pragma once
#include "World/Block.h"

#include <cstdint>

namespace cubiq {

// Predmet: hodnoty 0..BlockId::Count-1 sú bloky (rovnaké čísla ako BlockId → staré uložené
// inventáre ostávajú platné), od 256 vyššie sú nástroje, jedlo, suroviny a magické predmety.
enum class Item : uint16_t {
    Air = 0,
    FirstItem = 256,
    Stick = FirstItem,
    Coal,
    IronIngot,
    GoldIngot,
    AetheriteShard,
    SkyCrystal,
    ShadowEssence,
    RawMeat,
    CookedMeat,
    Apple,
    WoodPickaxe, WoodAxe, WoodShovel, WoodSword,
    StonePickaxe, StoneAxe, StoneShovel, StoneSword,
    IronPickaxe, IronAxe, IronShovel, IronSword,
    AetheritePickaxe, AetheriteSword,
    Wand,       // čarovná palička – odomkne manu a kúzla
    SkyStaff,   // nebeská palica – cieľ magického postupu
    RuneLight, RuneFire, RuneBlink, RuneLevitation,
    ManaCrystal,
    End
};

constexpr int ITEM_ICON_COUNT = (int)Item::End - (int)Item::FirstItem;

enum class ToolType : uint8_t { None, Pickaxe, Axe, Shovel, Sword, Magic };

struct ItemInfo {
    const char* name;     // bez diakritiky (pixelové písmo je ASCII)
    int maxStack = 64;
    ToolType tool = ToolType::None;
    int tier = 0;         // 1 drevo, 2 kameň, 3 železo, 4 aetherit
    float speed = 1.0f;   // rýchlosť kopania vhodným nástrojom
    int damage = 1;       // útok
    int durability = 0;   // 0 = nekazí sa
    int food = 0;         // koľko nasýti
};

inline bool isBlockItem(Item i) { return (uint16_t)i > 0 && (uint16_t)i < (uint16_t)BlockId::Count; }
inline Item itemOf(BlockId b) { return (Item)(uint16_t)b; }
inline BlockId blockOf(Item i) { return isBlockItem(i) ? (BlockId)(uint16_t)i : BlockId::Air; }
inline bool isValidItem(int v) { return (v > 0 && v < (int)BlockId::Count) || (v >= (int)Item::FirstItem && v < (int)Item::End); }

const ItemInfo& itemInfo(Item item);
const char* itemName(Item item);
// Index dlaždice ikony v atlase (len pre nebloky).
int itemIconTile(Item item);

// --- Kopanie ---
struct MiningInfo {
    float hardness;      // < 0 = nedá sa vykopať
    ToolType tool;       // vhodný nástroj
    int minTier;         // 0 = vypadne aj rukou; inak treba vhodný nástroj aspoň tejto úrovne
};
MiningInfo miningInfo(BlockId block);

// Čas vykopania bloku daným predmetom v ruke (sekundy) a či z neho niečo vypadne.
float breakTime(BlockId block, Item held, bool& drops);

// Čo z bloku vypadne (Air = nič). random01 = náhodné číslo 0..1 (napr. jablko z listov).
Item dropOf(BlockId block, float random01);

} // namespace cubiq
