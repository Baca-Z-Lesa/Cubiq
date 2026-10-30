# Cubiq

Voxelový svet od nuly v C++ (OpenGL 3.3, GLFW, GLM, GLEW).

## Build

Závislosti sa stiahnu automaticky cez CMake FetchContent (treba internet a git).

**Visual Studio 2022:** *File → Open → Folder…* → vyber tento priečinok → zvoľ `Cubiq.exe` a stlač F5.

**Príkazový riadok:**

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
build\Debug\Cubiq.exe
```

## Ovládanie

| Kláves | Akcia |
|---|---|
| Myš | rozhliadanie |
| WASD | pohyb |
| Space / Shift | hore / dole |
| Ctrl | rýchlejší let |
| F1 | wireframe |
| Esc | uvoľniť myš (2× = koniec) |
| Ľavý klik | znova zamknúť myš |

## Štruktúra

- `src/Engine/` – všeobecný engine (okno, vstup, shader, textúra, mesh, kamera)
- `src/World/` – voxelová logika (bloky, chunky 16×256×16, svet, generovanie terénu, meshing s face cullingom)
- `src/Game/` – hra (main loop, textúrový atlas)
- `assets/shaders/` – GLSL shadery (kopírujú sa vedľa .exe)
