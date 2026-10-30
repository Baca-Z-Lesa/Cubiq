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
#include "World/ChunkMesher.h"
#include "World/TerrainGenerator.h"
#include "World/World.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <exception>
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

static int run()
{
    Window window(1280, 720, "Cubiq");
    window.setCursorLocked(true);

    Shader blockShader("assets/shaders/block.vert", "assets/shaders/block.frag");
    Texture atlas(TILE_SIZE * ATLAS_TILES, TILE_SIZE, generateBlockAtlas());
    // Pevný svet 8x8 chunkov. Vo fáze 4 sa budú chunky načítavať okolo hráča.
    const int WORLD_RADIUS = 4;
    World world;
    for (int cx = -WORLD_RADIUS; cx < WORLD_RADIUS; ++cx)
        for (int cz = -WORLD_RADIUS; cz < WORLD_RADIUS; ++cz)
            generateTerrain(world.createChunk({cx, cz}));

    Camera camera({0.5f, 75.0f, 0.5f});
    camera.pitch = -25.0f;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);
    glClearColor(0.53f, 0.75f, 0.98f, 1.0f); // obloha

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

        if (window.isCursorLocked())
            updateCamera(camera, dt);

        // --- Render ---
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        blockShader.bind();
        blockShader.setMat4("uView", camera.view());
        blockShader.setMat4("uProjection", camera.projection(window.aspect()));
        blockShader.setInt("uAtlas", 0);
        atlas.bind(0);

        // Jeden draw call na chunk. Mesh sa prestavia len keď sa chunk zmenil.
        world.forEachChunk([&](Chunk& chunk) {
            if (chunk.dirty) {
                chunk.mesh = buildChunkMesh(world, chunk, ATLAS_TILES);
                chunk.dirty = false;
            }
            glm::vec3 origin(chunk.coord().x * Chunk::SIZE_X, 0.0f, chunk.coord().y * Chunk::SIZE_Z);
            blockShader.setMat4("uModel", glm::translate(glm::mat4(1.0f), origin));
            chunk.mesh->draw();
        });

        window.swapBuffers();

        // --- FPS v titulku ---
        ++frames;
        fpsTimer += dt;
        if (fpsTimer >= 1.0) {
            int faces = 0;
            world.forEachChunk([&](Chunk& chunk) { if (chunk.mesh) faces += chunk.mesh->faceCount(); });

            char title[160];
            std::snprintf(title, sizeof(title), "Cubiq | %d FPS | XYZ %.1f %.1f %.1f | %d stien",
                          frames, camera.position.x, camera.position.y, camera.position.z, faces);
            window.setTitle(title);
            frames = 0;
            fpsTimer = 0.0;
        }
    }
    return 0;
}

int main()
{
    try {
        return run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Chyba: %s\n", e.what());
#ifdef _WIN32
        MessageBoxA(nullptr, e.what(), "Cubiq - chyba", MB_ICONERROR);
#endif
        return 1;
    }
}
