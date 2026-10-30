#include "Game/Sky.h"

#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cubiq {

namespace {
    float smoothstep(float e0, float e1, float x)
    {
        float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    glm::vec3 mix3(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * t; }

    constexpr float CLOUD_HEIGHT = 196.0f;
}

SkyState computeSky(float t)
{
    const float angle = t * 6.2831853f;
    SkyState s;
    // Slnko obieha z východu (+X) cez juh nahor na západ (-X), mierne naklonené.
    s.sunDir = glm::normalize(glm::vec3(std::cos(angle), std::sin(angle), 0.25f));

    const float elev = s.sunDir.y;
    const float day = smoothstep(-0.15f, 0.2f, elev);           // 0 noc .. 1 deň
    const float sunset = 1.0f - smoothstep(0.0f, 0.35f, std::fabs(elev)); // najsilnejšie pri obzore

    const glm::vec3 DAY_ZENITH(0.30f, 0.56f, 0.95f), DAY_HORIZON(0.68f, 0.84f, 1.0f);
    const glm::vec3 NIGHT_ZENITH(0.008f, 0.012f, 0.035f), NIGHT_HORIZON(0.03f, 0.045f, 0.09f);
    const glm::vec3 SUNSET(1.0f, 0.52f, 0.28f);

    s.zenith = mix3(NIGHT_ZENITH, DAY_ZENITH, day);
    s.horizon = mix3(NIGHT_HORIZON, DAY_HORIZON, day);
    s.horizon = mix3(s.horizon, SUNSET, sunset * 0.75f * smoothstep(-0.25f, 0.05f, elev));

    // Cez deň slnko (pri západe oranžové), v noci studený modrastý mesačný svit.
    const glm::vec3 SUN_LOW(1.0f, 0.6f, 0.38f), SUN_HIGH(1.0f, 0.98f, 0.94f), MOON(0.55f, 0.66f, 1.0f);
    glm::vec3 sunLight = mix3(SUN_LOW, SUN_HIGH, smoothstep(0.0f, 0.4f, elev));
    s.sunColor = mix3(MOON, sunLight, day);
    s.sunBrightness = 0.16f + 0.84f * day;
    s.stars = 1.0f - smoothstep(-0.2f, 0.05f, elev);
    return s;
}

void formatClock(float t, char* out, int size)
{
    float hours = std::fmod(6.0f + t * 24.0f, 24.0f);
    int h = (int)hours, m = (int)((hours - h) * 60.0f);
    std::snprintf(out, size, "%02d:%02d", h, m);
}

SkyRenderer::SkyRenderer()
    : m_skyShader("assets/shaders/sky.vert", "assets/shaders/sky.frag"),
      m_cloudShader("assets/shaders/clouds.vert", "assets/shaders/clouds.frag")
{
    // Core profil vyžaduje naviazaný VAO aj keď vrcholy počítame v shaderi z gl_VertexID.
    glGenVertexArrays(1, &m_emptyVao);
}

SkyRenderer::~SkyRenderer() { glDeleteVertexArrays(1, &m_emptyVao); }

void SkyRenderer::drawSky(const SkyState& sky, const glm::mat4& view, const glm::mat4& projection, float time)
{
    // Len rotácia kamery (bez posunu) – obloha je "nekonečne ďaleko".
    glm::mat4 rotOnly = glm::mat4(glm::mat3(view));
    m_skyShader.bind();
    m_skyShader.setMat4("uInvViewProj", glm::inverse(projection * rotOnly));
    m_skyShader.setVec3("uSunDir", sky.sunDir);
    m_skyShader.setVec3("uSunColor", sky.sunColor);
    m_skyShader.setVec3("uZenith", sky.zenith);
    m_skyShader.setVec3("uHorizon", sky.horizon);
    m_skyShader.setFloat("uStars", sky.stars);
    m_skyShader.setFloat("uTime", time);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBindVertexArray(m_emptyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3); // jeden trojuholník cez celú obrazovku
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void SkyRenderer::drawClouds(const SkyState& sky, const glm::mat4& viewProjection, const glm::vec3& cameraPos,
                             float viewDistance, float time)
{
    m_cloudShader.bind();
    m_cloudShader.setMat4("uViewProj", viewProjection);
    m_cloudShader.setVec3("uCameraPos", cameraPos);
    m_cloudShader.setFloat("uHeight", CLOUD_HEIGHT);
    m_cloudShader.setFloat("uExtent", viewDistance * 1.6f);
    m_cloudShader.setFloat("uTime", time);
    m_cloudShader.setVec3("uSunColor", sky.sunColor);
    m_cloudShader.setFloat("uSunBrightness", sky.sunBrightness);
    m_cloudShader.setVec3("uHorizon", sky.horizon);

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(m_emptyVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} // namespace cubiq
