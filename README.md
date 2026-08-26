# AW-Recompiled ⚔️

[![Version](https://img.shields.io/badge/version-v0.1--alpha-orange.svg)](https://github.com/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-blue.svg)](https://github.com/)
[![Language](https://img.shields.io/badge/language-C%2B%2B20%20%2F%20C-00599C.svg)](https://github.com/)

**AW-Recompiled** is a native C++ static recompilation and port of *Advance Wars* (Game Boy Advance). By compiling GBA ARM/Thumb binary code directly into native instructions, it provides high-performance native execution, accurate software rendering, and real-time audio playback without traditional emulation overhead.

---

## ✨ Features (v0.1 Alpha)

- 🎮 **Fully Playable Native Engine**: Native executable rendering at 60 FPS with hardware input mapping — and now selectable between two execution backends: the mGBA core bridge (default) and the **gba-recomp static recompilation backend** (`AW_BACKEND=recomp`), where the game's own code runs as a native DLL and the full feature surface works unchanged.
- ↩️ **Undo Last Order**: `Ctrl+Z` (or left-stick click) instantly restores the exact moment before your last confirmed move or attack — snapshots are taken at the confirming button press while you are commanding the map.
- 📊 **Tactical Sidebar**: In widescreen aspect ratios the game keeps its pixel-perfect 3:2 frame while a native console panel fills the extra space — playback mode, cursor tile, undo/rewind depth, and emulation telemetry (`F4` to toggle). Only verified game-state reads are shown; nothing is invented.
- ⏪ **Instant Turn Rewind (Time Travel)**: Hold `Backspace` to rewind through an in-RAM savestate ring (5-second window by default) — no savestate files, no reloads.
- ⚡ **Zero-Latency Fast-Forward**: Hold `Tab` (or a controller trigger) to blast through AI turns at hundreds of FPS with clean, pitch-correct 1x audio on release.
- 🎬 **Replays + Input Display**: `F6` records a run from power-on to a tiny shareable `.awr` file (inputs only — no ROM data); play them back via **File → Play Replay**, even fast-forwarded. `F8` toggles the speedrun-style frame counter + button display over the game.
- 🎲 **Randomizer Hooks**: Apply any community IPS randomizer patch via **File → Apply IPS Patch** (or `--ips`); the patched game runs from a temp file, no patched-ROM copies to manage. Mined RAM addresses can also be tweaked at boot via `[Randomize]` in config.
- ✍️ **HD Text Replacement**: An ink-hash tile pack (`aw-hd-capture` builds the starter) swaps the 8×8 GBA font for 16×16 artist tiles at render time — crisp dialogue at any window size, palette-independent matching.
- 🧪 **Cheats**: RAM-write codes (`[Cheats]` in `config.ini`, `address:width:value` decimal or hex) applied every frame so the game can't fight them off; toggle live from the Settings menu, active count shown in the sidebar.
- 🖥️ **Internal Resolution Pre-Scaling**: the Internal Resolution setting now genuinely pre-scales the framebuffer (4×/6×/9× for 720p/1080p/4K) with nearest-neighbour before the window stretch.
- 🔊 **Full Audio Backend**: 16-bit stereo PCM audio synthesis via integrated mGBA core bridge and low-latency Windows `waveOut` audio pipeline.
- 🖼️ **Software & Windowed Renderer**: High-performance pixel pipeline supporting standard GBA display modes, tile layers, and sprite rendering.
- 🛠️ **Cross-Platform Toolchain**: Built with CMake & Ninja, supporting Clang, GCC, and MSVC.
- 🔍 **Instruction & Block Tracing**: Diagnostic disassembly and instruction trace tools built into the runtime.

---

## 🎮 Controls

| GBA Button | Keyboard Key |
| :--- | :--- |
| **D-Pad Up / Down / Left / Right** | Arrow Keys (`Up` / `Down` / `Left` / `Right`) |
| **Button A** | `Z` / `Space` |
| **Button B** | `X` |
| **Start** | `Enter` |
| **Select** | `Shift` |
| **L Shoulder** | `Q` |
| **R Shoulder** | `E` |

### ⏪ Time Travel & Fast-Forward

| Action | Keyboard | XInput Controller |
| :--- | :--- | :--- |
| **Undo Last Order** (one press = one order undone) | `Ctrl+Z` | Left stick click (`L3`) |
| **Instant Rewind** (hold to step back ~1/3 s per step, release to resume) | `Backspace` (hold) | `Y` or `LT` (hold) |
| **Fast-Forward** (hold for max-speed emulation, release for 1x) | `Tab` (hold) | `X` or `RT` (hold) |
| **Record Replay** (starts from power-on) / finish | `F6` | — |
| **Stop Replay Playback** | `F7` | — |
| **Input Display + Frame Counter** | `F8` | — |

- Rewind keeps an in-RAM savestate ring (up to **5 seconds** of history by default; no disk I/O, no savestate slots touched). Holding rewinds through your last moves, attacks, or combat animations; releasing resumes play from that point. Snapshots older than the window are evicted automatically, so you can never rewind further back than the limit.
- Fast-forward mutes timeline audio while held (no pitch distortion, no drifting sync) and returns to clean 1x playback on release. Both modes show their state in the window title, and are also available from the **File** menu.
- Tunable in `config.ini` under `[Rewind]`: `enabled`, `snapshot_interval` (frames between snapshots), `capacity` (ring size), `max_seconds` (rewind window, default 5).

### 🎬 Replays

- `F6` starts recording: the console power-cycles so the file replays from frame 0, then every fed input is logged (2 bytes per frame — about 430 KB per hour). The ROM's SHA-1 travels in the header, and playback refuses a mismatched revision instead of silently desyncing. Files contain no ROM data, so they're safe to share.
- Play back with **File → Play Replay…** (or `--replay run.awr`); `Tab` fast-forwards a replay just like live play, and `F7` stops it. Loading savestates during playback is refused to protect the input stream.
- `F8` toggles the on-game status strip: emulated frame counter plus every held GBA button (speedrun/capture friendly), with REC/PLAY markers while a replay is active.

### 🎲 Randomizing

- **File → Apply IPS Patch…** stages any IPS patch (the format community AW randomizers ship) and reboots with it applied; the choice persists in `config.ini` under `[Patches]`. The patched image lives in a temp file while playing — clear the setting and reboot to return to vanilla. `--ips patch.ips` does the same from the command line.
- Mined RAM addresses can be written once at boot via `[Randomize]` (`apply_frame`, plus `write1 = address:value` decimal slots) — see `data/symbols/README.md` for the mining workflow.

### ✍️ HD text packs

- Run `aw-hd-capture "<rom>" [state.ss] --mash` to dump every distinct 8×8 framebuffer block as an identity-upscaled starter pack under `data/hd/tiles/`. Redraw any `_16.bmp` by hand (dark pixels = ink), enable **Settings → HD Text Replacement**, and exactly those glyphs render at 2x with the game's own palette. Matching is by ink shape, not colour, so one tile covers the glyph everywhere it appears.

---

## 🛠️ Building & Running

### Prerequisites
- **CMake** (v3.20 or newer)
- **C++20 Compiler** (Clang / MSVC / GCC)
- **Ninja** or Visual Studio build system
- **Python** (3.10+)

### Building from Source

```powershell
# Configure build directory
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build native runtime executable
cmake --build build/native --target advance-wars-native
```

The compiled executable will be located at:
`build/native/runtime/advance-wars-native.exe`

### Running the Game

Provide the path to a legally obtained dump of *Advance Wars (USA) (Rev 1)*:

```powershell
.\build\native\runtime\advance-wars-native.exe "rom\Advance Wars (USA) (Rev 1).gba"
```

---

## 📜 ROM Policy

This repository does **not** contain any copyrighted ROM files or game assets. Users must provide their own legally acquired dump of the original game cartridge:

- **Target ROM**: *Advance Wars (USA) (Rev 1)*
- **Expected SHA-1**: `15053499D5B3F49128A941D7F2D84876F5424D0C`

---

## 📁 Repository Structure

```
├── runtime/       # Core native C++ engine, audio system, and windowing
├── config/        # Recompilation maps and disassembly definitions
├── docs/          # Architecture documentation and notes
├── scripts/       # Tooling and verification scripts
├── third-party/   # Submodules & libraries (libmgba, etc.)
└── tools/         # Recompilation generators and utility scripts
```

---

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.
