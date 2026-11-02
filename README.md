# Cubiq

Voxelový svet od nuly v C++ (OpenGL 3.3, GLFW, GLM, GLEW).

## Build

Závislosti sa stiahnu automaticky cez CMake FetchContent (treba internet a git).

**Visual Studio 2022:** *File → Open → Folder…* → vyber tento priečinok → zvoľ `Cubiq.exe` a stlač F5.

**Príkazový riadok:**

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config RelWithDebInfo
build\RelWithDebInfo\Cubiq.exe
```

Bez argumentov sa spustí **hlavné menu** (zoznam svetov, nový svet s názvom a seedom).
Svety sa ukladajú do `%APPDATA%\Cubiq\saves\<svet>`: `world.dat` (hráč, inventár, čas) a `region/`
(len zmenené chunky, RLE). Automaticky každú minútu, pri odchode do menu a pri zatvorení okna.

Rýchla hra bez ukladania: `Cubiq.exe 1337` (seed) alebo `Cubiq.exe 7 192 96` (seed, x, z) – napr. lietajúce ostrovy v seede 7.

Predvolená konfigurácia je **RelWithDebInfo** – optimalizovaná (60 FPS), a pritom sa v nej dá ladiť.
**Debug** je pri načítavaní sveta výrazne pomalší (~20 FPS), používaj ho len keď ladíš konkrétnu chybu.

## Installer

```
powershell -ExecutionPolicy Bypass -File installer\build-installer.ps1
```

Zostaví Release a vytvorí `dist\Cubiq-<verzia>-setup.exe` (potrebný [Inno Setup 6](https://jrsoftware.org/isinfo.php)).
Installer nepotrebuje administrátora, vytvorí odkaz v Štart menu (voliteľne na ploche) a dá sa odinštalovať
cez Nastavenia → Aplikácie. Hra je zostavená so statickým C++ runtime, takže beží aj na PC bez Visual Studia.
Uložené svety v `%APPDATA%\Cubiq` ostávajú aj po odinštalovaní.

## Ovládanie

| Kláves | Akcia |
|---|---|
| Myš | rozhliadanie |
| Ľavé tlačidlo | kopať (držaním opakovane) / udrieť zviera |
| Pravé tlačidlo | postaviť blok z hotbaru |
| 1–9 / koliesko | výber v hotbare |
| E | inventár (klik = vziať / položiť / vymeniť stack) |
| WASD | chôdza |
| Space | skok / plávanie hore / let hore |
| Shift | zakrádanie / let dole |
| Ctrl | šprint |
| F | zapnúť / vypnúť lietanie |
| F1 | wireframe |
| F3 | ladiace informácie (FPS, pozícia, biom, chunky, GPU…) |
| F11 | celá obrazovka |
| + / - | väčší / menší dohľad (chunky) |
| L | vzdialený terén (zjednodušená krajina až do 4× dohľadu) |
| Esc | pauza (uložiť, odísť do menu) |
| T | posunúť čas o 2 hodiny (deň trvá 20 minút) |
| Esc | zavrieť inventár / uvoľniť myš (2× = koniec) |

## Štruktúra

- `src/Engine/` – všeobecný engine (okno, vstup, shader, textúra, mesh, kamera)
- `src/World/` – voxelová logika (bloky, chunky 16×256×16, svet, generovanie terénu, meshing s face cullingom)
- `src/Game/` – hra (main loop, textúrový atlas)
- `assets/shaders/` – GLSL shadery (kopírujú sa vedľa .exe)
