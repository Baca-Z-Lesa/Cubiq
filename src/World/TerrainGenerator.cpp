#include "World/TerrainGenerator.h"
#include "World/Chunk.h"
#include "World/Features.h"
#include "World/Structures.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace cubiq {

namespace {
    // Mriežka 3D noise: hodnota sa počíta len v každom 4. bloku (vo Y 8.) a medzi tým sa interpoluje.
    // Je zarovnaná na svetové súradnice, takže susedné chunky aj priame dopyty dostanú rovnaké čísla.
    constexpr int LX = 4, LY = 8, LZ = 4;
    constexpr int CAVE_LY = 4; // jaskyne potrebujú jemnejšiu mriežku vo Y

    float smoothstep(float e0, float e1, float x)
    {
        float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // a*(1-t) + b*t – pri t = 0 a t = 1 vráti presne a / b (dôležité pre zhodu medzi chunkami).
    float lerp(float a, float b, float t) { return a * (1.0f - t) + b * t; }

    int floorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }

    float trilinear(const float c[8], float fx, float fy, float fz)
    {
        float x00 = lerp(c[0], c[1], fx), x10 = lerp(c[2], c[3], fx);
        float x01 = lerp(c[4], c[5], fx), x11 = lerp(c[6], c[7], fx);
        return lerp(lerp(x00, x10, fy), lerp(x01, x11, fy), fz);
    }

    // Priamy dopyt na interpolovanú hodnotu v jednom bode (pre stromy a spawn).
    template <typename F>
    float latticeSample(F&& f, int x, int y, int z, int sx, int sy, int sz)
    {
        int x0 = floorDiv(x, sx) * sx, y0 = floorDiv(y, sy) * sy, z0 = floorDiv(z, sz) * sz;
        float c[8];
        for (int dz = 0; dz < 2; ++dz)
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx)
                    c[(dz * 2 + dy) * 2 + dx] = f(x0 + dx * sx, y0 + dy * sy, z0 + dz * sz);
        return trilinear(c, (float)(x - x0) / sx, (float)(y - y0) / sy, (float)(z - z0) / sz);
    }

    // Predpočítaná mriežka pre celý chunk (rýchle hromadné dopyty pri generovaní).
    struct Lattice {
        int ox = 0, oy = 0, oz = 0, sx = 1, sy = 1, sz = 1, nx = 0, ny = 0, nz = 0;
        std::vector<float> v;

        template <typename F>
        void fill(F&& f, int originX, int originZ, int sizeY, int stepX, int stepY, int stepZ)
        {
            ox = originX; oy = 0; oz = originZ;
            sx = stepX; sy = stepY; sz = stepZ;
            nx = Chunk::SIZE_X / sx + 1; ny = sizeY / sy + 1; nz = Chunk::SIZE_Z / sz + 1;
            v.resize((size_t)nx * ny * nz);
            for (int k = 0; k < nz; ++k)
                for (int j = 0; j < ny; ++j)
                    for (int i = 0; i < nx; ++i)
                        v[((size_t)k * ny + j) * nx + i] = f(ox + i * sx, oy + j * sy, oz + k * sz);
        }

        float at(int i, int j, int k) const { return v[((size_t)k * ny + j) * nx + i]; }

        float sample(int x, int y, int z) const
        {
            int lx = x - ox, ly = y - oy, lz = z - oz;
            int i = lx / sx, j = ly / sy, k = lz / sz;
            float c[8];
            for (int dz = 0; dz < 2; ++dz)
                for (int dy = 0; dy < 2; ++dy)
                    for (int dx = 0; dx < 2; ++dx)
                        c[(dz * 2 + dy) * 2 + dx] = at(i + dx, j + dy, k + dz);
            return trilinear(c, (float)(lx - i * sx) / sx, (float)(ly - j * sy) / sy, (float)(lz - k * sz) / sz);
        }
    };

    // Kontinentálnosť (-1..1) → základná výška. Lineárne medzi bodmi krivky.
    float continentHeight(float c)
    {
        static const float POINTS[][2] = {
            {-1.00f, 26.0f},  // hlboký oceán
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

    glm::vec3 mix3(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * t; }

    // Farba trávy podľa klímy: studená modrozelená → mierna → suchá žltkastá / vlhká sýta.
    glm::vec3 climateTint(float temperature, float humidity)
    {
        const glm::vec3 COLD(0.42f, 0.62f, 0.50f), TEMPERATE(0.48f, 0.76f, 0.30f);
        const glm::vec3 DRY(0.76f, 0.72f, 0.36f), LUSH(0.28f, 0.68f, 0.20f);
        float t = std::clamp(temperature * 1.4f + 0.5f, 0.0f, 1.0f);
        float h = std::clamp(humidity * 1.4f + 0.5f, 0.0f, 1.0f);
        glm::vec3 warm = mix3(DRY, LUSH, h);
        return mix3(COLD, mix3(TEMPERATE, warm, t), smoothstep(0.0f, 0.6f, t));
    }

    uint32_t hash3(int x, int y, int z, uint32_t seed)
    {
        uint32_t h = seed ^ (uint32_t)(x * 374761393) ^ (uint32_t)(y * 668265263) ^ (uint32_t)(z * 2246822519u);
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }
}

const char* biomeName(Biome biome)
{
    static const char* NAMES[(int)Biome::Count] = {
        "Ocean", "Plaz", "Rieka", "Planiny", "Les", "Brezovy les", "Ceresnovy haj",
        "Pust", "Mesa", "Tundra", "Hory", "Zasnezene stity",
    };
    return NAMES[(int)biome];
}

TerrainGenerator::TerrainGenerator(uint32_t seed)
    : m_seed(seed),
      m_offsetX((float)(int)(seed * 2654435761u % 200000u) - 100000.0f),
      m_offsetZ((float)(int)(seed * 2246822519u % 200000u) - 100000.0f),
      m_continent(seed), m_mountain(seed + 1), m_ridge(seed + 2), m_detail(seed + 3), m_river(seed + 4),
      m_temperature(seed + 5), m_humidity(seed + 6), m_warp(seed + 7), m_mesa(seed + 8), m_spire(seed + 9),
      m_island(seed + 10), m_cherry(seed + 11), m_snow(seed + 12), m_strata(seed + 13),
      m_overhang3(seed + 14), m_island3(seed + 15), m_caveA(seed + 16), m_caveB(seed + 17), m_caveC(seed + 18)
{
}

float TerrainGenerator::random01(int x, int z, uint32_t salt) const
{
    return (hash3(x, (int)salt, z, m_seed) & 0xFFFFFF) / 16777216.0f;
}

// ------------------------------------------------------------------ 2D stĺpec

TerrainGenerator::Column TerrainGenerator::columnAt(int x, int z) const
{
    const float fx = x + m_offsetX, fz = z + m_offsetZ;

    // Domain warping: súradnice sa pred čítaním noise "pokrútia", takže pobrežia,
    // pohoria aj biómy majú organické, zakrivené tvary namiesto okrúhlych fľakov.
    float wx = fx + 70.0f * m_warp.fbm(fx * 0.0025f, fz * 0.0025f, 3);
    float wz = fz + 70.0f * m_warp.fbm(fx * 0.0025f + 71.3f, fz * 0.0025f - 23.9f, 3);

    // 1) Kontinenty
    float continent = m_continent.fbm(wx * 0.0011f, wz * 0.0011f, 5);
    float height = continentHeight(continent);
    float land = smoothstep(-0.02f, 0.2f, continent);

    float temperature = m_temperature.fbm(wx * 0.0007f, wz * 0.0007f, 3);
    float humidity = m_humidity.fbm(wx * 0.0009f, wz * 0.0009f, 3);

    // Mesa (badlands) – horúce a suché vnútrozemie.
    float badlands = smoothstep(0.26f, 0.40f, temperature) * smoothstep(0.0f, -0.15f, humidity) *
                     smoothstep(0.1f, 0.35f, continent);

    // 2) Hory: maska pohorí × ostré hrebene. Kubická zložka robí vysoké špicaté štíty.
    float mountainMask = smoothstep(0.0f, 0.4f, m_mountain.fbm(wx * 0.0008f, wz * 0.0008f, 3)) * land * (1.0f - badlands);
    float ridges = m_ridge.ridged(wx * 0.0035f, wz * 0.0035f, 6);
    const float smoothHeight = height + mountainMask * 60.0f; // pre teplotu – bez ostrých hrebeňov
    height += mountainMask * (ridges * 70.0f + ridges * ridges * ridges * 65.0f);

    // 3) Detail
    height += m_detail.fbm(wx * 0.01f, wz * 0.01f, 4) * (2.5f + land * 2.5f + mountainMask * 8.0f);

    // 4) Mesa: náhorné plošiny s terasami a osamelé skalné veže.
    if (badlands > 0.0f) {
        float plateau = smoothstep(-0.05f, 0.25f, m_mesa.fbm(wx * 0.0045f, wz * 0.0045f, 3));
        float mesa = SEA_LEVEL + 6.0f + plateau * 44.0f;
        const float step = 6.0f;
        float k = mesa / step;
        mesa = (std::floor(k) + smoothstep(0.7f, 1.0f, k - std::floor(k))) * step;
        // Osamelé skalné veže – zriedkavé a aspoň pár blokov široké.
        mesa += smoothstep(0.74f, 0.9f, m_spire.noise(wx * 0.035f, wz * 0.035f)) * 34.0f;
        height += (mesa - height) * badlands;
    }

    // Vrcholy nad 200 zmäkčíme, aby sa zmestili do 256 blokov.
    if (height > 200.0f) height = 200.0f + (height - 200.0f) * 0.5f;

    // 5) Rieky – iba v nížinách. Smerom do vysočín, hôr a mesy slabnú a končia,
    //    takže už nikdy nerozrežú horu na polovicu.
    float riverNoise = std::fabs(m_river.fbm(wx * 0.0016f, wz * 0.0016f, 4));
    float bank = smoothstep(0.0f, 0.032f, riverNoise);
    float lowland = smoothstep(SEA_LEVEL + 24.0f, SEA_LEVEL + 9.0f, height) *
                    (1.0f - smoothstep(0.04f, 0.18f, mountainMask)) * (1.0f - badlands);
    float carve = (1.0f - bank) * lowland * std::min(1.0f, land * 3.0f);
    bool isRiver = false;
    if (carve > 0.0f) {
        const float riverBed = SEA_LEVEL - 4.0f;
        float h = height + (riverBed - height) * carve;
        isRiver = h < SEA_LEVEL && height >= SEA_LEVEL;
        height = h;
    }

    Column c;
    c.height = std::clamp((int)std::floor(height), 1, Chunk::SIZE_Y - 24);

    // Lietajúce ostrovy – zriedkavé, nie nad horami ani mesou.
    float islandMask = smoothstep(0.40f, 0.66f, m_island.fbm(fx * 0.0016f, fz * 0.0016f, 3)) *
                       (1.0f - mountainMask) * (1.0f - badlands);
    // Jednotlivé ostrovy (~50–100 blokov) s medzerami medzi nimi.
    c.islandShape = islandMask * smoothstep(-0.05f, 0.35f, m_island.fbm(fx * 0.012f + 300.0f, fz * 0.012f, 3));
    c.islandCenter = 172 + (int)(14.0f * m_island.fbm(fx * 0.006f - 500.0f, fz * 0.006f, 2));
    c.snowLine = 150 + (int)(14.0f * m_snow.fbm(fx * 0.01f, fz * 0.01f, 2));

    // 3D tvarovanie: silné v horách, jemné v mese (inak by rozdrobilo veže a terasy).
    c.overhang = std::clamp(std::max(mountainMask * 1.2f - 0.1f, smoothstep(0.5f, 1.0f, badlands) * 0.25f), 0.0f, 1.0f);

    // Biom: teplota klesá s (vyhladenou) výškou.
    temperature -= (smoothHeight - SEA_LEVEL) / 220.0f;
    float cherry = m_cherry.fbm(wx * 0.002f, wz * 0.002f, 2);

    Biome landBiome;
    if (badlands > 0.5f)                                          landBiome = Biome::Badlands;
    else if (mountainMask > 0.35f)                                landBiome = Biome::Mountains;
    else if (temperature < -0.3f)                                 landBiome = Biome::Tundra;
    else if (temperature > 0.3f && humidity < 0.05f)              landBiome = Biome::Desert;
    else if (cherry > 0.32f && humidity > -0.05f && temperature > -0.15f) landBiome = Biome::CherryGrove;
    else if (humidity > 0.15f)                                    landBiome = temperature < 0.0f ? Biome::BirchForest : Biome::Forest;
    else                                                          landBiome = Biome::Plains;

    // Pláže len v miernej/teplej klíme; v tundre a horách ostáva biom pevniny až k vode.
    bool sandyShore = landBiome == Biome::Plains || landBiome == Biome::Forest || landBiome == Biome::BirchForest ||
                      landBiome == Biome::Desert || landBiome == Biome::CherryGrove;

    const int h = c.height;
    if (isRiver)                                c.biome = Biome::River;
    else if (h < SEA_LEVEL - 1)                 c.biome = Biome::Ocean;
    else if (h <= SEA_LEVEL + 1 && sandyShore)  c.biome = Biome::Beach;
    else if (h > c.snowLine)                    c.biome = Biome::SnowyPeaks;
    else                                        c.biome = landBiome;

    c.cold = temperature < -0.3f;
    c.tint = climateTint(temperature, humidity);
    if (landBiome == Biome::CherryGrove) c.tint = mix3(c.tint, glm::vec3(0.56f, 0.84f, 0.36f), 0.6f);
    if (landBiome == Biome::Badlands) c.tint = glm::vec3(0.72f, 0.64f, 0.34f);
    return c;
}

// ------------------------------------------------------------------ 3D tvar

float TerrainGenerator::overhangRaw(int x, int y, int z) const
{
    float fx = x + m_offsetX, fz = z + m_offsetZ;
    return 0.7f * m_overhang3.noise(fx * 0.03f, y * 0.045f, fz * 0.03f) +
           0.3f * m_overhang3.noise(fx * 0.075f + 40.0f, y * 0.09f, fz * 0.075f);
}

float TerrainGenerator::islandRaw(int x, int y, int z) const
{
    float fx = x + m_offsetX, fz = z + m_offsetZ;
    return m_island3.noise(fx * 0.03f, y * 0.06f, fz * 0.03f);
}

float TerrainGenerator::overhangAt(int x, int y, int z) const
{
    return latticeSample([&](int a, int b, int c) { return overhangRaw(a, b, c); }, x, y, z, LX, LY, LZ);
}

float TerrainGenerator::islandAt(int x, int y, int z) const
{
    return latticeSample([&](int a, int b, int c) { return islandRaw(a, b, c); }, x, y, z, LX, LY, LZ);
}

bool TerrainGenerator::terrainSolid(int y, const Column& c, float ov)
{
    // Ďaleko od povrchu rozhoduje len 2D výška; v páse ±16 blokov 3D noise
    // vytvára útesy, previsy a oblúky.
    if (c.overhang < 0.02f || y <= c.height - 16) return y <= c.height;
    if (y > c.height + 16) return false;
    return (c.height - y) + c.overhang * 14.0f * ov > 0.0f;
}

bool TerrainGenerator::islandSolid(int y, const Column& c, float isl)
{
    if (c.islandShape <= 0.0f) return false;
    // Hore plochý vrch, dole dlhé visiace skaly (tým dlhšie, čím je ostrov mohutnejší).
    float t = y >= c.islandCenter ? (y - c.islandCenter) / 6.0f
                                  : (c.islandCenter - y) / (4.0f + 30.0f * c.islandShape);
    if (t > 1.6f) return false;
    return c.islandShape - t * 0.8f + 0.22f * isl > 0.18f;
}

BlockId TerrainGenerator::strata(int x, int y, int z) const
{
    // Farebné vrstvy mesy – mierne zvlnené, aby neboli úplne rovné.
    static const BlockId BANDS[] = {
        BlockId::Terracotta, BlockId::Terracotta, BlockId::TerracottaYellow, BlockId::Terracotta,
        BlockId::TerracottaWhite, BlockId::TerracottaRed, BlockId::TerracottaRed, BlockId::Terracotta,
        BlockId::TerracottaBrown, BlockId::Terracotta, BlockId::TerracottaYellow, BlockId::TerracottaYellow,
        BlockId::Terracotta, BlockId::TerracottaWhite, BlockId::Terracotta, BlockId::TerracottaRed,
        BlockId::TerracottaBrown, BlockId::TerracottaBrown, BlockId::Terracotta, BlockId::TerracottaWhite,
    };
    const int n = sizeof(BANDS) / sizeof(BANDS[0]);
    int wobble = (int)std::floor(m_strata.noise((x + m_offsetX) * 0.02f, (z + m_offsetZ) * 0.02f) * 3.0f);
    int i = ((y + wobble) % n + n) % n;
    return BANDS[i];
}

BlockId TerrainGenerator::terrainMaterial(int x, int y, int z, int depth, const Column& c, int slope) const
{
    if (c.biome == Biome::Badlands) {
        if (depth == 0 && slope <= 1) return BlockId::RedSand;
        if (depth < 50) return strata(x, y, z);
        return BlockId::Stone;
    }

    // Nad snežnou čiarou: sneh, na strmých stenách holá skala.
    if (y >= c.snowLine && depth < 2) return slope >= 4 ? BlockId::Stone : BlockId::Snow;

    BlockId top = BlockId::Grass, filler = BlockId::Dirt;
    int fillerDepth = 3;
    switch (c.biome) {
    case Biome::Ocean:
    case Biome::River:
        top = filler = y < SEA_LEVEL - 8 ? BlockId::Gravel : BlockId::Sand;
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
        if (y > 128) top = filler = BlockId::Stone;
        else if (c.cold) top = BlockId::SnowyGrass;
        break;
    case Biome::SnowyPeaks:
        top = BlockId::Snow;
        filler = BlockId::Stone;
        break;
    default:
        break;
    }

    // Na strmých svahoch sa tráva neudrží → holá skala.
    if (slope >= 3 && y > SEA_LEVEL && c.biome != Biome::Desert && c.biome != Biome::Beach)
        top = filler = BlockId::Stone;

    if (depth == 0) return top;
    if (depth < fillerDepth) return filler;
    return BlockId::Stone;
}

BlockId TerrainGenerator::islandMaterial(int x, int y, int z, int depth, const Column& c) const
{
    if (depth == 0) return c.cold ? BlockId::SnowyGrass : BlockId::Grass;
    if (depth < 3) return BlockId::Dirt;
    // Nebeské kryštály rastú len vo vnútri lietajúcich ostrovov (zhluky 2×2×2).
    if (depth > 3 && (hash3(x >> 1, y >> 1, z >> 1, m_seed ^ 0x5C1A7u) % 1000u) < 22u) return BlockId::SkyCrystalOre;
    return BlockId::Stone;
}

// ------------------------------------------------------------------ dopyty

TerrainGenerator::Surface TerrainGenerator::surfaceAt(int x, int z) const
{
    Column c = columnAt(x, z);

    // Ostrov nad stĺpcom má prednosť.
    if (c.islandShape > 0.0f) {
        int top = std::min(c.islandCenter + 12, Chunk::SIZE_Y - 1);
        for (int y = top; y > c.height + 16; --y)
            if (islandSolid(y, c, islandAt(x, y, z))) return {y, islandMaterial(x, y, z, 0, c), c.biome, true};
    }

    int slope = 0;
    const int dx[4] = {1, -1, 0, 0}, dz[4] = {0, 0, 1, -1};
    for (int i = 0; i < 4; ++i) slope = std::max(slope, std::abs(c.height - heightAt(x + dx[i], z + dz[i])));

    int top = std::min(c.height + 16, Chunk::SIZE_Y - 1);
    for (int y = top; y >= std::max(0, c.height - 16); --y) {
        float ov = (c.overhang >= 0.02f && y > c.height - 16) ? overhangAt(x, y, z) : 0.0f;
        if (terrainSolid(y, c, ov)) return {y, terrainMaterial(x, y, z, 0, c, slope), c.biome, false};
    }
    return {c.height, BlockId::Stone, c.biome, false};
}

glm::ivec2 TerrainGenerator::findSpawn() const
{
    // Špirála po 16 blokoch, max. ~2 km od stredu.
    for (int r = 0; r < 128; ++r) {
        for (int i = -r; i <= r; ++i) {
            const glm::ivec2 candidates[4] = {{i, -r}, {i, r}, {-r, i}, {r, i}};
            for (glm::ivec2 p : candidates) {
                Column c = columnAt(p.x * 16, p.y * 16);
                if (c.height > SEA_LEVEL + 3 && c.overhang < 0.05f && c.biome != Biome::SnowyPeaks)
                    return p * 16;
            }
        }
    }
    return {0, 0};
}

// ------------------------------------------------------------------ generovanie chunku

void TerrainGenerator::fillTints(Chunk& chunk) const
{
    const int baseX = chunk.coord().x * Chunk::SIZE_X, baseZ = chunk.coord().y * Chunk::SIZE_Z;
    for (int z = 0; z < Chunk::SIZE_Z; ++z)
        for (int x = 0; x < Chunk::SIZE_X; ++x) chunk.setTint(x, z, columnAt(baseX + x, baseZ + z).tint);
}

void TerrainGenerator::generate(Chunk& chunk) const
{
    const int baseX = chunk.coord().x * Chunk::SIZE_X;
    const int baseZ = chunk.coord().y * Chunk::SIZE_Z;

    // Stĺpce s okrajom 1 blok – kvôli sklonu terénu aj na hranách chunku.
    constexpr int W = Chunk::SIZE_X + 2, D = Chunk::SIZE_Z + 2;
    std::vector<Column> cols(W * D);
    for (int z = 0; z < D; ++z)
        for (int x = 0; x < W; ++x)
            cols[z * W + x] = columnAt(baseX + x - 1, baseZ + z - 1);
    auto col = [&](int x, int z) -> const Column& { return cols[(z + 1) * W + (x + 1)]; };

    bool anyOverhang = false, anyIsland = false;
    int maxHeight = 0;
    for (int z = 0; z < Chunk::SIZE_Z; ++z) {
        for (int x = 0; x < Chunk::SIZE_X; ++x) {
            const Column& c = col(x, z);
            anyOverhang |= c.overhang >= 0.02f;
            anyIsland |= c.islandShape > 0.0f;
            maxHeight = std::max(maxHeight, c.height);
        }
    }

    // 3D noise len tam, kde treba.
    Lattice overhang, island, caveA, caveB, caveC;
    if (anyOverhang)
        overhang.fill([&](int a, int b, int c) { return overhangRaw(a, b, c); }, baseX, baseZ, Chunk::SIZE_Y, LX, LY, LZ);
    if (anyIsland)
        island.fill([&](int a, int b, int c) { return islandRaw(a, b, c); }, baseX, baseZ, Chunk::SIZE_Y, LX, LY, LZ);

    const float ox = m_offsetX, oz = m_offsetZ;
    const int caveHeight = std::min(Chunk::SIZE_Y, ((maxHeight + 20) / CAVE_LY + 1) * CAVE_LY);
    caveA.fill([&](int a, int b, int c) { return m_caveA.noise((a + ox) * 0.016f, b * 0.028f, (c + oz) * 0.016f); },
               baseX, baseZ, caveHeight, LX, CAVE_LY, LZ);
    caveB.fill([&](int a, int b, int c) { return m_caveB.noise((a + ox) * 0.016f, b * 0.028f, (c + oz) * 0.016f); },
               baseX, baseZ, caveHeight, LX, CAVE_LY, LZ);
    caveC.fill([&](int a, int b, int c) { return m_caveC.noise((a + ox) * 0.011f, b * 0.02f, (c + oz) * 0.011f); },
               baseX, baseZ, caveHeight, LX, CAVE_LY, LZ);

    for (int z = 0; z < Chunk::SIZE_Z; ++z) {
        for (int x = 0; x < Chunk::SIZE_X; ++x) {
            const Column& c = col(x, z);
            const int wx = baseX + x, wz = baseZ + z;
            const int h = c.height;
            chunk.setTint(x, z, c.tint);

            int slope = std::max({std::abs(h - col(x - 1, z).height), std::abs(h - col(x + 1, z).height),
                                  std::abs(h - col(x, z - 1).height), std::abs(h - col(x, z + 1).height)});

            // --- Tvar a materiály, zhora nadol. depth = koľko blokov sme pod najbližším vzduchom.
            // Aspoň po hladinu mora – inak by v hlbokom oceáne (dno < 46) chýbala horná vrstva vody.
            int top = std::max(h + 16, SEA_LEVEL);
            if (c.islandShape > 0.0f) top = std::max(top, c.islandCenter + 12);
            top = std::min(top, Chunk::SIZE_Y - 1);

            int depth = -1;
            int mainSurface = -1; // najvyšší pevný blok hlavného terénu (nie ostrova)
            for (int y = top; y >= 0; --y) {
                bool solid;
                bool onIsland = false;
                if (y > h + 16) {
                    onIsland = c.islandShape > 0.0f && islandSolid(y, c, island.sample(wx, y, wz));
                    solid = onIsland;
                } else {
                    float ov = (c.overhang >= 0.02f && y > h - 16) ? overhang.sample(wx, y, wz) : 0.0f;
                    solid = terrainSolid(y, c, ov);
                }

                if (!solid) {
                    depth = -1;
                    if (y <= SEA_LEVEL && y > h - 20) chunk.set(x, y, z, BlockId::Water);
                    continue;
                }
                depth = depth < 0 ? 0 : depth + 1;
                if (onIsland) {
                    chunk.set(x, y, z, islandMaterial(wx, y, wz, depth, c));
                } else {
                    if (mainSurface < 0) mainSurface = y;
                    chunk.set(x, y, z, terrainMaterial(wx, y, wz, depth, c, slope));
                }
            }

            // --- Jaskyne: kľukaté tunely (dva noise blízko nuly naraz) + veľké dutiny.
            // Pod morom a riekami nekopeme blízko dna, aby sa voda "nevyliala".
            int caveLimit = h < SEA_LEVEL + 3 ? mainSurface - 6 : mainSurface;
            caveLimit = std::min(caveLimit, caveHeight - 1);
            for (int y = 1; y <= caveLimit; ++y) {
                BlockId b = chunk.get(x, y, z);
                if (b == BlockId::Air || isLiquid(b)) continue;
                float a = caveA.sample(wx, y, wz), bb = caveB.sample(wx, y, wz);
                bool tunnel = a * a + bb * bb < 0.010f;
                bool cavern = y < h - 14 && caveC.sample(wx, y, wz) > 0.58f;
                if (tunnel || cavern) chunk.set(x, y, z, y <= 10 ? BlockId::Lava : BlockId::Air);
            }

            // --- Bedrock: dno sveta, v spodných vrstvách nepravidelný.
            chunk.set(x, 0, z, BlockId::Bedrock);
            for (int y = 1; y <= 3; ++y)
                if ((hash3(wx, y, wz, m_seed) & 3) < (uint32_t)(4 - y)) chunk.set(x, y, z, BlockId::Bedrock);
        }
    }

    placeOres(chunk, m_seed);
    placeTrees(chunk, *this);
    placeStructures(chunk, *this);
}

} // namespace cubiq
