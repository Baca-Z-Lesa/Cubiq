#include "Game/DroppedItems.h"
#include "Engine/Texture.h"
#include "Game/BlockAtlas.h"
#include "Game/Sky.h"
#include "World/World.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace cubiq {

namespace {
    constexpr float GRAVITY = 22.0f;
    constexpr float HALF = 0.125f; // polovičná veľkosť kolízneho boxu

    bool solidAt(const World& world, const glm::vec3& p)
    {
        return isSolid(world.getBlock((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z)));
    }
}

float DroppedItems::random01()
{
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return (m_rng & 0xFFFFFF) / (float)0x1000000;
}

void DroppedItems::spawn(const ItemStack& stack, const glm::vec3& position, const glm::vec3& velocity)
{
    if (stack.empty()) return;
    if ((int)m_items.size() >= MAX_ITEMS) m_items.erase(m_items.begin()); // najstarší ustúpi
    DroppedItem d;
    d.stack = stack;
    d.position = position;
    d.velocity = velocity + glm::vec3((random01() - 0.5f) * 2.5f, 3.0f + random01() * 1.5f, (random01() - 0.5f) * 2.5f);
    d.spin = random01() * 6.283f;
    if (glm::length(velocity) > 0.0f) d.pickupDelay = 1.5f; // vyhodený hráčom – nech stihne odletieť
    m_items.push_back(d);
}

bool DroppedItems::update(const World& world, Inventory& inventory, const glm::vec3& playerCenter, float dt)
{
    bool picked = false;
    for (size_t i = 0; i < m_items.size(); ++i) {
        DroppedItem& d = m_items[i];
        d.age += dt;
        d.pickupDelay -= dt;
        d.spin += dt * 1.6f;

        const glm::vec3 toPlayer = playerCenter - d.position;
        const float dist = glm::length(toPlayer);
        if (d.pickupDelay <= 0.0f && dist < 3.0f) {
            // Priťahovanie k hráčovi – ak sa ešte zmestí do inventára.
            if (dist < 0.9f) {
                int left = inventory.add(d.stack);
                if (left < d.stack.count) picked = true;
                d.stack.count = left;
                if (left == 0) continue; // vymaže sa nižšie
            } else {
                d.velocity = glm::mix(d.velocity, toPlayer / dist * 9.0f, std::min(1.0f, dt * 8.0f));
            }
        }

        // Fyzika po osiach: padá, kĺže, zastaví sa o bloky. V bloku sa vytlačí nahor.
        if (solidAt(world, d.position)) {
            d.position.y = std::floor(d.position.y) + 1.0f + HALF;
            d.velocity = glm::vec3(0.0f);
        }
        const bool inLiquid = isLiquid(world.getBlock((int)std::floor(d.position.x), (int)std::floor(d.position.y),
                                                      (int)std::floor(d.position.z)));
        d.velocity.y -= (inLiquid ? GRAVITY * 0.15f : GRAVITY) * dt;
        if (inLiquid) d.velocity.y = std::max(d.velocity.y, -1.0f);

        for (int axis = 0; axis < 3; ++axis) {
            glm::vec3 next = d.position;
            next[axis] += d.velocity[axis] * dt;
            glm::vec3 probe = next;
            probe[axis] += (d.velocity[axis] > 0 ? HALF : -HALF);
            if (solidAt(world, probe)) {
                if (axis == 1 && d.velocity.y < 0.0f) d.velocity.x *= 0.5f, d.velocity.z *= 0.5f; // trenie pri dopade
                d.velocity[axis] = 0.0f;
            } else {
                d.position = next;
            }
        }
        bool onGround = solidAt(world, d.position - glm::vec3(0.0f, HALF + 0.02f, 0.0f));
        if (onGround) {
            float damp = std::max(0.0f, 1.0f - dt * 10.0f);
            d.velocity.x *= damp;
            d.velocity.z *= damp;
        }
    }

    // Spojenie rovnakých kôp blízko seba (menej predmetov na zemi pri ťažbe).
    for (size_t i = 0; i < m_items.size(); ++i) {
        DroppedItem& a = m_items[i];
        if (a.stack.empty() || a.stack.maxStack() <= 1) continue;
        for (size_t j = i + 1; j < m_items.size(); ++j) {
            DroppedItem& b = m_items[j];
            if (b.stack.id != a.stack.id || b.stack.empty()) continue;
            if (glm::length(a.position - b.position) > 0.8f) continue;
            int n = std::min(b.stack.count, a.stack.maxStack() - a.stack.count);
            a.stack.count += n;
            b.stack.count -= n;
        }
    }

    m_items.erase(std::remove_if(m_items.begin(), m_items.end(),
                                 [](const DroppedItem& d) { return d.stack.empty() || d.age > LIFETIME || d.position.y < -32.0f; }),
                  m_items.end());
    return picked;
}

// ------------------------------------------------------------------ kreslenie

DroppedItemRenderer::DroppedItemRenderer() : m_shader("assets/shaders/item.vert", "assets/shaders/item.frag")
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // pozícia (3), uv (2), tieň (1)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

DroppedItemRenderer::~DroppedItemRenderer()
{
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
}

void DroppedItemRenderer::draw(const DroppedItems& items, const World& world, const glm::mat4& viewProjection,
                               const glm::vec3& cameraPos, const SkyState& sky, const glm::vec3& fogColor, float fogStart,
                               float fogEnd, const Texture& atlas)
{
    if (items.items().empty()) return;
    m_vertices.clear();

    auto vertex = [&](const glm::vec3& p, float u, float v, float shade) {
        m_vertices.insert(m_vertices.end(), {p.x, p.y, p.z, u, v, shade});
    };
    // Štvoruholník a, b, c, d (proti smeru hodín) s dlaždicou atlasu.
    auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, int tile, float shade) {
        float u0 = (float)tile / ATLAS_TILES, u1 = (float)(tile + 1) / ATLAS_TILES;
        vertex(a, u0, 1, shade); vertex(b, u1, 1, shade); vertex(c, u1, 0, shade);
        vertex(a, u0, 1, shade); vertex(c, u1, 0, shade); vertex(d, u0, 0, shade);
    };

    for (const DroppedItem& d : items.items()) {
        if (glm::length(d.position - cameraPos) > 48.0f) continue;
        // Svetlo: pod strechou / v jaskyni tmavšie.
        int bx = (int)std::floor(d.position.x), by = (int)std::floor(d.position.y), bz = (int)std::floor(d.position.z);
        bool covered = false;
        for (int y = by + 1; y < by + 24 && !covered; ++y) covered = isOpaque(world.getBlock(bx, y, bz));
        const float light = std::max(sky.sunBrightness * (covered ? 0.35f : 1.0f), 0.12f);

        const float bob = std::sin(d.age * 2.5f) * 0.05f + 0.06f;
        const glm::vec3 c = d.position + glm::vec3(0.0f, bob, 0.0f);
        const float cs = std::cos(d.spin), sn = std::sin(d.spin);
        const glm::vec3 ax(cs, 0.0f, sn), az(-sn, 0.0f, cs), ay(0.0f, 1.0f, 0.0f);
        const int copies = d.stack.count > 16 ? 3 : d.stack.count > 1 ? 2 : 1;

        for (int k = 0; k < copies; ++k) {
            const glm::vec3 o = c + glm::vec3(k * 0.07f, k * 0.06f, k * -0.05f);
            if (isBlockItem(d.stack.id)) {
                const BlockType& t = blockType(blockOf(d.stack.id));
                const float r = 0.125f;
                auto P = [&](float x, float y, float z) { return o + (ax * x + ay * y + az * z) * r; };
                quad(P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), P(-1, 1, -1), t.tileTop, light);
                quad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), t.tileBottom, light * 0.5f);
                quad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), t.tileSide, light * 0.8f);
                quad(P(1, -1, -1), P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), t.tileSide, light * 0.8f);
                quad(P(1, -1, 1), P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), t.tileSide, light * 0.62f);
                quad(P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), t.tileSide, light * 0.62f);
            } else {
                // Plochá ikona, otáča sa okolo zvislej osi (obe strany).
                const float r = 0.2f;
                const glm::vec3 base = o + glm::vec3(0.0f, r - 0.12f, 0.0f);
                quad(base - ax * r - ay * r, base + ax * r - ay * r, base + ax * r + ay * r, base - ax * r + ay * r,
                     itemIconTile(d.stack.id), light);
            }
        }
    }
    if (m_vertices.empty()) return;

    m_shader.bind();
    m_shader.setMat4("uViewProj", viewProjection);
    m_shader.setInt("uAtlas", 0);
    m_shader.setVec3("uSunColor", sky.sunColor);
    m_shader.setVec3("uCameraPos", cameraPos);
    m_shader.setVec3("uFogColor", fogColor);
    m_shader.setFloat("uFogStart", fogStart);
    m_shader.setFloat("uFogEnd", fogEnd);
    atlas.bind(0);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(float), m_vertices.data(), GL_STREAM_DRAW);
    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(m_vertices.size() / 6));
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}

} // namespace cubiq
