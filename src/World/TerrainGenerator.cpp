#include "World/TerrainGenerator.h"
#include "World/Chunk.h"

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    float smoothstep(float e0, float e1, float x)
    {
        float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // Kontinentálnosť (-1..1) → základná výška. Lineárne medzi bodmi krivky.
    float continentHeight(float c)
    {
        static const float POINTS[][2] = {
            {-1.00f, 28.0f},  // hlboký oceán
            {-0.35f, 40.0f},
            {-0.12f, 55.0f},  // šelf
            {-0.02f, 61.0f},  // pobrežie
            { 0.05f, 64.0f},  // pláž
            { 0.25f, 70.0f},
            { 0.60f, 80.0f},
            { 1.00f, 92.0f},  // vnútrozemie
        };
        const int n = sizeof(POINTS) / sizeof(POINTS[0]);
        if (c <= POINTS[0][0]) return POINTS[0][1];
        for (int i = 1; i < n; ++i) {
            if (c <= POINTS[i][0]) {
                float t = (c - POINTS[i - 1][0]) / (POINTS[i][0] - POINTS[i - 1][0]);
                return POINTS[i - 1][1] + t * (POINTS[i][1] - POINTS[i - 1][1]);
            }
        }
        return POINTS[n - 1][1];
    }
}

const char* biomeName(Biome biome)
{
    static const char* NAMES[(int)Biome::Count] = {
        "Ocean", "Plaz", "Rieka", "Planiny", "Les", "Pust", "Tundra", "Hory", "Zasnezene stity",
    };
    return NAMES[(int)biome];
}

TerrainGenerator::TerrainGenerator(uint32_t seed)
    : m_seed(seed),
      m_offsetX((float)(int)(seed * 2654435761u % 200000u) - 100000.0f),
      m_offsetZ((float)(int)(seed * 2246822519u % 200000u) - 100000.0f),
      m_continent(seed),
      m_mountain(seed + 1),
      m_ridge(seed + 2),
      m_detail(seed + 3),
      m_river(seed + 4),
      m_temperature(seed + 5),
      m_humidity(seed + 6)
{
}

TerrainGenerator::Column TerrainGenerator::sampleColumn(int x, int z) const
{
    const float fx = x + m_offsetX, fz = z + m_offsetZ;

    // 1) Kontinenty: veľké plochy oceánu a pevniny.
    float continent = m_continent.fbm(fx * 0.0012f, fz * 0.0012f, 5);
    float height = continentHeight(continent);
    float land = smoothstep(-0.02f, 0.2f, continent);

    // 2) Hory: maska, kde sú pohoria, a ostré hrebene z ridged noise.
    float mountainMask = smoothstep(0.05f, 0.45f, m_mountain.fbm(fx * 0.0009f, fz * 0.0009f, 3)) * land;
    float ridges = m_ridge.ridged(fx * 0.0045f, fz * 0.0045f, 5);
    // Hrubý tvar terénu bez hrebeňov a detailu – pre teplotu, aby biómy na svahoch neprepínali po blokoch.
    const float smoothHeight = height + mountainMask * 60.0f;
    height += mountainMask * ridges * 115.0f;

    // 3) Detail: malé kopčeky všade, výraznejšie v horách.
    height += m_detail.fbm(fx * 0.012f, fz * 0.012f, 4) * (3.0f + mountainMask * 10.0f);

    // 4) Rieky: tam, kde je noise blízko nuly, sa terén zarezáva pod hladinu mora.
    float riverNoise = std::fabs(m_river.fbm(fx * 0.0018f, fz * 0.0018f, 4));
    float riverWidth = 0.035f * (1.0f - mountainMask * 0.6f); // v horách užšie (kaňony)
    float riverBank = smoothstep(0.0f, riverWidth, riverNoise);
    float riverBed = SEA_LEVEL - 4.0f;
    bool isRiver = false;
    if (height > riverBed && land > 0.0f) {
        float carved = riverBed + (height - riverBed) * riverBank;
        float blend = std::min(1.0f, land * 2.0f); // pri pobreží sa rieka plynule vlieva do mora
        float h = height + (carved - height) * blend;
        isRiver = h < SEA_LEVEL && height >= SEA_LEVEL;
        height = h;
    }

    int h = std::clamp((int)std::floor(height), 1, Chunk::SIZE_Y - 2);

    // Biom: teplota klesá s výškou.
    float temperature = m_temperature.fbm(fx * 0.0007f, fz * 0.0007f, 3) - (smoothHeight - SEA_LEVEL) / 220.0f;
    float humidity = m_humidity.fbm(fx * 0.0009f, fz * 0.0009f, 3);

    // Biom pevniny podľa klímy (nezávisí od výšky jednotlivého stĺpca).
    Biome landBiome;
    if (mountainMask > 0.35f)                        landBiome = Biome::Mountains;
    else if (temperature < -0.3f)                    landBiome = Biome::Tundra;
    else if (temperature > 0.3f && humidity < 0.05f) landBiome = Biome::Desert;
    else if (humidity > 0.15f)                       landBiome = Biome::Forest;
    else                                             landBiome = Biome::Plains;

    // Pláže len v miernej/teplej klíme; v tundre a horách ostáva biom pevniny až k vode.
    bool sandyShore = landBiome == Biome::Plains || landBiome == Biome::Forest || landBiome == Biome::Desert;

    Biome biome;
    if (isRiver)                              biome = Biome::River;
    else if (h < SEA_LEVEL - 1)               biome = Biome::Ocean;
    else if (h <= SEA_LEVEL + 1 && sandyShore) biome = Biome::Beach;
    else if (h > 135)                         biome = Biome::SnowyPeaks;
    else                                      biome = landBiome;

    return {h, biome, temperature < -0.3f};
}

int TerrainGenerator::heightAt(int x, int z) const { return sampleColumn(x, z).height; }
Biome TerrainGenerator::biomeAt(int x, int z) const { return sampleColumn(x, z).biome; }

glm::ivec2 TerrainGenerator::findSpawn() const
{
    // Špirála po 16 blokoch, max. ~2 km od stredu.
    for (int r = 0; r < 128; ++r) {
        for (int i = -r; i <= r; ++i) {
            const glm::ivec2 candidates[4] = {{i, -r}, {i, r}, {-r, i}, {r, i}};
            for (glm::ivec2 c : candidates) {
                Column col = sampleColumn(c.x * 16, c.y * 16);
                if (col.height > SEA_LEVEL + 3 && col.biome != Biome::Mountains && col.biome != Biome::SnowyPeaks)
                    return c * 16;
            }
        }
    }
    return {0, 0};
}

void TerrainGenerator::generate(Chunk& chunk) const
{
    const int baseX = chunk.coord().x * Chunk::SIZE_X;
    const int baseZ = chunk.coord().y * Chunk::SIZE_Z;

    // Výšková mapa s okrajom 1 blok – kvôli sklonu terénu aj na hranách chunku.
    constexpr int W = Chunk::SIZE_X + 2, D = Chunk::SIZE_Z + 2;
    Column cols[D][W];
    for (int z = 0; z < D; ++z)
        for (int x = 0; x < W; ++x)
            cols[z][x] = sampleColumn(baseX + x - 1, baseZ + z - 1);

    for (int z = 0; z < Chunk::SIZE_Z; ++z) {
        for (int x = 0; x < Chunk::SIZE_X; ++x) {
            const Column& col = cols[z + 1][x + 1];
            const int h = col.height;

            // Sklon = najväčší rozdiel výšky voči 4 susedom.
            int slope = std::max({std::abs(h - cols[z + 1][x].height), std::abs(h - cols[z + 1][x + 2].height),
                                  std::abs(h - cols[z][x + 1].height), std::abs(h - cols[z + 2][x + 1].height)});

            // Povrch a vrstva pod ním podľa biómu.
            BlockId top = BlockId::Grass, filler = BlockId::Dirt;
            int fillerDepth = 3;
            switch (col.biome) {
            case Biome::Ocean:
            case Biome::River:
                top = h < SEA_LEVEL - 8 ? BlockId::Gravel : BlockId::Sand;
                filler = top;
                break;
            case Biome::Beach:
            case Biome::Desert:
                top = filler = BlockId::Sand;
                fillerDepth = 4;
                break;
            case Biome::Tundra:
                top = BlockId::SnowyGrass;
                break;
            case Biome::Mountains:
                // Vyššie časti hôr sú holé skaly, v chlade zasnežené.
                if (h > 105) top = filler = BlockId::Stone;
                else if (col.cold) top = BlockId::SnowyGrass;
                break;
            case Biome::SnowyPeaks:
                top = BlockId::Snow;
                filler = BlockId::Stone;
                break;
            default:
                break;
            }

            // Na strmých svahoch sa tráva ani sneh neudržia → holá skala.
            if (slope >= 3 && h > SEA_LEVEL && col.biome != Biome::Desert && col.biome != Biome::Beach)
                top = filler = BlockId::Stone;

            for (int y = 0; y <= h; ++y) {
                BlockId id = y == h               ? top
                           : y > h - fillerDepth  ? filler
                                                  : BlockId::Stone;
                chunk.set(x, y, z, id);
            }
            for (int y = h + 1; y <= SEA_LEVEL; ++y)
                chunk.set(x, y, z, BlockId::Water);
        }
    }
}

} // namespace cubiq
