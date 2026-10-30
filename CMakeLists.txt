cmake_minimum_required(VERSION 3.20)
project(Cubiq LANGUAGES CXX C)

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
)
target_link_libraries(CubiqWorld PUBLIC CubiqEngine)

# --- Hra ---
add_executable(Cubiq
    src/Game/main.cpp
    src/Game/BlockAtlas.cpp
)
target_link_libraries(Cubiq PRIVATE CubiqWorld)

# assets/ sa skopíruje vedľa .exe pri každom builde
add_custom_command(TARGET Cubiq POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Cubiq>/assets)

if(MSVC)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Cubiq)
    set_target_properties(Cubiq PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:Cubiq>)
endif()
