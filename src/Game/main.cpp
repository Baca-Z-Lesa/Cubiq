#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h> // musí byť pred GLFW, inak koliduje makro APIENTRY
#endif

#include "Engine/Camera.h"
#include "Engine/Frustum.h"
#include "Engine/Input.h"
#include "Engine/Shader.h"
#include "Engine/Texture.h"
#include "Engine/UIRenderer.h"
#include "Engine/Window.h"
#include "Game/BlockAtlas.h"
#include "Game/BlockHighlight.h"
#include "Game/DebugOverlay.h"
#include "Game/FarTerrain.h"
#include "Game/Gui.h"
#include "Game/Hud.h"
#include "Game/Inventory.h"
#include "Game/Player.h"
#include "Game/SaveGame.h"
#include "Game/Sky.h"
#include "World/ChunkManager.h"
#include "World/Raycast.h"
#include "World/TerrainGenerator.h"
#include "World/World.h"
#include "World/WorldStorage.h"

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
#include <thread>
#include <vector>

using namespace cubiq;

static void mouseLook(Camera& camera)
{
    const float sensitivity = 0.1f;
    glm::vec2 md = Input::mouseDelta();
    camera.rotate(md.x * sensitivity, -md.y * sensitivity);
}

static Player::Controls readControls(bool active)
{
    Player::Controls c;
    if (!active) return c;
    if (Input::isKeyDown(GLFW_KEY_W)) c.move.y += 1.0f;
    if (Input::isKeyDown(GLFW_KEY_S)) c.move.y -= 1.0f;
    if (Input::isKeyDown(GLFW_KEY_D)) c.move.x += 1.0f;
    if (Input::isKeyDown(GLFW_KEY_A)) c.move.x -= 1.0f;
    c.jump = Input::isKeyDown(GLFW_KEY_SPACE);
    c.sneak = Input::isKeyDown(GLFW_KEY_LEFT_SHIFT);
    c.sprint = Input::isKeyDown(GLFW_KEY_LEFT_CONTROL);
    return c;
}

// Fyzika beží až keď je chunk pod hráčom vygenerovaný a má mesh – inak by prepadol do prázdna.
static bool worldReadyAt(const World& world, const glm::vec3& pos)
{
    const Chunk* chunk = world.getChunk(World::chunkCoordOf((int)std::floor(pos.x), (int)std::floor(pos.z)));
    return chunk && chunk->mesh;
}

// Svet = bloky + generátor + streaming chunkov (+ voliteľne ukladanie na disk).
struct GameWorld {
    TerrainGenerator generator;
    World world;
    ChunkManager chunks;
    FarTerrain distant; // zjednodušená krajina za dohľadom chunkov (pozor: "far" je makro z windows.h)

    GameWorld(uint32_t seed, WorldStorage* storage, int renderDistance)
        : generator(seed), chunks(world, generator, storage, renderDistance, ATLAS_TILES), distant(generator)
    {
    }
};

static void spawnPlayer(Player& player, Camera& camera, const TerrainGenerator& generator,
                        const glm::ivec2* at = nullptr)
{
    glm::ivec2 spawn = at ? *at : generator.findSpawn();
    int ground = std::max(generator.surfaceAt(spawn.x, spawn.y).y, TerrainGenerator::SEA_LEVEL);
    player.teleport(glm::vec3(spawn.x + 0.5f, ground + 1.0f, spawn.y + 0.5f));
    player.flying = false;
    camera.position = player.eyePosition();
    camera.pitch = -10.0f;
}

static uint32_t randomSeed() { return std::random_device{}(); }

// Kopanie a stavanie. Držaním tlačidla sa opakuje každých REPEAT sekúnd.
struct BlockInteraction {
    static constexpr float REACH = 6.0f;
    static constexpr float REPEAT = 0.22f;
    float breakCooldown = 0.0f;
    float placeCooldown = 0.0f;

    void update(World& world, const Player& player, Inventory& inventory, const RaycastHit& hit, float dt)
    {
        breakCooldown -= dt;
        placeCooldown -= dt;

        bool breakNow = Input::wasMousePressed(GLFW_MOUSE_BUTTON_LEFT) ||
                        (Input::isMouseDown(GLFW_MOUSE_BUTTON_LEFT) && breakCooldown <= 0.0f);
        bool placeNow = Input::wasMousePressed(GLFW_MOUSE_BUTTON_RIGHT) ||
                        (Input::isMouseDown(GLFW_MOUSE_BUTTON_RIGHT) && placeCooldown <= 0.0f);

        if (breakNow && hit.hit && blockType(hit.id).breakable) {
            world.setBlock(hit.block.x, hit.block.y, hit.block.z, BlockId::Air);
            inventory.add(hit.id, 1);
            breakCooldown = REPEAT;
        }

        if (placeNow && hit.hit) {
            ItemStack& stack = inventory.selectedStack();
            glm::ivec3 p = hit.block + hit.normal;
            BlockId current = world.getBlock(p.x, p.y, p.z);
            bool free = current == BlockId::Air || isLiquid(current);
            bool insidePlayer = AABB::block(p).intersects(player.bounds()); // nezamurovať sa
            if (!stack.empty() && free && !insidePlayer && p.y >= 0 && p.y < Chunk::SIZE_Y) {
                world.setBlock(p.x, p.y, p.z, stack.id);
                if (--stack.count == 0) stack.clear();
                placeCooldown = REPEAT;
            }
        }
    }
};

static void giveStarterItems(Inventory& inventory)
{
    const BlockId starter[] = {BlockId::Grass, BlockId::Stone, BlockId::OakPlanks, BlockId::OakLog, BlockId::StoneBricks,
                               BlockId::Glass, BlockId::Lantern, BlockId::CherryLeaves, BlockId::AetheriteOre};
    for (BlockId id : starter) inventory.add(id, Inventory::MAX_STACK);
}

// ------------------------------------------------------------------ kreslenie sveta

// Všetko, čo treba na nakreslenie sveta (zdieľané medzi hrou a pozadím menu).
struct WorldRenderer {
    Shader blockShader{"assets/shaders/block.vert", "assets/shaders/block.frag"};
    Shader waterShader{"assets/shaders/water.vert", "assets/shaders/water.frag"};
    SkyRenderer sky;
    BlockHighlight highlight;
    bool farTerrain = true; // L = zapnúť/vypnúť vzdialený terén

    // Dosah vzdialeného terénu: 4× dohľad chunkov (aspoň 384, najviac 1024 blokov).
    static float farRadiusFor(float nearRadius) { return std::clamp(nearRadius * 4.0f, 384.0f, 1024.0f); }

    void draw(GameWorld& game, const Camera& camera, float aspect, const SkyState& s, float animTime,
              const RaycastHit* hit, const Texture& atlas)
    {
        const glm::vec3 WATER_FOG_COLOR(0.08f, 0.22f, 0.45f);
        ChunkManager& chunks = game.chunks;
        const bool underwater = isLiquid(game.world.getBlock(
            (int)std::floor(camera.position.x), (int)std::floor(camera.position.y), (int)std::floor(camera.position.z)));
        const float nearRadius = (float)(chunks.renderDistance() * Chunk::SIZE_X);
        const float farRadius = farRadiusFor(nearRadius);
        // Hmla má farbu oblohy pri obzore, takže terén v diaľke plynule splynie s oblohou.
        // So vzdialeným terénom začína až ďaleko za chunkami.
        const float viewDistance = farTerrain ? farRadius : nearRadius;
        const glm::vec3 fogColor = underwater ? WATER_FOG_COLOR * std::max(s.sunBrightness, 0.3f) : s.horizon;
        const float fogStart = underwater ? 0.0f : viewDistance * (farTerrain ? 0.35f : 0.6f);
        const float fogEnd = underwater ? 28.0f : viewDistance - 8.0f;
        const glm::mat4 view = camera.view(), projection = camera.projection(aspect);

        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 0) Obloha (pod vodou len farba hmly).
        if (!underwater) sky.drawSky(s, view, projection, animTime);

        // 0b) Vzdialený terén – vnútri dohľadu chunkov ho shader vystrihne.
        if (farTerrain && !underwater) {
            game.distant.update(camera.position, farRadius);
            game.distant.draw(projection * view, camera.position, s, fogColor, fogStart, fogEnd, nearRadius - 16.0f);
        }

        blockShader.bind();
        blockShader.setMat4("uView", view);
        blockShader.setMat4("uProjection", projection);
        blockShader.setInt("uAtlas", 0);
        blockShader.setFloat("uAtlasTiles", (float)ATLAS_TILES);
        blockShader.setVec3("uFogColor", fogColor);
        blockShader.setFloat("uSunBrightness", s.sunBrightness);
        blockShader.setVec3("uSunColor", s.sunColor);
        blockShader.setFloat("uFogStart", fogStart);
        blockShader.setFloat("uFogEnd", fogEnd);
        atlas.bind(0);

        // Kreslia sa len chunky v zornom poli kamery (frustum culling) a v dohľade.
        const Frustum frustum(projection * view);
        const glm::ivec2 center = World::chunkCoordOf((int)std::floor(camera.position.x), (int)std::floor(camera.position.z));
        const int rd = chunks.renderDistance();
        auto drawChunks = [&](bool liquid) {
            Shader& shader = liquid ? waterShader : blockShader;
            game.world.forEachChunk([&](Chunk& chunk) {
                Mesh* mesh = liquid ? chunk.liquidMesh.get() : chunk.mesh.get();
                if (!mesh || mesh->faceCount() == 0) return;
                glm::ivec2 d = chunk.coord() - center;
                if (d.x * d.x + d.y * d.y > rd * rd) return;
                glm::vec3 origin(chunk.coord().x * Chunk::SIZE_X, 0.0f, chunk.coord().y * Chunk::SIZE_Z);
                glm::vec3 size(Chunk::SIZE_X, chunk.maxY() + 2.0f, Chunk::SIZE_Z);
                if (!frustum.intersects(origin, origin + size)) return;
                shader.setMat4("uModel", glm::translate(glm::mat4(1.0f), origin));
                mesh->draw();
            });
        };

        // 1) Terén
        drawChunks(false);

        // 2) Mraky (pod vodou ich nevidno)
        if (!underwater) sky.drawClouds(s, projection * view, camera.position, viewDistance, animTime);

        // 3) Voda a láva: vlny, odraz oblohy, odlesk slnka. Priesvitné, viditeľné aj zospodu.
        waterShader.bind();
        waterShader.setMat4("uView", view);
        waterShader.setMat4("uProjection", projection);
        waterShader.setInt("uAtlas", 0);
        waterShader.setFloat("uAtlasTiles", (float)ATLAS_TILES);
        waterShader.setFloat("uTime", animTime);
        waterShader.setVec3("uCameraPos", camera.position);
        waterShader.setVec3("uSunDir", s.sunDir);
        waterShader.setVec3("uSunColor", s.sunColor);
        waterShader.setFloat("uSunBrightness", s.sunBrightness);
        waterShader.setVec3("uZenith", s.zenith);
        waterShader.setVec3("uHorizon", s.horizon);
        waterShader.setVec3("uFogColor", fogColor);
        waterShader.setFloat("uFogStart", fogStart);
        waterShader.setFloat("uFogEnd", fogEnd);
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        drawChunks(true);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // 4) Obrys bloku pod zameriavačom
        if (hit && hit->hit) highlight.draw(hit->block, projection * view);
    }
};

// ------------------------------------------------------------------ rozohraný svet

struct Session {
    WorldInfo info;
    bool persistent = false; // false = rýchla hra z príkazového riadku, neukladá sa
    std::unique_ptr<WorldStorage> storage;
    std::unique_ptr<GameWorld> game;
    Player player;
    Camera camera;
    Inventory inventory;
    ItemStack held;
    bool inventoryOpen = false;
    BlockInteraction interaction;
    double worldTime = 0.08;
    float autosaveTimer = 0.0f;

    void save()
    {
        if (!persistent) return;
        info.hasPlayer = true;
        info.position = player.position;
        info.yaw = camera.yaw;
        info.pitch = camera.pitch;
        info.flying = player.flying;
        info.time = worldTime;
        info.inventory = inventory;
        if (!held.empty()) info.inventory.add(held.id, held.count);
        info.lastPlayed = nowUnix();
        saveWorldInfo(info);
        game->chunks.saveAll();
        autosaveTimer = 0.0f;
    }
};

static std::unique_ptr<Session> startSession(const WorldInfo& info, bool persistent, int renderDistance,
                                             const glm::ivec2* startAt = nullptr)
{
    auto s = std::make_unique<Session>();
    s->info = info;
    s->persistent = persistent;
    if (persistent) s->storage = std::make_unique<WorldStorage>(info.folder / "region");
    s->game = std::make_unique<GameWorld>(info.seed, s->storage.get(), renderDistance);
    s->worldTime = info.time;

    if (info.hasPlayer) {
        s->player.teleport(info.position);
        s->player.flying = info.flying;
        s->camera.yaw = info.yaw;
        s->camera.pitch = info.pitch;
        s->camera.position = s->player.eyePosition();
        s->inventory = info.inventory;
    } else {
        spawnPlayer(s->player, s->camera, s->game->generator, startAt);
        giveStarterItems(s->inventory);
    }
    return s;
}

// ------------------------------------------------------------------ menu

enum class Screen { Title, Worlds, Create, Game, Paused };

// Pozadie menu: skutočný svet (lietajúce ostrovy v seede 7), kamera sa pomaly otáča.
struct MenuBackground {
    std::unique_ptr<GameWorld> game;
    Camera camera;

    MenuBackground()
    {
        game = std::make_unique<GameWorld>(7u, nullptr, 8);
        camera.position = glm::vec3(180.5f, 120.0f, 128.5f);
        camera.pitch = -10.0f;
    }
};

static int run(const uint32_t* quickSeed, const glm::ivec2* startAt)
{
    Window window(1280, 720, "Cubiq");

    Texture atlas(TILE_SIZE * ATLAS_TILES, TILE_SIZE, generateBlockAtlas());
    WorldRenderer renderer;
    UIRenderer ui;
    Hud hud;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int renderDistance = 12;
    std::unique_ptr<Session> session;
    std::unique_ptr<MenuBackground> background;
    Screen screen = Screen::Title;

    // Stav obrazoviek menu
    std::vector<WorldInfo> worlds;
    int selectedWorld = -1;
    float worldScroll = 0.0f;
    bool confirmDelete = false;
    std::string newName = "Novy svet", newSeed;
    int focusedField = 0;
    std::string statusMessage;
    float statusTimer = 0.0f;

    // Príkazový riadok (Cubiq.exe seed [x z]) = rýchla hra bez menu a bez ukladania.
    if (quickSeed) {
        WorldInfo quick;
        quick.name = "Rychla hra";
        quick.seed = *quickSeed;
        session = startSession(quick, false, renderDistance, startAt);
        screen = Screen::Game;
        window.setCursorLocked(true);
    } else {
        background = std::make_unique<MenuBackground>();
    }

    auto enterWorld = [&](const WorldInfo& info) {
        background.reset(); // uvoľní vlákna a pamäť pozadia
        session = startSession(info, true, renderDistance);
        session->info.lastPlayed = nowUnix();
        saveWorldInfo(session->info);
        screen = Screen::Game;
        window.setCursorLocked(true);
    };
    auto leaveToMenu = [&] {
        if (session) session->save();
        session.reset();
        background = std::make_unique<MenuBackground>();
        worlds = listWorlds();
        selectedWorld = worlds.empty() ? -1 : 0;
        screen = Screen::Title;
        window.setCursorLocked(false);
    };
    auto setInventoryOpen = [&](bool open) {
        Session& s = *session;
        s.inventoryOpen = open;
        if (!open && !s.held.empty()) { // čo ostalo v ruke, vráť späť
            s.inventory.add(s.held.id, s.held.count);
            s.held.clear();
        }
        window.setCursorLocked(!open);
    };

    bool wireframe = false;
    bool showDebug = false;
    double lastTime = glfwGetTime();
    double fpsTimer = 0.0;
    int frames = 0;
    double updateMs = 0.0, renderMs = 0.0; // súčty časov častí snímky za poslednú sekundu
    int shownFps = 0;
    double shownUpdateMs = 0.0, shownRenderMs = 0.0, shownFrameMs = 0.0;
    int shownFaces = 0;
    float animTime = 0.0f;
    const std::string gpuName = (const char*)glGetString(GL_RENDERER);
    const std::string glVersion = (const char*)glGetString(GL_VERSION);
    constexpr double DAY_LENGTH_SECONDS = 20.0 * 60.0;

    while (!window.shouldClose()) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        animTime += dt;
        statusTimer -= dt;

        window.pollEvents();

        if (Input::wasKeyPressed(GLFW_KEY_F11)) window.toggleFullscreen();
        if (Input::wasKeyPressed(GLFW_KEY_F3)) showDebug = !showDebug;
        if (Input::wasKeyPressed(GLFW_KEY_L)) renderer.farTerrain = !renderer.farTerrain;
        if (Input::wasKeyPressed(GLFW_KEY_F1)) {
            wireframe = !wireframe;
            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        }

        const bool inGame = screen == Screen::Game || screen == Screen::Paused;
        bool clickConsumed = false; // klik, ktorý len zamkol myš alebo kliklo menu, nemá kopať
        const bool clicked = Input::wasMousePressed(GLFW_MOUSE_BUTTON_LEFT);

        // ---------------------------------------------------------------- hra
        RaycastHit hit;
        float timeOfDay = 0.0f;
        SkyState sky{};
        if (inGame) {
            Session& s = *session;
            GameWorld& g = *s.game;
            ChunkManager& chunks = g.chunks;

            if (screen == Screen::Game) {
                if (Input::wasKeyPressed(GLFW_KEY_ESCAPE)) {
                    if (s.inventoryOpen) setInventoryOpen(false);
                    else { screen = Screen::Paused; window.setCursorLocked(false); }
                }
                if (Input::wasKeyPressed(GLFW_KEY_E)) setInventoryOpen(!s.inventoryOpen);
                if (!s.inventoryOpen && !window.isCursorLocked() && clicked) {
                    window.setCursorLocked(true);
                    clickConsumed = true;
                }
                if (Input::wasKeyPressed(GLFW_KEY_KP_ADD) || Input::wasKeyPressed(GLFW_KEY_EQUAL))
                    chunks.setRenderDistance(renderDistance = std::min(renderDistance + 2, 32));
                if (Input::wasKeyPressed(GLFW_KEY_KP_SUBTRACT) || Input::wasKeyPressed(GLFW_KEY_MINUS))
                    chunks.setRenderDistance(renderDistance = std::max(renderDistance - 2, 2));
                if (Input::wasKeyPressed(GLFW_KEY_T)) s.worldTime += 1.0 / 12.0;
                if (Input::wasKeyPressed(GLFW_KEY_F)) s.player.toggleFlying();

                for (int i = 0; i < Inventory::HOTBAR; ++i)
                    if (Input::wasKeyPressed(GLFW_KEY_1 + i)) s.inventory.select(i);
                if (float scroll = Input::scrollDelta(); scroll != 0.0f && !s.inventoryOpen)
                    s.inventory.select(s.inventory.selected() + (scroll > 0 ? -1 : 1));
            } else if (Input::wasKeyPressed(GLFW_KEY_ESCAPE)) {
                screen = Screen::Game;
                window.setCursorLocked(true);
            }

            // Pauza zastaví čas aj hráča (svet sa ďalej načítava a kreslí).
            const bool running = screen == Screen::Game;
            const bool playing = running && window.isCursorLocked() && !s.inventoryOpen;
            if (running) {
                s.worldTime += dt / DAY_LENGTH_SECONDS;
                if (playing) mouseLook(s.camera);
                if (worldReadyAt(g.world, s.player.position)) {
                    // Ak hráč uviazol v bloku (spawn pod previsom, strom...), vytlač ho nahor.
                    for (int i = 0; i < 64 && collidesWithWorld(g.world, s.player.bounds()); ++i)
                        s.player.position.y += 1.0f;
                    glm::vec3 flatForward = glm::normalize(glm::vec3(s.camera.forward().x, 0.0f, s.camera.forward().z));
                    s.player.update(g.world, readControls(playing), flatForward, s.camera.right(), dt);
                }
                float targetFov = s.player.sprinting ? 80.0f : 70.0f;
                s.camera.fov += (targetFov - s.camera.fov) * std::min(1.0f, dt * 10.0f);

                // Automatické uloženie každú minútu.
                s.autosaveTimer += dt;
                if (s.persistent && s.autosaveTimer > 60.0f) {
                    s.save();
                    statusMessage = "Svet ulozeny";
                    statusTimer = 2.0f;
                }
            }
            s.camera.position = s.player.eyePosition();

            double tUpdate0 = glfwGetTime();
            chunks.update(s.player.position);
            updateMs += (glfwGetTime() - tUpdate0) * 1000.0;

            hit = raycast(g.world, s.camera.position, s.camera.forward(), BlockInteraction::REACH);
            if (screen == Screen::Game) {
                if (s.inventoryOpen) {
                    if (clicked) {
                        int slot = hud.slotAt(window.width(), window.height(), Input::mousePosition());
                        if (slot >= 0) s.inventory.clickSlot(slot, s.held);
                    }
                } else if (window.isCursorLocked() && !clickConsumed) {
                    s.interaction.update(g.world, s.player, s.inventory, hit, dt);
                }
            }

            timeOfDay = (float)(s.worldTime - std::floor(s.worldTime));
            sky = computeSky(timeOfDay);
        } else {
            // Pozadie menu: pomaly sa otáčajúca kamera nad krajinou s ostrovmi, zlatá hodinka.
            background->camera.yaw += dt * 3.0f;
            background->game->chunks.update(background->camera.position);
            timeOfDay = 0.07f;
            sky = computeSky(timeOfDay);
        }

        // ---------------------------------------------------------------- kreslenie
        double tRender0 = glfwGetTime();
        if (inGame) {
            Session& s = *session;
            renderer.draw(*s.game, s.camera, window.aspect(), sky, animTime,
                          (screen == Screen::Game && !s.inventoryOpen) ? &hit : nullptr, atlas);
        } else {
            renderer.draw(*background->game, background->camera, window.aspect(), sky, animTime, nullptr, atlas);
        }

        ui.begin(window.width(), window.height());
        Gui gui(ui, Input::mousePosition(), clicked && !window.isCursorLocked());
        const float S = gui.scale();
        const float W = gui.width(), H = gui.height();
        const glm::vec4 WHITE(1.0f), GRAY(0.7f, 0.72f, 0.76f, 1.0f), ACCENT(0.62f, 0.9f, 0.5f, 1.0f);
        const float bw = 150.0f * S, bh = 20.0f * S, gap = 6.0f * S; // tlačidlo

        if (inGame) {
            Session& s = *session;
            hud.draw(ui, s.inventory, s.inventoryOpen, s.held, Input::mousePosition());

            if (!worldReadyAt(s.game->world, s.player.position)) {
                gui.dim(0.65f);
                gui.text("Nacitavam svet...", W * 0.5f, H * 0.5f - 10.0f * S, 3.0f * S, WHITE, true);
            }

            if (screen == Screen::Paused) {
                gui.dim(0.45f);
                gui.text("Pauza", W * 0.5f, H * 0.28f, 4.0f * S, WHITE, true);
                float x = W * 0.5f - bw * 0.5f, y = H * 0.40f;
                if (gui.button("Pokracovat", x, y, bw, bh)) { screen = Screen::Game; window.setCursorLocked(true); }
                y += bh + gap;
                if (gui.button("Ulozit", x, y, bw, bh, s.persistent)) {
                    s.save();
                    statusMessage = "Svet ulozeny";
                    statusTimer = 2.0f;
                }
                y += bh + gap;
                if (gui.button(s.persistent ? "Ulozit a odist do menu" : "Odist do menu", x, y, bw, bh)) {
                    leaveToMenu();
                }
                if (!s.persistent)
                    gui.text("Rychla hra z prikazoveho riadku sa neuklada", W * 0.5f, y + bh + gap * 2, S, GRAY, true);
            }

            if (showDebug && session) {
                GameWorld& g = *s.game;
                DebugOverlay dbg;
                auto fmt = [](const char* f, auto... args) {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf), f, args...);
                    return std::string(buf);
                };
                const glm::vec3 p = s.player.position;
                const glm::ivec3 b((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z));
                const glm::ivec2 cc = World::chunkCoordOf(b.x, b.z);
                const glm::vec3 fwd = s.camera.forward();
                const char* facing = std::fabs(fwd.x) > std::fabs(fwd.z) ? (fwd.x > 0 ? "vychod (+X)" : "zapad (-X)")
                                                                          : (fwd.z > 0 ? "juh (+Z)" : "sever (-Z)");
                const TerrainGenerator::Column col = g.generator.columnAt(b.x, b.z);
                const float speed = glm::length(glm::vec2(s.player.velocity.x, s.player.velocity.z));
                char clock[8];
                formatClock(timeOfDay, clock, sizeof(clock));

                dbg.left = {
                    fmt("Cubiq  %d FPS  (%.1f ms/snimka)", shownFps, shownFrameMs),
                    fmt("chunky %.2f ms   render %.2f ms", shownUpdateMs, shownRenderMs),
                    "",
                    fmt("XYZ: %.3f / %.3f / %.3f", p.x, p.y, p.z),
                    fmt("Blok: %d %d %d", b.x, b.y, b.z),
                    fmt("Chunk: %d %d   v chunku: %d %d", cc.x, cc.y, b.x - cc.x * Chunk::SIZE_X, b.z - cc.y * Chunk::SIZE_Z),
                    fmt("Smer: %s   yaw %.1f  pitch %.1f", facing, s.camera.yaw, s.camera.pitch),
                    fmt("Rychlost: %.2f b/s  vertikalne %.2f", speed, s.player.velocity.y),
                    fmt("Stav: %s%s", s.player.flying ? "let" : s.player.inWater ? "plavanie" : s.player.onGround ? "na zemi" : "vo vzduchu",
                        s.player.sprinting ? ", sprint" : s.player.sneaking ? ", zakradanie" : ""),
                    "",
                    fmt("Biom: %s", biomeName(col.biome)),
                    fmt("Vyska terenu: %d   more: %d   snezna ciara: %d", col.height, TerrainGenerator::SEA_LEVEL, col.snowLine),
                    fmt("Teren 3D: %.2f   ostrov: %.2f", col.overhang, col.islandShape),
                    hit.hit ? fmt("Ciel: %s @ %d %d %d", blockType(hit.id).name, hit.block.x, hit.block.y, hit.block.z)
                            : std::string("Ciel: -"),
                    "",
                    fmt("Svet: %s   seed %u", s.info.name.c_str(), g.generator.seed()),
                    fmt("Chunky: %zu nacitanych, %zu v rade, dohlad %d", g.world.chunkCount(), g.chunks.pendingCount(), g.chunks.renderDistance()),
                    fmt("Steny: %d   mesh %.2f ms/chunk (%d)", shownFaces, g.chunks.meshMsAverage(), g.chunks.meshedTotal()),
                    renderer.farTerrain ? fmt("Vzdialeny teren: %zu dlazdic do %.0f blokov (L)", g.distant.tileCount(),
                                              WorldRenderer::farRadiusFor((float)(g.chunks.renderDistance() * Chunk::SIZE_X)))
                                        : std::string("Vzdialeny teren: vypnuty (L)"),
                };
                dbg.right = {
                    fmt("%s", glVersion.c_str()),
                    fmt("%s", gpuName.c_str()),
                    fmt("Okno: %d x %d%s", window.width(), window.height(), window.isFullscreen() ? "  (cela obrazovka)" : ""),
                    fmt("CPU vlakna: %u", std::thread::hardware_concurrency()),
                    fmt("Pamat blokov: %.0f MB", g.world.chunkCount() * (double)(Chunk::SIZE_X * Chunk::SIZE_Y * Chunk::SIZE_Z) / (1024.0 * 1024.0)),
                    fmt("FOV: %.0f", s.camera.fov),
                    fmt("Cas: %s  den %d  (slnko %.0f%%)", clock, (int)s.worldTime + 1, sky.sunBrightness * 100.0f),
                    s.persistent ? fmt("Autosave o %.0f s", std::max(0.0f, 60.0f - s.autosaveTimer)) : std::string("Neuklada sa"),
                    "",
                    "F1 drotovy model   F3 toto okno",
                    "F11 cela obrazovka   T +2 hodiny",
                    "F let   +/- dohlad   L vzdialeny teren",
                };
                dbg.draw(ui);
            }
        } else if (screen == Screen::Title) {
            // Logo s jemným "dýchaním" farby
            float pulse = 0.85f + 0.15f * std::sin(animTime * 1.5f);
            const float logo = 9.0f * S, logoY = H * 0.14f;
            gui.text("CUBIQ", W * 0.5f, logoY, logo, glm::vec4(ACCENT.r * pulse, ACCENT.g * pulse, ACCENT.b, 1.0f), true);
            gui.text("voxelovy svet od nuly v C++ a OpenGL", W * 0.5f, logoY + 7.0f * logo + 8.0f * S, 1.5f * S, WHITE, true);

            float x = W * 0.5f - bw * 0.5f, y = H * 0.54f;
            if (gui.button("Hrat", x, y, bw, bh)) {
                worlds = listWorlds();
                selectedWorld = worlds.empty() ? -1 : 0;
                confirmDelete = false;
                screen = worlds.empty() ? Screen::Create : Screen::Worlds;
            }
            y += bh + gap;
            if (gui.button("Novy svet", x, y, bw, bh)) {
                newName = "Novy svet";
                newSeed.clear();
                focusedField = 0;
                screen = Screen::Create;
            }
            y += bh + gap;
            if (gui.button("Koniec", x, y, bw, bh)) window.close();

            gui.text("F11 cela obrazovka", 6.0f * S, H - 12.0f * S, S, GRAY);
            gui.text("verzia 0.10", W - 6.0f * S - UIRenderer::textWidth("verzia 0.10", S), H - 12.0f * S, S, GRAY, false);
        } else if (screen == Screen::Worlds) {
            gui.dim(0.35f);
            gui.text("Vyber svet", W * 0.5f, 18.0f * S, 4.0f * S, WHITE, true);

            const float listW = 300.0f * S, itemH = 30.0f * S;
            const float listX = W * 0.5f - listW * 0.5f, listY = 60.0f * S, listH = H - listY - 70.0f * S;
            gui.panel(listX - 4 * S, listY - 4 * S, listW + 8 * S, listH + 8 * S);

            float maxScroll = std::max(0.0f, worlds.size() * (itemH + 3 * S) - listH);
            worldScroll = std::clamp(worldScroll - Input::scrollDelta() * 30.0f * S, 0.0f, maxScroll);
            for (int i = 0; i < (int)worlds.size(); ++i) {
                float y = listY + i * (itemH + 3 * S) - worldScroll;
                if (y < listY - 1 || y + itemH > listY + listH + 1) continue; // mimo viditeľnej časti
                const WorldInfo& w = worlds[i];
                std::string sub = "naposledy " + formatDate(w.lastPlayed) + "   seed " + std::to_string(w.seed);
                if (gui.listItem(w.name.c_str(), sub.c_str(), listX, y, listW, itemH, i == selectedWorld)) {
                    if (i == selectedWorld) { enterWorld(worlds[i]); break; } // dvojklik = hrať
                    selectedWorld = i;
                    confirmDelete = false;
                }
            }
            if (worlds.empty()) gui.text("Zatial nemas ziadny svet", W * 0.5f, listY + 20 * S, 2 * S, GRAY, true);

            if (screen == Screen::Worlds) {
                const float sbw = 100.0f * S, y1 = H - 56.0f * S, y2 = y1 + bh + gap;
                const bool has = selectedWorld >= 0 && selectedWorld < (int)worlds.size();
                if (gui.button("Hrat", W * 0.5f - sbw - gap * 0.5f, y1, sbw, bh, has)) enterWorld(worlds[selectedWorld]);
                else if (gui.button("Novy svet", W * 0.5f + gap * 0.5f, y1, sbw, bh)) {
                    newName = "Novy svet";
                    newSeed.clear();
                    focusedField = 0;
                    screen = Screen::Create;
                } else if (gui.button(confirmDelete ? "Naozaj vymazat?" : "Vymazat", W * 0.5f - sbw - gap * 0.5f, y2, sbw, bh, has)) {
                    if (confirmDelete) {
                        deleteWorld(worlds[selectedWorld]);
                        worlds = listWorlds();
                        selectedWorld = worlds.empty() ? -1 : 0;
                        confirmDelete = false;
                    } else {
                        confirmDelete = true;
                    }
                } else if (gui.button("Spat", W * 0.5f + gap * 0.5f, y2, sbw, bh) || Input::wasKeyPressed(GLFW_KEY_ESCAPE)) {
                    screen = Screen::Title;
                }
                if (Input::wasKeyPressed(GLFW_KEY_ENTER) && has) enterWorld(worlds[selectedWorld]);
            }
        } else if (screen == Screen::Create) {
            gui.dim(0.35f);
            gui.text("Novy svet", W * 0.5f, H * 0.16f, 4.0f * S, WHITE, true);

            const float fw = 220.0f * S, fh = 20.0f * S, x = W * 0.5f - fw * 0.5f;
            float y = H * 0.32f;
            gui.text("Nazov sveta", x, y - 11.0f * S, S, GRAY, false);
            if (gui.textField(newName, "Novy svet", x, y, fw, fh, focusedField == 0, 32)) focusedField = 0;
            y += fh + 22.0f * S;
            gui.text("Seed (prazdne = nahodny, cislo alebo lubovolny text)", x, y - 11.0f * S, S, GRAY, false);
            if (gui.textField(newSeed, "nahodny", x, y, fw, fh, focusedField == 1, 32)) focusedField = 1;
            if (Input::wasKeyPressed(GLFW_KEY_TAB)) focusedField = 1 - focusedField;

            y += fh + 26.0f * S;
            const float sbw = (fw - gap) * 0.5f;
            bool create = gui.button("Vytvorit svet", x, y, sbw, bh) || Input::wasKeyPressed(GLFW_KEY_ENTER);
            if (create) {
                WorldInfo w = createWorld(newName, seedFromText(newSeed));
                enterWorld(w);
            } else if (gui.button("Zrusit", x + fw - sbw, y, sbw, bh) || Input::wasKeyPressed(GLFW_KEY_ESCAPE)) {
                worlds = listWorlds();
                screen = worlds.empty() ? Screen::Title : Screen::Worlds;
            }
        }

        if (statusTimer > 0.0f && !statusMessage.empty())
            gui.text(statusMessage.c_str(), W * 0.5f, H - 46.0f * S, 1.5f * S, glm::vec4(1, 1, 1, std::min(1.0f, statusTimer)), true);

        ui.end(atlas);
        renderMs += (glfwGetTime() - tRender0) * 1000.0;
        window.swapBuffers();

        // --- FPS a titulok (raz za sekundu) ---
        ++frames;
        fpsTimer += dt;
        if (fpsTimer >= 1.0) {
            int faces = 0;
            GameWorld* g = session ? session->game.get() : background ? background->game.get() : nullptr;
            if (g) g->world.forEachChunk([&](Chunk& c) {
                if (c.mesh) faces += c.mesh->faceCount();
                if (c.liquidMesh) faces += c.liquidMesh->faceCount();
            });
            char title[360];
            if (session) {
                const Player& p = session->player;
                std::snprintf(title, sizeof(title),
                              "Cubiq | %d FPS | XYZ %.1f %.1f %.1f | %s | %s | ciel: %s | seed %u | %zu chunkov (+%zu) | dohlad %d | %d stien | chunky %.1f ms, render %.1f ms",
                              frames, p.position.x, p.position.y, p.position.z,
                              p.flying ? "let" : p.inWater ? "plavanie" : p.onGround ? "na zemi" : "vo vzduchu",
                              biomeName(g->generator.biomeAt((int)std::floor(p.position.x), (int)std::floor(p.position.z))),
                              hit.hit ? blockType(hit.id).name : "-", g->generator.seed(), g->world.chunkCount(),
                              g->chunks.pendingCount(), g->chunks.renderDistance(), faces, updateMs / frames, renderMs / frames);
            } else {
                std::snprintf(title, sizeof(title), "Cubiq | %d FPS | menu", frames);
            }
            window.setTitle(title);
            shownFps = frames;
            shownUpdateMs = updateMs / frames;
            shownRenderMs = renderMs / frames;
            shownFrameMs = fpsTimer * 1000.0 / frames;
            shownFaces = faces;
            frames = 0;
            fpsTimer = 0.0;
            updateMs = renderMs = 0.0;
        }
    }

    // Zatvorenie okna počas hry → uložiť.
    if (session) session->save();
    return 0;
}

// Cubiq.exe              – hlavné menu
// Cubiq.exe seed [x z]   – rýchla hra bez ukladania (x z = miesto spawnu)
int main(int argc, char** argv)
{
    try {
        uint32_t seed = 0;
        glm::ivec2 start;
        bool hasSeed = argc > 1, hasStart = argc > 3;
        if (hasSeed) seed = (uint32_t)std::strtoul(argv[1], nullptr, 10);
        if (hasStart) start = {std::atoi(argv[2]), std::atoi(argv[3])};
        return run(hasSeed ? &seed : nullptr, hasStart ? &start : nullptr);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Chyba: %s\n", e.what());
#ifdef _WIN32
        MessageBoxA(nullptr, e.what(), "Cubiq - chyba", MB_ICONERROR);
#endif
        return 1;
    }
}
