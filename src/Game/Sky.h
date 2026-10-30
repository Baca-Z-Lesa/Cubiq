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

// Obloha cez celú obrazovku (gradient, slnko, mesiac, hviezdy) a vrstva mrakov.
class SkyRenderer {
public:
    SkyRenderer();
    ~SkyRenderer();

    SkyRenderer(const SkyRenderer&) = delete;
    SkyRenderer& operator=(const SkyRenderer&) = delete;

    void drawSky(const SkyState& sky, const glm::mat4& view, const glm::mat4& projection, float time);
    void drawClouds(const SkyState& sky, const glm::mat4& viewProjection, const glm::vec3& cameraPos,
                    float viewDistance, float time);

private:
    Shader m_skyShader;
    Shader m_cloudShader;
    unsigned int m_emptyVao = 0;
};

} // namespace cubiq
