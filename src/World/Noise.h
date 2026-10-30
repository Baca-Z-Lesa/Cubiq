#pragma once
#include <array>
#include <cstdint>

namespace cubiq {

// 2D simplex noise (podľa Stefana Gustavsona) so seedom.
// Po vytvorení je iba na čítanie, takže sa dá bezpečne volať z viacerých vlákien.
class SimplexNoise {
public:
    explicit SimplexNoise(uint32_t seed);

    float noise(float x, float y) const; // približne -1..1

    // Fractal Brownian motion: súčet oktáv, výsledok normalizovaný do -1..1.
    float fbm(float x, float y, int octaves, float lacunarity = 2.0f, float gain = 0.5f) const;

    // Ostré hrebene (hory): 0..1, hrebeň = 1.
    float ridged(float x, float y, int octaves, float lacunarity = 2.0f, float gain = 0.5f) const;

private:
    std::array<uint8_t, 512> m_perm;
};

} // namespace cubiq
