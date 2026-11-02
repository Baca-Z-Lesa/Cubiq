#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace cubiq {

class World;

// A* po blokovej mriežke. Uzol = blok, v ktorom tvor stojí (pod ním pevný blok, nad ním voľno
// na výšku tela). Dá sa ísť do 8 smerov, vyjsť o 1 blok hore a zoskočiť až o 3 bloky.
// Diagonálne sa nesmie "sekať roh" cez prekážku.
class Pathfinder {
public:
    struct Options {
        int height = 2;        // výška tela v blokoch
        int maxDrop = 3;       // najväčší zoskok
        int maxNodes = 700;    // limit hľadania (aby AI nebrzdila hru)
        bool avoidWater = true;
    };

    // Cesta od start do goal (vrátane oboch), alebo čo najbližšie k cieľu, ak sa nedá dôjsť.
    // Prázdna, ak sa nedá urobiť ani krok.
    static std::vector<glm::ivec3> find(const World& world, glm::ivec3 start, glm::ivec3 goal, const Options& options);

    // Dá sa v tomto bloku stáť?
    static bool standable(const World& world, glm::ivec3 p, const Options& options);
};

} // namespace cubiq
