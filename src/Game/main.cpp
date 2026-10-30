#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h> // musí byť pred GLFW, inak koliduje makro APIENTRY
#endif

#include "Engine/Camera.h"
#include "Engine/Input.h"
#include "Engine/Shader.h"
#include "Engine/Texture.h"
#include "Engine/Window.h"
#include "Game/BlockAtlas.h"
#include "World/ChunkManager.h"
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

static int run(uint32_t seed)
{
    Window window(1280, 720, "Cubiq");
    window.setCursorLocked(true);

    Shader blockShader("assets/shaders/block.vert", "assets/shaders/block.frag");
    Texture atlas(TILE_SIZE * ATLAS_TILES, TILE_SIZE, generateBlockAtlas());

    auto game = std::make_unique<GameWorld>(seed, 12); // render distance v chunkoch

    Camera camera;
    spawnCamera(camera, game->generator);

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

    while (!window.shouldClose()) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        window.pollEvents();

        // --- Ovládanie okna ---
        if (Input::wasKeyPressed(GLFW_KEY_ESCAPE)) {
            if (window.isCursorLocked()) window.setCursorLocked(false);
            else window.close();
        }
        if (!window.isCursorLocked() && Input::wasMousePressed(GLFW_MOUSE_BUTTON_LEFT))
            window.setCursorLocked(true);
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

        if (window.isCursorLocked())
            updateCamera(camera, dt);

        chunks.update(camera.position);

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

            char title[256];
            std::snprintf(title, sizeof(title),
                          "Cubiq | %d FPS | XYZ %.0f %.0f %.0f | %s | seed %u | %zu chunkov (+%zu) | dohlad %d | %d stien | mesh %.2f ms",
                          frames, camera.position.x, camera.position.y, camera.position.z,
                          biomeName(biome), game->generator.seed(),
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
