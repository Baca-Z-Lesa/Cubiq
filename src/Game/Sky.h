#pragma once
#include "Engine/Shader.h"

#include <glm/glm.hpp>

namespace cubiq {

// Stav oblohy pre daný čas dňa (0 = východ slnka, 0.25 = poludnie, 0.5 = západ, 0.75 = polnoc).
struct SkyState {
    glm::vec3 sunDir;       // smer k slnku (mesiac je opačne)
    glm::vec3 sunColor;     // farba slnečného svetla (pri západe oranžová)
    float sunBrightness;    // sila svetla oblohy pre terén 0.16 (mesiac) .. 1 (deň)
    glm::vec3 zenith;       // farba oblohy hore
    glm::vec3 horizon;      // farba oblohy pri obzore = farba hmly
    float stars;            // viditeľnosť hviezd 0..1
};

SkyState computeSky(float timeOfDay);

// Čas dňa → "hh:mm" (východ slnka = 06:00).
void formatClock(float timeOfDay, char* out, int size);

// Obloha cez celú obrazovku (gradient, slnko, mesiac, hviezdy) a volumetrické mraky.
class SkyRenderer {
public:
    static constexpr float CLOUD_BOTTOM = 190.0f;
    static constexpr float CLOUD_TOP = 250.0f;

    SkyRenderer();
    ~SkyRenderer();

    SkyRenderer(const SkyRenderer&) = delete;
    SkyRenderer& operator=(const SkyRenderer&) = delete;

    void drawSky(const SkyState& sky, const glm::mat4& view, const glm::mat4& projection, float time);
    // Raymarching cez vrstvu mrakov pre každý pixel. Pod mrakmi sa kreslí pred terénom
    // (terén ich prekryje), nad nimi alebo v nich až po teréne (mraky zakryjú zem pod sebou).
    void drawClouds(const SkyState& sky, const glm::mat4& view, const glm::mat4& projection,
                    const glm::vec3& cameraPos, float maxDistance, float time);

private:
    void createNoiseTexture();

    Shader m_skyShader;
    Shader m_cloudShader;
    unsigned int m_emptyVao = 0;
    unsigned int m_noiseTexture = 0;
};

} // namespace cubiq
