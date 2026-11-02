#include "Game/Sky.h"

#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace cubiq {

namespace {
    float smoothstep(float e0, float e1, float x)
    {
        float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    glm::vec3 mix3(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * t; }

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
    createNoiseTexture();
}

SkyRenderer::~SkyRenderer()
{
    glDeleteVertexArrays(1, &m_emptyVao);
    glDeleteTextures(1, &m_noiseTexture);
}

void SkyRenderer::createNoiseTexture()
{
    // Opakujúci sa 3D value noise 64³ pre mraky (vzorkovanie textúry je oveľa rýchlejšie ako
    // počítať šum v shaderi). R = tvar (nižšie frekvencie), G = detail (vyššie frekvencie).
    constexpr int S = 64;
    auto hash = [](int x, int y, int z, int seed) {
        uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)(z * 83492791) ^ (uint32_t)(seed * 2654435761u);
        h = (h ^ (h >> 13)) * 1274126177u;
        return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
    };
    // Value noise s periódou P buniek (bunka = S / P voxelov) → bezšvové opakovanie.
    auto noise = [&](float x, float y, float z, int P, int seed) {
        float cell = (float)S / P;
        float fx = x / cell, fy = y / cell, fz = z / cell;
        int ix = (int)std::floor(fx), iy = (int)std::floor(fy), iz = (int)std::floor(fz);
        float tx = fx - ix, ty = fy - iy, tz = fz - iz;
        tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty); tz = tz * tz * (3 - 2 * tz);
        auto v = [&](int dx, int dy, int dz) {
            return hash(((ix + dx) % P + P) % P, ((iy + dy) % P + P) % P, ((iz + dz) % P + P) % P, seed);
        };
        auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
        float x00 = lerp(v(0, 0, 0), v(1, 0, 0), tx), x10 = lerp(v(0, 1, 0), v(1, 1, 0), tx);
        float x01 = lerp(v(0, 0, 1), v(1, 0, 1), tx), x11 = lerp(v(0, 1, 1), v(1, 1, 1), tx);
        return lerp(lerp(x00, x10, ty), lerp(x01, x11, ty), tz);
    };

    std::vector<uint8_t> data((size_t)S * S * S * 2);
    for (int z = 0; z < S; ++z)
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x) {
                float shape = noise(x, y, z, 4, 1) * 0.55f + noise(x, y, z, 8, 2) * 0.3f + noise(x, y, z, 16, 3) * 0.15f;
                float detail = noise(x, y, z, 16, 4) * 0.6f + noise(x, y, z, 32, 5) * 0.4f;
                size_t i = (((size_t)z * S + y) * S + x) * 2;
                data[i + 0] = (uint8_t)std::clamp((int)(shape * 255.0f), 0, 255);
                data[i + 1] = (uint8_t)std::clamp((int)(detail * 255.0f), 0, 255);
            }

    glGenTextures(1, &m_noiseTexture);
    glBindTexture(GL_TEXTURE_3D, m_noiseTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RG8, S, S, S, 0, GL_RG, GL_UNSIGNED_BYTE, data.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glBindTexture(GL_TEXTURE_3D, 0);
}

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

void SkyRenderer::drawClouds(const SkyState& sky, const glm::mat4& view, const glm::mat4& projection,
                             const glm::vec3& cameraPos, float maxDistance, float time)
{
    m_cloudShader.bind();
    m_cloudShader.setMat4("uInvViewProj", glm::inverse(projection * view));
    m_cloudShader.setVec3("uCameraPos", cameraPos);
    m_cloudShader.setInt("uNoise", 1);
    m_cloudShader.setFloat("uTime", time);
    m_cloudShader.setFloat("uBottom", CLOUD_BOTTOM);
    m_cloudShader.setFloat("uTop", CLOUD_TOP);
    m_cloudShader.setFloat("uMaxDistance", maxDistance);
    m_cloudShader.setVec3("uSunDir", sky.sunDir);
    m_cloudShader.setVec3("uSunColor", sky.sunColor);
    m_cloudShader.setFloat("uSunBrightness", sky.sunBrightness);
    m_cloudShader.setVec3("uZenith", sky.zenith);
    m_cloudShader.setVec3("uHorizon", sky.horizon);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, m_noiseTexture);
    glActiveTexture(GL_TEXTURE0);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // farba mrakov je už prenásobená alfou
    glBindVertexArray(m_emptyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

} // namespace cubiq
