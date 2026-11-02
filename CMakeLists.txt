cmake_minimum_required(VERSION 3.20)

# Prvá konfigurácia je vo Visual Studiu predvolená. RelWithDebInfo je optimalizovaná
# (Debug je pri generovaní sveta 10–50× pomalší), ale stále sa v nej dá ladiť.
set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Debug;Release" CACHE STRING "" FORCE)

project(Cubiq VERSION 0.11.0 LANGUAGES CXX C)

# Statický C++ runtime (/MT): hotové Cubiq.exe beží aj na PC bez Visual C++ Redistributable.
# Policy default platí aj pre závislosti so starým cmake_minimum_required (glew).
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- Závislosti (stiahnu sa automaticky pri prvom configure) ---
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1)

set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ONLY_LIBS ON CACHE BOOL "" FORCE)
FetchContent_Declare(glew
    GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
    GIT_TAG glew-cmake-2.2.0)

FetchContent_MakeAvailable(glfw glm glew)

# --- Engine: všeobecné veci (okno, shadery, kamera...) ---
add_library(CubiqEngine STATIC
    src/Engine/Window.cpp
    src/Engine/Input.cpp
    src/Engine/Shader.cpp
    src/Engine/Texture.cpp
    src/Engine/Mesh.cpp
    src/Engine/Camera.cpp
    src/Engine/UIRenderer.cpp
)
target_include_directories(CubiqEngine PUBLIC src)
target_link_libraries(CubiqEngine PUBLIC glfw glm::glm libglew_static)

# --- World: voxelová logika (bloky, chunky, generovanie, meshing) ---
add_library(CubiqWorld STATIC
    src/World/Block.cpp
    src/World/Chunk.cpp
    src/World/World.cpp
    src/World/ChunkMesher.cpp
    src/World/TerrainGenerator.cpp
    src/World/ChunkManager.cpp
    src/World/Noise.cpp
    src/World/Raycast.cpp
    src/World/Physics.cpp
    src/World/Features.cpp
    src/World/Structures.cpp
    src/World/WorldStorage.cpp
)
target_link_libraries(CubiqWorld PUBLIC CubiqEngine)

# --- Hra ---
add_executable(Cubiq
    src/Game/main.cpp
    src/Game/BlockAtlas.cpp
    src/Game/BlockHighlight.cpp
    src/Game/Inventory.cpp
    src/Game/Hud.cpp
    src/Game/Player.cpp
    src/Game/DebugOverlay.cpp
    src/Game/Sky.cpp
    src/Game/SaveGame.cpp
    src/Game/Gui.cpp
    src/Game/FarTerrain.cpp
    src/Game/SectionCulling.cpp
    src/Game/Pathfinder.cpp
    src/Game/Entities.cpp
    src/Game/EntityRenderer.cpp
    src/Game/Item.cpp
    src/Game/DroppedItems.cpp
    src/Game/Vitals.cpp
)
if(WIN32)
    # Ikona aplikácie a údaje o verzii v .exe; bez konzolového okna v Release.
    target_sources(Cubiq PRIVATE installer/Cubiq.rc)
    set_target_properties(Cubiq PROPERTIES WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)
    target_link_options(Cubiq PRIVATE $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>)
endif()
target_link_libraries(Cubiq PRIVATE CubiqWorld)

# assets/ sa skopíruje vedľa .exe pri každom builde
add_custom_command(TARGET Cubiq POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Cubiq>/assets)

if(MSVC)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Cubiq)
    set_target_properties(Cubiq PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:Cubiq>)
endif()
