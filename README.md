# final-year-project

Final year project: a small 2D tile-based game built on a custom C99 engine (ECS + Chipmunk2D physics) with Raylib used as the platform/renderer backend.

## Quick start (Linux)

### Prereqs

- C compiler: `gcc` or `clang`
- Build tools: `git`, `cmake`, `make`
- X11/OpenGL dev packages (needed by Raylib/GLFW)

Ubuntu/Debian example:

```bash
sudo apt install build-essential cmake git \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev \
  libxcursor-dev libxinerama-dev libxxf86vm-dev libasound2-dev
```

### Build third-party deps (once)

```bash
./third_party/setup.sh
```

This initializes `git` submodules, applies local patches, and builds:
- `third_party/raylib` (static library)
- `third_party/Chipmunk2D`
- `third_party/xml.c`

### Build + run

```bash
# Build tool (only needed if ./nob isn't already present)
cc -O2 -o nob build.c

# Build the game (default is release)
./nob

# Run (from repo root so relative assets paths work)
./build/game
```

### Headless build (no window)

Useful for quick simulation/CI-style runs.

```bash
./nob --headless
HEADLESS_MAX_FRAMES=600 ./build/game_headless
```

Build flags:
- `--debug` enables extra debug toggles/overlays
- `--release` forces release flags
- `--headless` builds `build/game_headless`

## Controls

- Move: WASD / arrow keys
- Interact: `E` (buy hat near vendor)
- Lift/throw: `C`
- Screenshot: `Print Screen` (saves to `./screenshots/` as `screenshot_#####.png`)
- Debug build extras:
  - `Space` reloads assets and prints asset debug
  - `1/2/3` toggle collider overlays
  - `4` toggle trigger overlay
  - `5` toggle inspect mode (click entity to log components)
  - `R` reloads current TMX map
  - `` ` `` toggles FPS overlay

## Game overview

- Collect coins.
- Vendor NPC sells a hat; buying swaps the player sprite and makes the vendor follow the player.
- Doors open/close based on proximity using Tiled tile animations.

Content is authored in Tiled (`assets/maps/*.tmx`) and entity prefabs (`assets/prefabs/*.ent`).

## Engine design (high level)

- Fixed timestep simulation (60Hz) with variable render framerate.
- ECS with SoA component storage and phase-based system scheduling (`PHASE_INPUT`, `PHASE_PHYSICS`, `PHASE_SIM_*`, `PHASE_PRESENT`).
- 2D physics via Chipmunk2D (bodies in Chipmunk, transforms synced back into ECS).
- Tile/world pipeline:
  - TMX parsing + tilesets via a small XML loader (`xml.c`) and a custom Tiled module.
  - Collision built from per-tile 4x4 subtile bitmasks; static colliders merged, with “dynamic” tiles (e.g. doors) kept separate.
- Rendering + input via Raylib (kept behind engine modules so it can be swapped; there is also a headless backend).

## Portability notes

The core engine/game code is C99 with a small set of “platform edges” (window/input, timing, filesystem). C is a good fit here because it compiles to native code across most targets, has stable toolchains, and keeps dependencies explicit.

In practice, “runs anywhere with a C compiler” means:
- the ECS/gameplay/physics logic is largely portable as-is
- the platform layer may need tweaks per target (windowing backend, filesystem paths, build flags)

## Third-party

- Raylib (rendering/window/input)
- Chipmunk2D (physics)
- xml.c (TMX parsing helper)
