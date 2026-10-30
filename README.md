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

Konkrétny svet: `Cubiq.exe 1337` (seed ako argument, bez neho náhodný).

Na hranie použi **Release** (`--config Release`): meshing chunkov je tam ~6× rýchlejší než v Debug.

## Ovládanie

| Kláves | Akcia |
|---|---|
| Myš | rozhliadanie |
| Ľavé tlačidlo | kopať (držaním opakovane) |
| Pravé tlačidlo | postaviť blok z hotbaru |
| 1–9 / koliesko | výber v hotbare |
| E | inventár (klik = vziať / položiť / vymeniť stack) |
| WASD | pohyb |
| Space / Shift | hore / dole |
| Ctrl | rýchlejší let |
| F1 | wireframe |
| + / - | väčší / menší dohľad (chunky) |
| N | nový svet s náhodným seedom |
| Esc | zavrieť inventár / uvoľniť myš (2× = koniec) |

## Štruktúra

- `src/Engine/` – všeobecný engine (okno, vstup, shader, textúra, mesh, kamera)
- `src/World/` – voxelová logika (bloky, chunky 16×256×16, svet, generovanie terénu, meshing s face cullingom)
- `src/Game/` – hra (main loop, textúrový atlas)
- `assets/shaders/` – GLSL shadery (kopírujú sa vedľa .exe)
