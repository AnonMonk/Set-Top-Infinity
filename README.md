# Set-Top-Infinity

OpenGL demoscene intro in C++ — classic effects (rotozoom, tunnel, twister, starfield, Julia set, credits/greets), timed at **60 FPS** and **1280×720** (fullscreen).

Originally aimed at **Apple TV / set-top** style targets (lean Julia precalc, optional CPU throttling for testing). Builds and runs natively on **macOS**, **Linux**, and **Windows** (MinGW).

Full demo length: **about 1:30** (90 seconds).

---

## Scenes

| # | Name        | Duration | Description |
|---|-------------|----------|-------------|
| 1 | `logo`      | 3 s      | Logo intro (fish head/tail) |
| 2 | `wave`      | 2 s      | Transition logo → rotozoom |
| 3 | `rotozoom`  | 8 s      | Rotozoomer (`Testbild.png`) |
| 4 | `tunnel`    | 10 s     | Tunnel (`bdl.png`) |
| 5 | `twister`   | 15 s     | Twister |
| 6 | `starfield` | 20 s     | Starfield |
| 7 | `julia`     | 14 s     | Julia zoom (chunked precalc during earlier scenes) |
| 8 | `credits`   | 9 s      | Credits |
| 9 | `greets`    | 9 s      | Greets |

Julia levels are **not** loaded from PNGs; they are computed at runtime (60 levels @ 512×288, per-frame budget by scene — see `EffectJulia.h`).

---

## Requirements

- **C++17** compiler (`g++`)
- **OpenGL**
- Bundled libraries in the project root:
  - `vipgfx` — window, input, PNG, timing (`vipgfx.h`)
  - `glTTF` — TrueType text (`glTTF.h`)
- **Audio player** (platform-dependent; see below)
- Assets under `assets/` (must be reachable relative to the binary)

### Platform libraries

| Platform | Libraries in repo | Link flags (Makefile) |
|----------|-------------------|------------------------|
| macOS    | `libvipgfx.dylib`, `libglTTF.dylib` | `-framework OpenGL -lvipgfx -lglTTF` |
| Linux    | `libvipgfx.so`, `libglTTF.so`       | `-lGL -lvipgfx -lglTTF` |
| Windows  | `vipgfx.dll`, `glTTF.dll` (+ import libs) | `-lopengl32 -lvipgfx -lglTTF -lwinmm` |

DLL/SO/DYLIB files must sit next to the binary (or on the library path) at launch.

---

## Build

```bash
make mac      # macOS → binary: demo
make linux    # Linux  → binary: demo
make win      # Windows (MinGW) → binary: demo.exe
make clean    # remove build artifacts
make info     # print sources / targets
```

Running plain `make` only prints which platform target to use.

---

## Run

```bash
# Full demo
./demo          # macOS / Linux
demo.exe        # Windows

# Solo mode: one scene, looped
./demo tunnel
./demo 4
./demo julia
./demo greetz

# Help
./demo --help
```

### Scene names (CLI)

Numbers `1`–`9` (or `0`–`8`) and e.g.:

`logo` / `intro`, `wave` / `logo_wave`, `roto` / `rotozoom`, `tunnel`, `twister`, `starfield` / `stars`, `julia`, `credits`, `greets` / `greetz`

### Keyboard (dev controls)

| Key     | Action |
|---------|--------|
| `1`–`9` | Jump to scene (or switch solo scene) |
| `R`     | Restart current scene |
| `Space` | Pause / resume (virtual demo clock) |
| `ESC`   | Quit |

---

## Audio

| Phase        | Windows                 | macOS / Linux              | Player |
|--------------|-------------------------|----------------------------|--------|
| Intro        | `assets/Introsound.wav` | `assets/Introsound.aiff`   | Win: `PlaySound`, macOS: `afplay`, Linux: `pw-play` |
| Main music   | `assets/neon.wav`       | `assets/neon.aiff`         | starts after the logo scene |

MP3 variants and `music/Neon Velocity no lyrics.mp3` are also in the repo — the demo itself plays the WAV/AIFF paths under `assets/`.

Audio can be disabled via `#define playaudio` in `main.cpp`.

---

## Assets

| File | Usage |
|------|--------|
| `assets/Kopf_transparent.png` | Logo fish (head) |
| `assets/Schwanz_transparent.png` | Logo fish (tail) |
| `assets/Testbild.png` | Rotozoom texture |
| `assets/bdl.png` | Tunnel texture |
| `assets/corbel.ttf` | Font (credits / logo text) |
| `assets/Introsound.*` / `neon.*` | Audio |

---

## Tools (macOS-oriented)

`tools/` contains helpers for **CPU profiling** and **weak-hardware simulation** (Apple TV approximation) without changing demo code. The logger is a small **C++** binary (not linked into the demo).

```bash
# Build external logger once
make tools
# alternatively: cd tools && make

# ~10% CPU + CSV log under tools/log/
./tools/run_atv_slow.sh
./tools/run_atv_slow.sh 15 julia   # 15%, solo Julia

# Full CPU, log only
./tools/run_with_cpu_log.sh

# Logger manually (PID or wait for process named "demo")
./tools/log_demo_stats <pid>
./tools/log_demo_stats --find-demo
./tools/log_demo_stats <pid> 0.5 tools/log/my.csv
```

The shell wrappers build `tools/log_demo_stats` automatically if missing.

For a more precise limit: `brew install cpulimit`.  
`NO_CPU_LOG=1` disables CSV logging.

---

## Project layout

```
Set-Top-Infinity/
├── main.cpp                 # timeline, CLI, audio, main loop
├── EffectLogo.*             # intro + wave transition
├── EffectRotozoom.*
├── EffectTunnel.*
├── EffectTwister.*
├── EffectStarfield.*
├── EffectJulia.*            # chunked Julia precalc
├── EffectEndTitles.*        # credits + greets
├── vipgfx.h / glTTF.h       # graphics / font API
├── gettime.h
├── Makefile
├── assets/                  # textures, font, audio
├── music/                   # extra music sources
└── tools/                   # CPU logger (C++), ATV slow runner
```

---

## Technical notes

- **Target FPS:** 60 (`DEMO_FPS`)
- **Resolution:** 1280×720, fullscreen (`openGLcontext`)
- **Demo clock:** wall-clock based, pauseable and seekable — scene changes via frame offsets
- **Julia:** precalc runs with a budget during logo … starfield; before drawing the Julia scene it may finish blocking
- **Windows path:** `getExecutableDir` currently returns `"."` on Windows — run the binary from the project root so `assets/` is found

---

## License / origin

Demoscene project (working / earlier name: *demomac*). Use bundled `vipgfx`/`glTTF` binaries and assets according to their respective original licenses.
