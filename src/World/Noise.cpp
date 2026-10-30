#include "World/Noise.h"

#include <cmath>
#include <numeric>
#include <random>

namespace cubiq {

namespace {
    const float GRAD[8][2] = {
        {1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1},
    };

    const float F2 = 0.5f * (std::sqrt(3.0f) - 1.0f);
    const float G2 = (3.0f - std::sqrt(3.0f)) / 6.0f;

    int fastFloor(float x)
    {
        int i = (int)x;
        return x < i ? i - 1 : i;
    }
}

SimplexNoise::SimplexNoise(uint32_t seed)
{
    std::array<uint8_t, 256> p;
    std::iota(p.begin(), p.end(), 0);
    std::shuffle(p.begin(), p.end(), std::mt19937(seed));
    for (int i = 0; i < 512; ++i) m_perm[i] = p[i & 255];
}

float SimplexNoise::noise(float x, float y) const
{
    // Posun do simplexovej mriežky (rovnostranné trojuholníky).
    float s = (x + y) * F2;
    int i = fastFloor(x + s);
    int j = fastFloor(y + s);
    float t = (i + j) * G2;
    float x0 = x - (i - t);
    float y0 = y - (j - t);

    int i1 = x0 > y0 ? 1 : 0;
    int j1 = x0 > y0 ? 0 : 1;

    float x1 = x0 - i1 + G2, y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2, y2 = y0 - 1.0f + 2.0f * G2;

    int ii = i & 255, jj = j & 255;

    auto corner = [&](float cx, float cy, int gi) {
        float tt = 0.5f - cx * cx - cy * cy;
        if (tt < 0.0f) return 0.0f;
        tt *= tt;
        const float* g = GRAD[gi & 7];
        return tt * tt * (g[0] * cx + g[1] * cy);
    };

    float n0 = corner(x0, y0, m_perm[ii + m_perm[jj]]);
    float n1 = corner(x1, y1, m_perm[ii + i1 + m_perm[jj + j1]]);
    float n2 = corner(x2, y2, m_perm[ii + 1 + m_perm[jj + 1]]);

    return 70.0f * (n0 + n1 + n2);
}

float SimplexNoise::noise(float x, float y, float z) const
{
    static const float GRAD3[12][3] = {
        {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1}, {-1, 0, 1},
        {1, 0, -1}, {-1, 0, -1}, {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1},
    };
    const float F3 = 1.0f / 3.0f, G3 = 1.0f / 6.0f;

    float s = (x + y + z) * F3;
    int i = fastFloor(x + s), j = fastFloor(y + s), k = fastFloor(z + s);
    float t = (i + j + k) * G3;
    float x0 = x - (i - t), y0 = y - (j - t), z0 = z - (k - t);

    // V ktorom zo 6 štvorstenov kocky sme.
    int i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0)      { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
        else if (x0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; }
        else               { i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; }
    } else {
        if (y0 < z0)       { i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; }
        else if (x0 < z0)  { i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; }
        else               { i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
    }

    float x1 = x0 - i1 + G3, y1 = y0 - j1 + G3, z1 = z0 - k1 + G3;
    float x2 = x0 - i2 + 2 * G3, y2 = y0 - j2 + 2 * G3, z2 = z0 - k2 + 2 * G3;
    float x3 = x0 - 1 + 3 * G3, y3 = y0 - 1 + 3 * G3, z3 = z0 - 1 + 3 * G3;

    int ii = i & 255, jj = j & 255, kk = k & 255;
    auto corner = [&](float cx, float cy, float cz, int gi) {
        float tt = 0.6f - cx * cx - cy * cy - cz * cz;
        if (tt < 0.0f) return 0.0f;
        tt *= tt;
        const float* g = GRAD3[gi % 12];
        return tt * tt * (g[0] * cx + g[1] * cy + g[2] * cz);
    };

    float n0 = corner(x0, y0, z0, m_perm[ii + m_perm[jj + m_perm[kk]]]);
    float n1 = corner(x1, y1, z1, m_perm[ii + i1 + m_perm[jj + j1 + m_perm[kk + k1]]]);
    float n2 = corner(x2, y2, z2, m_perm[ii + i2 + m_perm[jj + j2 + m_perm[kk + k2]]]);
    float n3 = corner(x3, y3, z3, m_perm[ii + 1 + m_perm[jj + 1 + m_perm[kk + 1]]]);
    return 32.0f * (n0 + n1 + n2 + n3);
}

float SimplexNoise::fbm(float x, float y, int octaves, float lacunarity, float gain) const
{
    float sum = 0.0f, amp = 1.0f, freq = 1.0f, norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        // Posun medzi oktávami, aby sa nuly neprekrývali v (0,0).
        sum += amp * noise(x * freq + o * 17.3f, y * freq - o * 31.7f);
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return sum / norm;
}

float SimplexNoise::ridged(float x, float y, int octaves, float lacunarity, float gain) const
{
    float sum = 0.0f, amp = 1.0f, freq = 1.0f, norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        float n = 1.0f - std::fabs(noise(x * freq + o * 17.3f, y * freq - o * 31.7f));
        sum += amp * n * n;
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return sum / norm;
}

} // namespace cubiq
