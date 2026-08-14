# Set-Top-Infinity

OpenGL demoscene intro in C++ — classic effects (rotozoom, tunnel, twister, Julia set, Amiga Boing Ball, starfield, credits/greets), timed at **60 FPS** and **1280×720** (fullscreen).

Originally aimed at **Apple TV / set-top** style targets (CPU-live Julia, optional CPU throttling for testing). The demo source is compatible with the old compiler on the **first-generation Intel Apple TV** and also builds natively on current **macOS**, **Linux**, and **Windows** (MinGW).

Full demo length: **about 1:41** (101 seconds).

---

## Scenes

| # | Name        | Duration | Description |
|---|-------------|----------|-------------|
| 1 | `logo`      | 3 s      | Logo intro (fish head/tail) |
| 2 | `wave`      | 2 s      | Transition logo → rotozoom |
| 3 | `rotozoom`  | 10 s     | Rotozoomer (`Testbild.png`) |
| 4 | `tunnel`    | 12 s     | Tunnel (`bdl.png`) |
| 5 | `twister`   | 12 s     | Twister |
| 6 | `julia`     | 14 s     | CPU-live Julia zoom |
| 7 | `ball`      | 10 s     | Classic red/white Amiga Boing Ball |
| 8 | `starfield` | 20 s     | Final graphical effect before credits |
| 9 | `credits`   | 9 s      | Credits |
| 10 | `greets`   | 9 s      | Greets |

The Julia is **not** loaded or prepared in stages. Every displayed frame is computed completely at runtime at 384×216. On Intel, four pixels are iterated together with SSE; ARM uses the scalar fallback. Palette lookup tables keep transcendental functions out of the pixel loop, and the finished frame is uploaded as RGB565. The camera moves from the overview to a true preperiodic Julia-boundary point so the deep zoom cannot run into the empty region around the origin. No partially updated frame is ever displayed.

---

## Requirements

- C++98-capable compiler (`g++`) for the demo; the optional tools require C++17
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

The bundled `libglTTF.dylib` contains i386, x86_64, and arm64 code and exports
the real `getTextSize` implementation used to center the end scroller.

DLL/SO/DYLIB files can sit next to the binary. Linux embeds an `$ORIGIN`
runtime search path; the macOS libraries already use `@executable_path`.

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

`make mac` detects the compiler target. Intel builds use `-msse2`; ARM64
builds omit that x86-only option and use the scalar Julia path. On a
first-generation Apple TV, use the same `make mac` command. No C++17 option is
passed to its legacy GCC.

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
./demo ball
./demo greetz

# Help
./demo --help
```

### Scene names (CLI)

Numbers `1`–`10` and e.g.:

`logo` / `intro`, `wave` / `logo_wave`, `roto` / `rotozoom`, `tunnel`, `twister`, `julia`, `ball` / `amigaball` / `boing`, `starfield` / `stars`, `credits`, `greets` / `greetz`

### Keyboard (dev controls)

| Key     | Action |
|---------|--------|
| `1`–`9`, `0` | Jump to scene 1–10 (or switch solo scene) |
| `R`     | Restart current scene |
| `Space` | Pause / resume (virtual demo clock) |
| `ESC`   | Quit |

---

## Audio

| Phase        | Windows                 | macOS / Linux              | Player |
|--------------|-------------------------|----------------------------|--------|
| Intro        | `assets/Introsound.wav` | `assets/Introsound.aiff`   | Win: `PlaySound`, macOS/Apple TV: prepared `NSSound`, Linux: `pw-play` |
| Main music   | `assets/neon.wav`       | `assets/neon.aiff`         | starts after the logo scene; prepared on macOS/Apple TV |

MP3 variants and `music/Neon Velocity no lyrics.mp3` are also in the repo — the demo itself plays the WAV/AIFF paths under `assets/`.

On macOS and the Apple TV, the real first demo frame is presented before any
audible sound starts. Intro and main music are loaded into `NSSound`, started
silently once, paused, and reset to time zero. The demo then resumes the prepared
intro without launching another process. `afplay` remains the error fallback.

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

## Tools

`tools/` contains helpers for **CPU profiling** and **weak-hardware simulation** (Apple TV approximation) without changing demo code. `make tools` builds the platform-appropriate C++ helper.

### Windows

```powershell
mingw32-make tools

# Approximately 10% of one logical CPU core, Julia solo mode
.\tools\run_atv_slow_win.exe 10 julia

# Clock-ratio approximation for a 1 GHz target
.\tools\run_atv_slow_win.exe --mhz 1000 julia

# Try a little more or less CPU
.\tools\run_atv_slow_win.exe 15 julia
.\tools\run_atv_slow_win.exe 5 julia
```

The Windows launcher uses a hard CPU cap, pins the demo to one logical core, and automatically closes the demo if the launcher exits. `--mhz` converts the requested clock to a fraction of the host clock reported by Windows; it cannot reproduce Pentium M IPC, cache behavior, turbo behavior, the GeForce Go 7300 GPU, memory bandwidth, or the old Apple/NVIDIA driver.

### macOS / Linux

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
├── EffectJulia.*            # CPU-live Julia renderer
├── EffectBall.*             # Amiga Boing Ball
├── EffectStarfield.*        # final graphical effect
├── EffectEndTitles.*        # credits + greets
├── vipgfx.h / glTTF.h       # graphics / font API
├── MacSound.*               # small macOS/Apple TV audio preload wrapper
├── gettime.h
├── Makefile
├── assets/                  # textures, font, audio
├── music/                   # extra music sources
└── tools/                   # CPU logger (C++), ATV slow runner
```

---

## Technical notes

- **Target FPS:** 60 (`DEMO_FPS`), explicitly frame-paced even when VSync is disabled
- **Resolution:** 1280×720, fullscreen (`openGLcontext`)
- **Demo clock:** wall-clock based, pauseable and seekable — scene changes via frame offsets
- **Julia:** every frame is calculated live at 384×216, using SSE on Intel and a scalar fallback on ARM, then uploaded once as RGB565
- **Asset path:** macOS, Linux, and Windows resolve `assets/` relative to the executable, independent of the current working directory

---

## License / origin

Demoscene project (working / earlier name: *demomac*). Use bundled `vipgfx`/`glTTF` binaries and assets according to their respective original licenses.
