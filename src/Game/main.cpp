#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h> // musí byť pred GLFW, inak koliduje makro APIENTRY
#endif

#include "Engine/Camera.h"
#include "Engine/Input.h"
#include "Engine/Shader.h"
#include "Engine/Texture.h"
#include "Engine/UIRenderer.h"
#include "Engine/Window.h"
#include "Game/BlockAtlas.h"
#include "Game/BlockHighlight.h"
#include "Game/Hud.h"
#include "Game/Inventory.h"
#include "World/ChunkManager.h"
#include "World/Raycast.h"
#include "World/TerrainGenerator.h"
#include "World/World.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <random>
#include <string>

using namespace cubiq;

static void updateCamera(Camera& camera, float dt)
{
    const float sensitivity = 0.1f;
    glm::vec2 md = Input::mouseDelta();
    camera.rotate(md.x * sensitivity, -md.y * sensitivity);

    // Pohyb ako v creative móde: horizontálne podľa smeru pohľadu, hore/dole zvlášť.
    glm::vec3 flatForward = glm::normalize(glm::vec3(camera.forward().x, 0.0f, camera.forward().z));
    glm::vec3 right = camera.right();
    glm::vec3 move{0.0f};

    if (Input::isKeyDown(GLFW_KEY_W)) move += flatForward;
    if (Input::isKeyDown(GLFW_KEY_S)) move -= flatForward;
    if (Input::isKeyDown(GLFW_KEY_D)) move += right;
    if (Input::isKeyDown(GLFW_KEY_A)) move -= right;
    if (Input::isKeyDown(GLFW_KEY_SPACE)) move.y += 1.0f;
    if (Input::isKeyDown(GLFW_KEY_LEFT_SHIFT)) move.y -= 1.0f;

    if (glm::length(move) > 0.0f) {
        float speed = Input::isKeyDown(GLFW_KEY_LEFT_CONTROL) ? 20.0f : 6.0f;
        camera.position += glm::normalize(move) * speed * dt;
    }
}

// Svet = bloky + generátor + streaming chunkov. Pri novom seede sa celý vymení.
struct GameWorld {
    TerrainGenerator generator;
    World world;
    ChunkManager chunks;

    GameWorld(uint32_t seed, int renderDistance)
        : generator(seed), chunks(world, generator, renderDistance, ATLAS_TILES)
    {
    }
};

static void spawnCamera(Camera& camera, const TerrainGenerator& generator)
{
    glm::ivec2 spawn = generator.findSpawn();
    int ground = std::max(generator.heightAt(spawn.x, spawn.y), TerrainGenerator::SEA_LEVEL);
    camera.position = glm::vec3(spawn.x + 0.5f, ground + 20.0f, spawn.y + 0.5f);
    camera.pitch = -25.0f;
}

static uint32_t randomSeed() { return std::random_device{}(); }

// Kopanie a stavanie. Držaním tlačidla sa opakuje každých REPEAT sekúnd.
struct BlockInteraction {
    static constexpr float REACH = 6.0f;
    static constexpr float REPEAT = 0.22f;
    float breakCooldown = 0.0f;
    float placeCooldown = 0.0f;

    void update(World& world, const Camera& camera, Inventory& inventory, const RaycastHit& hit, float dt)
    {
        breakCooldown -= dt;
        placeCooldown -= dt;

        bool breakNow = Input::wasMousePressed(GLFW_MOUSE_BUTTON_LEFT) ||
                        (Input::isMouseDown(GLFW_MOUSE_BUTTON_LEFT) && breakCooldown <= 0.0f);
        bool placeNow = Input::wasMousePressed(GLFW_MOUSE_BUTTON_RIGHT) ||
                        (Input::isMouseDown(GLFW_MOUSE_BUTTON_RIGHT) && placeCooldown <= 0.0f);

        if (breakNow && hit.hit) {
            world.setBlock(hit.block.x, hit.block.y, hit.block.z, BlockId::Air);
            inventory.add(hit.id, 1);
            breakCooldown = REPEAT;
        }

        if (placeNow && hit.hit) {
            ItemStack& stack = inventory.selectedStack();
            glm::ivec3 p = hit.block + hit.normal;
            glm::ivec3 eye((int)std::floor(camera.position.x), (int)std::floor(camera.position.y),
                           (int)std::floor(camera.position.z));
            BlockId current = world.getBlock(p.x, p.y, p.z);
            bool free = current == BlockId::Air || isLiquid(current);
            if (!stack.empty() && free && p != eye && p.y >= 0 && p.y < Chunk::SIZE_Y) {
                world.setBlock(p.x, p.y, p.z, stack.id);
                if (--stack.count == 0) stack.clear();
                placeCooldown = REPEAT;
            }
        }
    }
};

static void giveStarterItems(Inventory& inventory)
{
    const BlockId starter[] = {BlockId::Grass, BlockId::Dirt, BlockId::Stone, BlockId::Sand,
                               BlockId::Gravel, BlockId::Snow, BlockId::SnowyGrass};
    for (BlockId id : starter) inventory.add(id, Inventory::MAX_STACK);
}

static int run(uint32_t seed)
{
    Window window(1280, 720, "Cubiq");
    window.setCursorLocked(true);

    Shader blockShader("assets/shaders/block.vert", "assets/shaders/block.frag");
    Texture atlas(TILE_SIZE * ATLAS_TILES, TILE_SIZE, generateBlockAtlas());

    auto game = std::make_unique<GameWorld>(seed, 12); // render distance v chunkoch

    Camera camera;
    spawnCamera(camera, game->generator);

    UIRenderer ui;
    Hud hud;
    BlockHighlight highlight;
    Inventory inventory;
    giveStarterItems(inventory);
    ItemStack held; // stack ťahaný myšou v obrazovke inventára
    bool inventoryOpen = false;
    BlockInteraction interaction;

    const glm::vec3 SKY_COLOR(0.53f, 0.75f, 0.98f);
    const glm::vec3 WATER_FOG_COLOR(0.08f, 0.22f, 0.45f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    bool wireframe = false;
    double lastTime = glfwGetTime();
    double fpsTimer = 0.0;
    int frames = 0;

    auto setInventoryOpen = [&](bool open) {
        inventoryOpen = open;
        if (!open && !held.empty()) { // čo ostalo v ruke, vráť späť
            inventory.add(held.id, held.count);
            held.clear();
        }
        window.setCursorLocked(!open);
    };

    while (!window.shouldClose()) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        window.pollEvents();

        // --- Ovládanie okna ---
        bool clickConsumed = false; // klik, ktorý len zamkol myš, nemá kopať
        if (Input::wasKeyPressed(GLFW_KEY_ESCAPE)) {
            if (inventoryOpen) setInventoryOpen(false);
            else if (window.isCursorLocked()) window.setCursorLocked(false);
            else window.close();
        }
        if (Input::wasKeyPressed(GLFW_KEY_E))
            setInventoryOpen(!inventoryOpen);
        if (!inventoryOpen && !window.isCursorLocked() && Input::wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
            window.setCursorLocked(true);
            clickConsumed = true;
        }
        if (Input::wasKeyPressed(GLFW_KEY_F1)) {
            wireframe = !wireframe;
            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        }

        ChunkManager& chunks = game->chunks;

        // + / - mení dohľad
        if (Input::wasKeyPressed(GLFW_KEY_KP_ADD) || Input::wasKeyPressed(GLFW_KEY_EQUAL))
            chunks.setRenderDistance(chunks.renderDistance() + 2);
        if (Input::wasKeyPressed(GLFW_KEY_KP_SUBTRACT) || Input::wasKeyPressed(GLFW_KEY_MINUS))
            chunks.setRenderDistance(chunks.renderDistance() - 2);

        // N = nový svet s náhodným seedom
        if (Input::wasKeyPressed(GLFW_KEY_N)) {
            int rd = chunks.renderDistance();
            game.reset(); // najskôr zastaví vlákna starého sveta
            game = std::make_unique<GameWorld>(randomSeed(), rd);
            spawnCamera(camera, game->generator);
            continue;
        }

        // Výber v hotbare: 1–9 alebo koliesko.
        for (int i = 0; i < Inventory::HOTBAR; ++i)
            if (Input::wasKeyPressed(GLFW_KEY_1 + i)) inventory.select(i);
        if (float scroll = Input::scrollDelta(); scroll != 0.0f && !inventoryOpen)
            inventory.select(inventory.selected() + (scroll > 0 ? -1 : 1));

        if (window.isCursorLocked())
            updateCamera(camera, dt);

        chunks.update(camera.position);

        // Na ktorý blok sa pozeráme.
        RaycastHit hit = raycast(game->world, camera.position, camera.forward(), BlockInteraction::REACH);

        if (inventoryOpen) {
            if (Input::wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
                int slot = hud.slotAt(window.width(), window.height(), Input::mousePosition());
                if (slot >= 0) inventory.clickSlot(slot, held);
            }
        } else if (window.isCursorLocked() && !clickConsumed) {
            interaction.update(game->world, camera, inventory, hit, dt);
        }

        // --- Render ---
        const bool underwater = isLiquid(game->world.getBlock(
            (int)std::floor(camera.position.x), (int)std::floor(camera.position.y), (int)std::floor(camera.position.z)));
        const float viewDistance = (float)(chunks.renderDistance() * Chunk::SIZE_X);
        const glm::vec3 fogColor = underwater ? WATER_FOG_COLOR : SKY_COLOR;

        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        blockShader.bind();
        blockShader.setMat4("uView", camera.view());
        blockShader.setMat4("uProjection", camera.projection(window.aspect()));
        blockShader.setInt("uAtlas", 0);
        blockShader.setVec3("uFogColor", fogColor);
        blockShader.setFloat("uFogStart", underwater ? 0.0f : viewDistance * 0.6f);
        blockShader.setFloat("uFogEnd", underwater ? 28.0f : viewDistance - 8.0f);
        atlas.bind(0);

        auto drawChunks = [&](bool liquid) {
            game->world.forEachChunk([&](Chunk& chunk) {
                Mesh* mesh = liquid ? chunk.liquidMesh.get() : chunk.mesh.get();
                if (!mesh || mesh->faceCount() == 0) return;
                glm::vec3 origin(chunk.coord().x * Chunk::SIZE_X, 0.0f, chunk.coord().y * Chunk::SIZE_Z);
                blockShader.setMat4("uModel", glm::translate(glm::mat4(1.0f), origin));
                mesh->draw();
            });
        };

        // 1) Terén. Jeden draw call na chunk.
        drawChunks(false);

        // 2) Voda: priesvitná, nezapisuje hĺbku a je vidno aj zospodu (keď sa potápaš).
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        drawChunks(true);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // 3) Obrys bloku pod zameriavačom.
        if (hit.hit && !inventoryOpen)
            highlight.draw(hit.block, camera.projection(window.aspect()) * camera.view());

        // 4) UI
        ui.begin(window.width(), window.height());
        hud.draw(ui, inventory, inventoryOpen, held, Input::mousePosition());
        ui.end(atlas);

        window.swapBuffers();

        // --- FPS v titulku ---
        ++frames;
        fpsTimer += dt;
        if (fpsTimer >= 1.0) {
            int faces = 0;
            game->world.forEachChunk([&](Chunk& chunk) {
                if (chunk.mesh) faces += chunk.mesh->faceCount();
                if (chunk.liquidMesh) faces += chunk.liquidMesh->faceCount();
            });
            Biome biome = game->generator.biomeAt((int)std::floor(camera.position.x), (int)std::floor(camera.position.z));

            char title[300];
            std::snprintf(title, sizeof(title),
                          "Cubiq | %d FPS | XYZ %.0f %.0f %.0f | %s | ciel: %s | seed %u | %zu chunkov (+%zu) | dohlad %d | %d stien | mesh %.2f ms",
                          frames, camera.position.x, camera.position.y, camera.position.z,
                          biomeName(biome), hit.hit ? blockType(hit.id).name : "-", game->generator.seed(),
                          game->world.chunkCount(), chunks.pendingCount(), chunks.renderDistance(), faces,
                          chunks.meshMsAverage());
            window.setTitle(title);
            frames = 0;
            fpsTimer = 0.0;
        }
    }
    return 0;
}

// Cubiq.exe [seed] – bez argumentu náhodný svet.
int main(int argc, char** argv)
{
    try {
        uint32_t seed = argc > 1 ? (uint32_t)std::strtoul(argv[1], nullptr, 10) : randomSeed();
        return run(seed);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Chyba: %s\n", e.what());
#ifdef _WIN32
        MessageBoxA(nullptr, e.what(), "Cubiq - chyba", MB_ICONERROR);
#endif
        return 1;
    }
}
