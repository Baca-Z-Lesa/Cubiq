#pragma once
#include "Game/Crafting.h"
#include "Game/Inventory.h"

#include <glm/vec3.hpp>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cubiq {

// Údaje sveta a hráča v súbore world.dat (čitateľný text: kľúč = hodnota).
struct WorldInfo {
    std::string name;
    std::filesystem::path folder;
    uint32_t seed = 0;
    int64_t created = 0;
    int64_t lastPlayed = 0;
    bool creative = false;  // kreatívny mód: nekonečné bloky, let, okamžité kopanie

    bool hasPlayer = false; // false = nový svet, hráč sa spawne na štandardnom mieste
    glm::vec3 position{0.0f};
    float yaw = -90.0f, pitch = -10.0f;
    bool flying = false;
    double time = 0.08;     // čas sveta v dňoch
    Inventory inventory;
    float health = 20.0f, hunger = 20.0f, saturation = 5.0f; // survival
    std::vector<Furnace> furnaces;
};

// Priečinok so svetmi: %APPDATA%\Cubiq\saves (inak ./saves).
std::filesystem::path savesDirectory();

// Všetky uložené svety, naposledy hrané prvé.
std::vector<WorldInfo> listWorlds();

// Nový svet v novom priečinku (názov priečinka sa odvodí z mena).
WorldInfo createWorld(const std::string& name, uint32_t seed, bool creative);

bool saveWorldInfo(const WorldInfo& info);
bool loadWorldInfo(const std::filesystem::path& folder, WorldInfo& out);
void deleteWorld(const WorldInfo& info);

// Seed z textu: číslo sa použije priamo, iný text sa zahešuje, prázdny = náhodný.
uint32_t seedFromText(const std::string& text);

int64_t nowUnix();
std::string formatDate(int64_t unix);

} // namespace cubiq
