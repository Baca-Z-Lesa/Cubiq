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
#include "Game/DroppedItems.h"
#include "Game/Entities.h"
#include "Game/EntityRenderer.h"
#include "Game/FarTerrain.h"
#include "Game/SectionCulling.h"
#include "Game/Gui.h"
#include "Game/Hud.h"
#include "Game/Inventory.h"
#include "Game/Player.h"
#include "Game/SaveGame.h"
#include "Game/Sky.h"
#include "Game/Vitals.h"
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

// Kopanie a stavanie.
// Survival: blok sa kope, kým sa drží tlačidlo – čas podľa tvrdosti a nástroja; vypadne predmet.
// Kreatívny: blok zmizne hneď (držaním sa opakuje každých REPEAT sekúnd), stavanie neminie bloky.
struct BlockInteraction {
    static constexpr float REACH = 6.0f;
    static constexpr float REPEAT = 0.22f;
    float breakCooldown = 0.0f;
    float placeCooldown = 0.0f;
    glm::ivec3 target{0, -1, 0}; // práve kopaný blok
    float progress = 0.0f;       // sekundy kopania
    float needed = 1.0f;         // potrebný čas
    std::mt19937 rng{std::random_device{}()};

    float breakProgress() const { return needed > 0.0f ? std::clamp(progress / needed, 0.0f, 1.0f) : 0.0f; }

    void update(World& world, const Player& player, Inventory& inventory, DroppedItems& drops, const RaycastHit& hit,
                bool creative, float dt)
    {
        breakCooldown -= dt;
        placeCooldown -= dt;

        const bool mining = Input::isMouseDown(GLFW_MOUSE_BUTTON_LEFT) && hit.hit && blockType(hit.id).breakable;
        if (!mining || hit.block != target) progress = 0.0f;

        if (creative) {
            bool breakNow = Input::wasMousePressed(GLFW_MOUSE_BUTTON_LEFT) || breakCooldown <= 0.0f;
            if (mining && breakNow) {
                world.setBlock(hit.block.x, hit.block.y, hit.block.z, BlockId::Air);
                breakCooldown = REPEAT;
            }
        } else if (mining) {
            ItemStack& tool = inventory.selectedStack();
            bool dropsItem = false;
            target = hit.block;
            needed = breakTime(hit.id, tool.empty() ? Item::Air : tool.id, dropsItem);
            progress += dt;
            if (needed >= 0.0f && progress >= needed && breakCooldown <= 0.0f) {
                world.setBlock(hit.block.x, hit.block.y, hit.block.z, BlockId::Air);
                Item drop = dropsItem ? dropOf(hit.id, std::uniform_real_distribution<float>(0.0f, 1.0f)(rng)) : Item::Air;
                if (drop != Item::Air) drops.spawn(ItemStack::of(drop), glm::vec3(hit.block) + glm::vec3(0.5f));
                const ToolType toolType = tool.empty() ? ToolType::None : itemInfo(tool.id).tool;
                if (toolType != ToolType::None && toolType != ToolType::Magic && miningInfo(hit.id).hardness > 0.0f)
                    inventory.damageSelected(toolType == ToolType::Sword ? 2 : 1);
                progress = 0.0f;
                breakCooldown = 0.12f; // krátka pauza medzi blokmi
            }
        }

        const bool placeNow = Input::wasMousePressed(GLFW_MOUSE_BUTTON_RIGHT) ||
                              (Input::isMouseDown(GLFW_MOUSE_BUTTON_RIGHT) && placeCooldown <= 0.0f);
        if (placeNow && hit.hit) {
            ItemStack& stack = inventory.selectedStack();
            glm::ivec3 p = hit.block + hit.normal;
            BlockId current = world.getBlock(p.x, p.y, p.z);
            bool free = current == BlockId::Air || isLiquid(current);
            bool insidePlayer = AABB::block(p).intersects(player.bounds()); // nezamurovať sa
            if (!stack.empty() && isBlockItem(stack.id) && free && !insidePlayer && p.y >= 0 && p.y < Chunk::SIZE_Y) {
                world.setBlock(p.x, p.y, p.z, blockOf(stack.id));
                if (!creative && --stack.count == 0) stack.clear();
                placeCooldown = REPEAT;
            }
        }
    }
};

static void giveStarterItems(Inventory& inventory, bool creative)
{
    if (!creative) return; // survival začína s prázdnymi rukami
    const BlockId starter[] = {BlockId::Grass, BlockId::Stone, BlockId::OakPlanks, BlockId::OakLog, BlockId::StoneBricks,
                               BlockId::Glass, BlockId::Lantern, BlockId::CherryLeaves, BlockId::AetheriteOre};
    for (BlockId id : starter) inventory.add(itemOf(id), Inventory::MAX_STACK);
}

// ------------------------------------------------------------------ kreslenie sveta

// Všetko, čo treba na nakreslenie sveta (zdieľané medzi hrou a pozadím menu).
struct WorldRenderer {
    Shader blockShader{"assets/shaders/block.vert", "assets/shaders/block.frag"};
    Shader waterShader{"assets/shaders/water.vert", "assets/shaders/water.frag"};
    SkyRenderer sky;
    BlockHighlight highlight;
    bool farTerrain = true; // L = zapnúť/vypnúť vzdialený terén
    VisibleSections visible;
    EntityRenderer mobRenderer;
    DroppedItemRenderer itemRenderer;
    int facesDrawn = 0; // pre F3

    // Dosah vzdialeného terénu: 6× dohľad chunkov (aspoň 512, najviac 1536 blokov).
    static float farRadiusFor(float nearRadius) { return std::clamp(nearRadius * 6.0f, 512.0f, 1536.0f); }

    void draw(GameWorld& game, const Camera& camera, float aspect, const SkyState& s, float animTime,
              const RaycastHit* hit, const Texture& atlas, const EntityManager* entities,
              const DroppedItems* drops = nullptr, float breakProgress = 0.0f)
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

        // 0) Obloha (pod vodou len farba hmly). Keď je kamera pod mrakmi, mraky patria k oblohe
        //    a terén, ktorý sa kreslí po nich, ich správne prekryje.
        const bool cloudsBehindTerrain = camera.position.y < SkyRenderer::CLOUD_BOTTOM;
        if (!underwater) sky.drawSky(s, view, projection, animTime);
        if (!underwater && cloudsBehindTerrain)
            sky.drawClouds(s, view, projection, camera.position, viewDistance, animTime);

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

        // Čo kresliť: len sekcie, ktoré môže kamera vidieť (cave culling + frustum),
        // a v nich len steny otočené ku kamere.
        const Frustum frustum(projection * view);
        const int rd = chunks.renderDistance();
        visible.compute(game.world, camera.position, rd, frustum);
        facesDrawn = 0;
        const glm::vec3 cam = camera.position;
        auto drawChunks = [&](bool liquid) {
            Shader& shader = liquid ? waterShader : blockShader;
            game.world.forEachChunk([&](Chunk& chunk) {
                Mesh* mesh = liquid ? chunk.liquidMesh.get() : chunk.mesh.get();
                if (!mesh || mesh->faceCount() == 0) return;
                const uint16_t sections = visible.mask(chunk.coord());
                if (!sections) return;
                glm::vec3 origin(chunk.coord().x * Chunk::SIZE_X, 0.0f, chunk.coord().y * Chunk::SIZE_Z);
                shader.setMat4("uModel", glm::translate(glm::mat4(1.0f), origin));
                if (liquid) {
                    mesh->draw();
                    return;
                }
                // Stena smerom +X je vidno len z kamery, ktorá je na +X strane od nej (atď.).
                const bool side[4] = {cam.z > origin.z, cam.z < origin.z + 16.0f, cam.x > origin.x, cam.x < origin.x + 16.0f};
                facesDrawn += mesh->drawGroups([&](int g) {
                    int sec = g / 6, f = g % 6;
                    if (!(sections & (1u << sec))) return false;
                    if (f < 4) return side[f];
                    return f == 4 ? cam.y > sec * 16.0f : cam.y < (sec + 1) * 16.0f;
                });
            });
        };

        // 1) Terén
        drawChunks(false);

        // 1b) Zvieratá
        if (entities) mobRenderer.draw(*entities, game.world, projection * view, camera.position, s, fogColor, fogStart, fogEnd);
        if (drops)
            itemRenderer.draw(*drops, game.world, projection * view, camera.position, s, fogColor, fogStart, fogEnd, atlas);

        // 2) Mraky nad kamerou alebo okolo nej – až po teréne, aby zakryli zem pod sebou.
        if (!underwater && !cloudsBehindTerrain)
            sky.drawClouds(s, view, projection, camera.position, viewDistance, animTime);

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
        if (hit && hit->hit) highlight.draw(hit->block, projection * view, breakProgress);
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
    bool inventoryOpen = false; // je otvorené nejaké UI (container != None)
    Container container = Container::None;
    glm::ivec3 containerPos{0};  // stôl / oltár / pec, pri ktorej hráč stojí
    CraftGrid grid;
    Furnaces furnaces;
    BlockInteraction interaction;
    EntityManager entities; // zvieratá sa neukladajú, objavujú sa znova podľa biómu
    DroppedItems drops;     // predmety na zemi sa tiež neukladajú
    Vitals vitals;
    float eatTimer = 0.0f;  // ako dlho sa drží pravé tlačidlo s jedlom
    double worldTime = 0.08;
    float autosaveTimer = 0.0f;

    static constexpr float EAT_TIME = 1.2f;

    bool creative() const { return info.creative; }

    void returnGrid() { cubiq::returnGrid(grid, inventory, drops, player.position + glm::vec3(0.0f, 1.0f, 0.0f)); }

    UiState uiState() const
    {
        UiState u;
        u.inventory = &inventory;
        u.held = &held;
        u.open = container;
        u.creative = creative();
        u.grid = &grid;
        u.furnace = container == Container::Furnace ? furnaces.find(containerPos) : nullptr;
        return u;
    }

    // Vloží stack do slotov [from, to) – najprv k rovnakým, potom do prázdnych. Vráti zvyšok.
    int insertInto(ItemStack stack, int from, int to)
    {
        for (int pass = 0; pass < 2 && stack.count > 0; ++pass)
            for (int i = from; i < to && stack.count > 0; ++i) {
                ItemStack& d = inventory.slot(i);
                if (pass == 0 && !d.empty() && d.id == stack.id && d.count < d.maxStack() && stack.maxStack() > 1) {
                    int n = std::min(stack.count, d.maxStack() - d.count);
                    d.count += n;
                    stack.count -= n;
                } else if (pass == 1 && d.empty()) {
                    d = stack;
                    stack.count = 0;
                }
            }
        return stack.count;
    }

    // Shift+klik na slot inventára: do pece, alebo medzi hotbarom a batohom.
    void quickMove(int index)
    {
        ItemStack& s = inventory.slot(index);
        if (s.empty()) return;
        if (container == Container::Furnace) {
            Furnace& f = furnaces.at(containerPos);
            ItemStack* target = smeltResult(s.id) != Item::Air ? &f.input : fuelTime(s.id) > 0.0f ? &f.fuel : nullptr;
            if (target) {
                if (target->empty()) { *target = s; s.clear(); }
                else if (target->id == s.id) {
                    int n = std::min(s.count, target->maxStack() - target->count);
                    target->count += n;
                    if ((s.count -= n) <= 0) s.clear();
                }
                return;
            }
        }
        const bool fromHotbar = index < Inventory::HOTBAR;
        int left = fromHotbar ? insertInto(s, Inventory::HOTBAR, Inventory::SIZE) : insertInto(s, 0, Inventory::HOTBAR);
        if (left == 0) s.clear();
        else s.count = left;
    }

    // Klik na výsledok craftingu (so Shiftom vyrobí čo najviac rovno do inventára).
    void takeCraftResult(bool all)
    {
        for (int n = 0; n < 64; ++n) {
            const Recipe* r = matchRecipe(grid);
            if (!r) return;
            const ItemStack result = r->result;
            if (all) {
                int left = inventory.add(result);
                if (left > 0) {
                    ItemStack rest = result;
                    rest.count = left;
                    drops.spawn(rest, player.position + glm::vec3(0.0f, 1.0f, 0.0f));
                }
            } else {
                if (!held.empty() && (held.id != result.id || held.count + result.count > held.maxStack())) return;
                if (held.empty()) held = result;
                else held.count += result.count;
            }
            consumeGrid(grid);
            if (!all) return;
        }
    }

    // Smrť: všetko z inventára vypadne na zem.
    void dropEverything()
    {
        returnGrid();
        const glm::vec3 at = player.position + glm::vec3(0.0f, 1.0f, 0.0f);
        for (int i = 0; i < Inventory::SIZE; ++i) {
            drops.spawn(inventory.slot(i), at);
            inventory.slot(i).clear();
        }
        drops.spawn(held, at);
        held.clear();
    }

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
        if (!held.empty()) info.inventory.add(held);
        for (const ItemStack& c : grid.cells) info.inventory.add(c);
        info.furnaces = furnaces.all();
        info.health = vitals.health;
        info.hunger = vitals.hunger;
        info.saturation = vitals.saturation;
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
        s->player.flying = info.flying && info.creative;
        s->camera.yaw = info.yaw;
        s->camera.pitch = info.pitch;
        s->camera.position = s->player.eyePosition();
        s->inventory = info.inventory;
        s->vitals.health = info.health;
        s->vitals.hunger = info.hunger;
        s->vitals.saturation = info.saturation;
        s->furnaces.all() = info.furnaces;
        if (s->vitals.dead()) { // uložené počas smrti → začni znova na spawne
            s->vitals.reset();
            spawnPlayer(s->player, s->camera, s->game->generator);
        }
    } else {
        spawnPlayer(s->player, s->camera, s->game->generator, startAt);
        giveStarterItems(s->inventory, info.creative);
    }
    return s;
}

// ------------------------------------------------------------------ menu

enum class Screen { Title, Worlds, Create, Game, Paused, Dead };

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

static int run(const uint32_t* quickSeed, const glm::ivec2* startAt, bool quickSurvival)
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
    bool newCreative = false;
    int focusedField = 0;
    std::string statusMessage;
    float statusTimer = 0.0f;

    // Príkazový riadok (Cubiq.exe seed [x z]) = rýchla hra bez menu a bez ukladania.
    if (quickSeed) {
        WorldInfo quick;
        quick.name = "Rychla hra";
        quick.seed = *quickSeed;
        quick.creative = !quickSurvival;
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
    // Otvorenie inventára / stola / oltára / pece (None = zavrieť).
    auto openContainer = [&](Container c, glm::ivec3 pos) {
        Session& s = *session;
        if (s.container != Container::None) {
            // Zatvorenie: mriežka a to, čo ostalo v ruke, sa vráti do inventára.
            s.returnGrid();
            if (!s.held.empty()) {
                int left = s.inventory.add(s.held);
                if (left > 0) { // nezmestí sa → vyhodiť pred hráča
                    ItemStack rest = s.held;
                    rest.count = left;
                    s.drops.spawn(rest, s.camera.position, s.camera.forward() * 4.0f);
                }
                s.held.clear();
            }
        }
        s.container = c;
        s.inventoryOpen = c != Container::None;
        s.containerPos = pos;
        s.grid.size = (c == Container::Table || c == Container::Altar) ? 3 : 2;
        s.grid.station = c == Container::Table ? Station::Table : c == Container::Altar ? Station::Altar : Station::Hand;
        if (c == Container::Furnace) s.furnaces.at(pos);
        window.setCursorLocked(!s.inventoryOpen);
    };
    auto setInventoryOpen = [&](bool open) { openContainer(open ? Container::Inventory : Container::None, glm::ivec3(0)); };

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

        const bool inGame = screen == Screen::Game || screen == Screen::Paused || screen == Screen::Dead;
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
                if (Input::wasKeyPressed(GLFW_KEY_F) && s.creative()) s.player.toggleFlying();
                // Q = vyhodiť jeden kus z ruky (s Ctrl celý stack).
                if (Input::wasKeyPressed(GLFW_KEY_Q) && !s.inventoryOpen && !s.inventory.selectedStack().empty()) {
                    ItemStack& sel = s.inventory.selectedStack();
                    ItemStack thrown = sel;
                    thrown.count = Input::isKeyDown(GLFW_KEY_LEFT_CONTROL) ? sel.count : 1;
                    sel.count -= thrown.count;
                    if (sel.count <= 0) sel.clear();
                    s.drops.spawn(thrown, s.camera.position - glm::vec3(0.0f, 0.3f, 0.0f), s.camera.forward() * 5.0f);
                }

                for (int i = 0; i < Inventory::HOTBAR; ++i)
                    if (Input::wasKeyPressed(GLFW_KEY_1 + i)) s.inventory.select(i);
                if (float scroll = Input::scrollDelta(); scroll != 0.0f && !s.inventoryOpen)
                    s.inventory.select(s.inventory.selected() + (scroll > 0 ? -1 : 1));
            } else if (screen == Screen::Paused && Input::wasKeyPressed(GLFW_KEY_ESCAPE)) {
                screen = Screen::Game;
                window.setCursorLocked(true);
            }

            // Pauza zastaví čas aj hráča (svet sa ďalej načítava a kreslí).
            // Pri smrti beží svet ďalej (predmety dopadnú na zem), hráč stojí.
            const bool running = screen == Screen::Game;
            if (screen == Screen::Dead) s.drops.update(g.world, s.inventory, glm::vec3(0.0f, -1000.0f, 0.0f), dt);
            const bool playing = running && window.isCursorLocked() && !s.inventoryOpen;
            if (running) {
                s.worldTime += dt / DAY_LENGTH_SECONDS;
                if (playing) mouseLook(s.camera);
                if (worldReadyAt(g.world, s.player.position)) {
                    // Ak hráč uviazol v bloku (spawn pod previsom, strom...), vytlač ho nahor.
                    for (int i = 0; i < 64 && collidesWithWorld(g.world, s.player.bounds()); ++i)
                        s.player.position.y += 1.0f;
                    glm::vec3 flatForward = glm::normalize(glm::vec3(s.camera.forward().x, 0.0f, s.camera.forward().z));
                    Player::Controls controls = readControls(playing);
                    if (!s.creative() && s.vitals.hunger <= 6.0f) controls.sprint = false; // hladný nešprintuje
                    s.player.update(g.world, controls, flatForward, s.camera.right(), dt);

                    if (!s.creative()) {
                        s.vitals.update(g.world, s.player, dt);
                        if (s.vitals.dead()) {
                            s.dropEverything();
                            s.inventoryOpen = false;
                            s.container = Container::None;
                            screen = Screen::Dead;
                            window.setCursorLocked(false);
                        }
                    }
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

            if (running) {
                const float dayPart = (float)(s.worldTime - std::floor(s.worldTime));
                const bool night = computeSky(dayPart).sunBrightness < 0.4f;
                s.entities.update(g.world, g.generator, s.player.position, dt, night, !s.creative());
                glm::vec3 hitDir;
                if (float dmg = s.entities.takePlayerDamage(hitDir); dmg > 0.0f && !s.creative() && !s.vitals.dead()) {
                    if (s.vitals.damage(dmg, "matoha")) s.player.velocity += hitDir * 7.0f + glm::vec3(0.0f, 4.5f, 0.0f);
                }
                s.drops.update(g.world, s.inventory, s.player.position + glm::vec3(0.0f, 0.8f, 0.0f), dt);
                s.furnaces.update(g.world, s.drops, dt);
                // Stôl / pec zničená alebo hráč odišiel ďaleko → zavrieť.
                if (s.container == Container::Table || s.container == Container::Altar || s.container == Container::Furnace) {
                    const glm::ivec3 p = s.containerPos;
                    const BlockId need = s.container == Container::Table ? BlockId::CraftingTable
                                       : s.container == Container::Altar ? BlockId::RunicAltar : BlockId::Furnace;
                    if (g.world.getBlock(p.x, p.y, p.z) != need ||
                        glm::length(glm::vec3(p) + glm::vec3(0.5f) - s.player.position) > 8.0f)
                        setInventoryOpen(false);
                }
            }

            hit = raycast(g.world, s.camera.position, s.camera.forward(), BlockInteraction::REACH);
            if (screen == Screen::Game) {
                if (s.inventoryOpen) {
                    const bool rightClick = Input::wasMousePressed(GLFW_MOUSE_BUTTON_RIGHT);
                    const bool shift = Input::isKeyDown(GLFW_KEY_LEFT_SHIFT);
                    if (clicked || rightClick) {
                        const UiState uiSt = s.uiState();
                        const UiTarget t = hud.targetAt(window.width(), window.height(), Input::mousePosition(), uiSt);
                        Furnace* furnace = s.container == Container::Furnace ? &s.furnaces.at(s.containerPos) : nullptr;
                        switch (t.kind) {
                        case UiTarget::Slot:
                            if (shift && clicked) s.quickMove(t.index);
                            else clickStack(s.inventory.slot(t.index), s.held, rightClick);
                            break;
                        case UiTarget::Catalog:
                            // Kreatívny katalóg: ľavé = plný stack, pravé = 1 kus; s predmetom v ruke ho zahodí.
                            if (s.held.empty()) s.held = ItemStack::of(t.item, rightClick ? 1 : itemInfo(t.item).maxStack);
                            else s.held.clear();
                            break;
                        case UiTarget::Craft:
                            clickStack(s.grid.cells[t.index], s.held, rightClick);
                            break;
                        case UiTarget::Result:
                            s.takeCraftResult(shift);
                            break;
                        case UiTarget::Book: {
                            const std::vector<int> book = Hud::bookRecipes(s.grid);
                            if (t.index < (int)book.size()) fillGrid(allRecipes()[book[t.index]], s.inventory, s.grid);
                            break;
                        }
                        case UiTarget::FurnaceInput:
                            if (furnace) clickStack(furnace->input, s.held, rightClick);
                            break;
                        case UiTarget::FurnaceFuel:
                            if (furnace) clickStack(furnace->fuel, s.held, rightClick);
                            break;
                        case UiTarget::FurnaceOutput:
                            // Z výstupu sa len berie.
                            if (furnace && !furnace->output.empty()) {
                                if (shift) {
                                    furnace->output.count = s.inventory.add(furnace->output);
                                    if (furnace->output.count <= 0) furnace->output.clear();
                                } else if (s.held.empty() || (s.held.id == furnace->output.id &&
                                                               s.held.count + furnace->output.count <= s.held.maxStack())) {
                                    if (s.held.empty()) s.held = furnace->output;
                                    else s.held.count += furnace->output.count;
                                    furnace->output.clear();
                                }
                            }
                            break;
                        default: break;
                        }
                    }
                } else if (window.isCursorLocked() && !clickConsumed && Input::wasMousePressed(GLFW_MOUSE_BUTTON_RIGHT) &&
                           hit.hit && !Input::isKeyDown(GLFW_KEY_LEFT_SHIFT) &&
                           (hit.id == BlockId::CraftingTable || hit.id == BlockId::RunicAltar || hit.id == BlockId::Furnace)) {
                    // Pravý klik na stôl / oltár / pec ho otvorí (so Shiftom sa dá na ne stavať).
                    openContainer(hit.id == BlockId::CraftingTable ? Container::Table
                                  : hit.id == BlockId::RunicAltar  ? Container::Altar
                                                                   : Container::Furnace,
                                  hit.block);
                } else if (window.isCursorLocked() && !clickConsumed) {
                    // Úder do zvieraťa má prednosť pred blokom, ak je bližšie.
                    float mobDist = 0.0f;
                    int mob = s.entities.raycast(s.camera.position, s.camera.forward(), BlockInteraction::REACH, mobDist);
                    if (mob >= 0 && (!hit.hit || mobDist < hit.distance) && Input::wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
                        const ItemStack& weapon = s.inventory.selectedStack();
                        const int damage = weapon.empty() ? 1 : itemInfo(weapon.id).damage;
                        const Mob victim = s.entities.mobs()[mob];
                        if (s.entities.hit(mob, s.camera.forward(), damage) && !s.creative()) {
                            // Mäso: jeleň 2–3, ovečka 1–2, žabka nič.
                            int meat = victim.type == MobType::Deer ? 2 + (victim.id & 1)
                                     : victim.type == MobType::Sheep ? 1 + (victim.id & 1) : 0;
                            if (meat > 0) s.drops.spawn(ItemStack::of(Item::RawMeat, meat), victim.position + glm::vec3(0.0f, 0.5f, 0.0f));
                            // Mátoha: tieňová esencia (na runy) 1–2.
                            if (victim.type == MobType::Wraith)
                                s.drops.spawn(ItemStack::of(Item::ShadowEssence, 1 + (victim.id % 2)), victim.position + glm::vec3(0.0f, 1.0f, 0.0f));
                        }
                        if (!weapon.empty() && itemInfo(weapon.id).tool == ToolType::Sword && !s.creative())
                            s.inventory.damageSelected(1);
                        s.interaction.breakCooldown = BlockInteraction::REPEAT; // tento klik nekope
                    } else {
                        s.interaction.update(g.world, s.player, s.inventory, s.drops, hit, s.creative(), dt);
                    }

                    // Jedenie: držať pravé tlačidlo s jedlom v ruke (len keď je hlad).
                    ItemStack& food = s.inventory.selectedStack();
                    const bool canEat = !s.creative() && !food.empty() && itemInfo(food.id).food > 0 &&
                                        s.vitals.hunger < Vitals::MAX_HUNGER;
                    if (canEat && Input::isMouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
                        s.eatTimer += dt;
                        if (s.eatTimer >= Session::EAT_TIME) {
                            s.vitals.eat(itemInfo(food.id).food);
                            if (--food.count <= 0) food.clear();
                            s.eatTimer = 0.0f;
                        }
                    } else {
                        s.eatTimer = 0.0f;
                    }
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
                          (screen == Screen::Game && !s.inventoryOpen) ? &hit : nullptr, atlas, &s.entities, &s.drops,
                          s.interaction.breakProgress());
        } else {
            renderer.draw(*background->game, background->camera, window.aspect(), sky, animTime, nullptr, atlas, nullptr);
        }

        ui.begin(window.width(), window.height());
        Gui gui(ui, Input::mousePosition(), clicked && !window.isCursorLocked());
        const float S = gui.scale();
        const float W = gui.width(), H = gui.height();
        const glm::vec4 WHITE(1.0f), GRAY(0.7f, 0.72f, 0.76f, 1.0f), ACCENT(0.62f, 0.9f, 0.5f, 1.0f);
        const float bw = 150.0f * S, bh = 20.0f * S, gap = 6.0f * S; // tlačidlo

        if (inGame) {
            Session& s = *session;
            if (!s.creative() && !s.inventoryOpen)
                hud.drawVitals(ui, s.vitals, s.eatTimer / Session::EAT_TIME, animTime);
            hud.draw(ui, s.uiState(), Input::mousePosition());

            if (screen == Screen::Dead) {
                ui.rect(0, 0, W, H, glm::vec4(0.45f, 0.0f, 0.0f, 0.45f));
                gui.text("Zomrel si", W * 0.5f, H * 0.26f, 5.0f * S, glm::vec4(1.0f, 0.85f, 0.85f, 1.0f), true);
                std::string cause = std::string("Pricina: ") + s.vitals.lastDamage;
                gui.text(cause.c_str(), W * 0.5f, H * 0.26f + 44.0f * S, 1.5f * S, WHITE, true);
                gui.text("Predmety ostali lezat na mieste smrti", W * 0.5f, H * 0.26f + 58.0f * S, S, GRAY, true);
                float x = W * 0.5f - bw * 0.5f, y = H * 0.50f;
                if (gui.button("Znovu ozit", x, y, bw, bh)) {
                    s.vitals.reset();
                    s.interaction.progress = 0.0f;
                    spawnPlayer(s.player, s.camera, s.game->generator);
                    s.camera.position = s.player.eyePosition();
                    screen = Screen::Game;
                    window.setCursorLocked(true);
                }
                y += bh + gap;
                if (gui.button(s.persistent ? "Ulozit a odist do menu" : "Odist do menu", x, y, bw, bh)) leaveToMenu();
            }

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
                    fmt("Svet: %s   seed %u   mod: %s", s.info.name.c_str(), g.generator.seed(), s.creative() ? "kreativny" : "survival"),
                    fmt("Predmety na zemi: %zu", s.drops.items().size()),
                    fmt("Chunky: %zu nacitanych, %zu v rade, dohlad %d", g.world.chunkCount(), g.chunks.pendingCount(), g.chunks.renderDistance()),
                    fmt("Steny: %d, kreslenych %d (%.0f%%)", shownFaces, renderer.facesDrawn,
                        shownFaces ? 100.0 * renderer.facesDrawn / shownFaces : 0.0),
                    fmt("Viditelne sekcie: %d   mesh %.2f ms/chunk", renderer.visible.visibleCount(), g.chunks.meshMsAverage()),
                    fmt("Zvierata: %zu / %d   matohy: %d / %d", s.entities.mobs().size() - s.entities.hostileCount(),
                        EntityManager::MAX_MOBS - EntityManager::MAX_HOSTILE, s.entities.hostileCount(), EntityManager::MAX_HOSTILE),
                    renderer.farTerrain ? fmt("Vzdialeny teren: %d/%zu dlazdic do %.0f blokov (L)", g.distant.drawnTiles(), g.distant.tileCount(),
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

            y += fh + 22.0f * S;
            gui.text("Herny mod", x, y - 11.0f * S, S, GRAY, false);
            if (gui.button(newCreative ? "Mod: Kreativny" : "Mod: Survival", x, y, fw, bh)) newCreative = !newCreative;
            gui.text(newCreative ? "nekonecne bloky, let (F), okamzite kopanie"
                                 : "zivoty, hlad, nastroje, crafting a magia",
                     W * 0.5f, y + bh + 4.0f * S, S, GRAY, true);

            y += bh + 26.0f * S;
            const float sbw = (fw - gap) * 0.5f;
            bool create = gui.button("Vytvorit svet", x, y, sbw, bh) || Input::wasKeyPressed(GLFW_KEY_ENTER);
            if (create) {
                WorldInfo w = createWorld(newName, seedFromText(newSeed), newCreative);
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
// Cubiq.exe seed [x z] [survival] – rýchla hra bez ukladania (x z = miesto spawnu), predvolene kreatívna
int main(int argc, char** argv)
{
    try {
        uint32_t seed = 0;
        glm::ivec2 start;
        bool survival = argc > 1 && std::string(argv[argc - 1]) == "survival";
        if (survival) --argc;
        bool hasSeed = argc > 1, hasStart = argc > 3;
        if (hasSeed) seed = (uint32_t)std::strtoul(argv[1], nullptr, 10);
        if (hasStart) start = {std::atoi(argv[2]), std::atoi(argv[3])};
        return run(hasSeed ? &seed : nullptr, hasStart ? &start : nullptr, survival);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Chyba: %s\n", e.what());
#ifdef _WIN32
        MessageBoxA(nullptr, e.what(), "Cubiq - chyba", MB_ICONERROR);
#endif
        return 1;
    }
}
