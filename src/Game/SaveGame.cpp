#include "Game/SaveGame.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>

namespace cubiq {

namespace fs = std::filesystem;

int64_t nowUnix()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string formatDate(int64_t unix)
{
    std::time_t t = (std::time_t)unix;
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M", &tm);
    return buf;
}

fs::path savesDirectory()
{
    const char* appdata = std::getenv("APPDATA");
    fs::path dir = appdata ? fs::path(appdata) / "Cubiq" / "saves" : fs::path("saves");
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

uint32_t seedFromText(const std::string& text)
{
    if (text.empty()) return std::random_device{}();
    bool numeric = std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
    if (numeric && text.size() <= 10) return (uint32_t)std::strtoull(text.c_str(), nullptr, 10);
    uint32_t h = 2166136261u; // FNV-1a
    for (char c : text) h = (h ^ (uint8_t)c) * 16777619u;
    return h;
}

bool saveWorldInfo(const WorldInfo& w)
{
    std::error_code ec;
    fs::create_directories(w.folder, ec);
    fs::path tmp = w.folder / "world.dat.tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f) return false;
        f << "name=" << w.name << "\n";
        f << "seed=" << w.seed << "\n";
        f << "created=" << w.created << "\n";
        f << "lastPlayed=" << w.lastPlayed << "\n";
        f << "time=" << w.time << "\n";
        f << "mode=" << (w.creative ? "creative" : "survival") << "\n";
        if (w.hasPlayer) {
            f << "player=" << w.position.x << " " << w.position.y << " " << w.position.z << "\n";
            f << "look=" << w.yaw << " " << w.pitch << "\n";
            f << "flying=" << (w.flying ? 1 : 0) << "\n";
            f << "selected=" << w.inventory.selected() << "\n";
            for (int i = 0; i < Inventory::SIZE; ++i) {
                const ItemStack& s = w.inventory.slot(i);
                if (!s.empty()) f << "slot=" << i << " " << (int)s.id << " " << s.count << " " << s.durability << "\n";
            }
        }
    }
    fs::rename(tmp, w.folder / "world.dat", ec);
    return !ec;
}

bool loadWorldInfo(const fs::path& folder, WorldInfo& w)
{
    std::ifstream f(folder / "world.dat");
    if (!f) return false;
    w = WorldInfo{};
    w.folder = folder;
    w.creative = true; // svety spred survivalu nemajú kľúč mode – boli kreatívne
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::istringstream v(line.substr(eq + 1));
        if (key == "name") w.name = line.substr(eq + 1);
        else if (key == "seed") v >> w.seed;
        else if (key == "created") v >> w.created;
        else if (key == "lastPlayed") v >> w.lastPlayed;
        else if (key == "time") v >> w.time;
        else if (key == "mode") w.creative = line.substr(eq + 1) == "creative";
        else if (key == "player") { v >> w.position.x >> w.position.y >> w.position.z; w.hasPlayer = true; }
        else if (key == "look") v >> w.yaw >> w.pitch;
        else if (key == "flying") { int b = 0; v >> b; w.flying = b != 0; }
        else if (key == "selected") { int s = 0; v >> s; w.inventory.select(s); }
        else if (key == "slot") {
            int i = -1, id = 0, count = 0, durability = -1;
            v >> i >> id >> count >> durability;
            if (i >= 0 && i < Inventory::SIZE && isValidItem(id) && count > 0) {
                ItemStack& s = w.inventory.slot(i);
                s = ItemStack::of((Item)id, std::min(count, itemInfo((Item)id).maxStack));
                if (durability > 0) s.durability = std::min(durability, itemInfo((Item)id).durability);
            }
        }
    }
    if (w.name.empty()) w.name = folder.filename().string();
    return true;
}

std::vector<WorldInfo> listWorlds()
{
    std::vector<WorldInfo> worlds;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(savesDirectory(), ec)) {
        if (!entry.is_directory()) continue;
        WorldInfo w;
        if (loadWorldInfo(entry.path(), w)) worlds.push_back(std::move(w));
    }
    std::sort(worlds.begin(), worlds.end(), [](const WorldInfo& a, const WorldInfo& b) { return a.lastPlayed > b.lastPlayed; });
    return worlds;
}

WorldInfo createWorld(const std::string& name, uint32_t seed, bool creative)
{
    // Názov priečinka: len bezpečné znaky, pri zhode pridáme číslo.
    std::string base;
    for (char c : name) base += (std::isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
    if (base.empty()) base = "svet";
    fs::path dir = savesDirectory() / base;
    for (int n = 2; fs::exists(dir); ++n) dir = savesDirectory() / (base + "_" + std::to_string(n));

    WorldInfo w;
    w.name = name.empty() ? "Novy svet" : name;
    w.folder = dir;
    w.seed = seed;
    w.creative = creative;
    w.created = w.lastPlayed = nowUnix();
    saveWorldInfo(w);
    return w;
}

void deleteWorld(const WorldInfo& info)
{
    std::error_code ec;
    // Poistka: mažeme len priečinky vnútri priečinka so svetmi.
    fs::path saves = fs::weakly_canonical(savesDirectory(), ec), target = fs::weakly_canonical(info.folder, ec);
    if (target.parent_path() == saves) fs::remove_all(target, ec);
}

} // namespace cubiq
