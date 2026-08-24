# AW-Recompiled ⚔️

[![Version](https://img.shields.io/badge/version-v0.1--alpha-orange.svg)](https://github.com/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-blue.svg)](https://github.com/)
[![Language](https://img.shields.io/badge/language-C%2B%2B20%20%2F%20C-00599C.svg)](https://github.com/)

**AW-Recompiled** is a native C++ static recompilation and port of *Advance Wars* (Game Boy Advance). By compiling GBA ARM/Thumb binary code directly into native instructions, it provides high-performance native execution, accurate software rendering, and real-time audio playback without traditional emulation overhead.

---

## ✨ Features (v0.1 Alpha)

- 🎮 **Fully Playable Native Engine**: Native executable rendering at 60 FPS with hardware input mapping.
- ⏪ **Instant Turn Rewind (Time Travel)**: Hold `Backspace` to rewind moves, attacks, or entire turns through an in-RAM savestate ring — no savestate files, no reloads.
- ⚡ **Zero-Latency Fast-Forward**: Hold `Tab` (or a controller trigger) to blast through AI turns at hundreds of FPS with clean, pitch-correct 1x audio on release.
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
| **Instant Rewind** (hold to step back ~1/3 s per step, release to resume) | `Backspace` (hold) | `Y` or `LT` (hold) |
| **Fast-Forward** (hold for max-speed emulation, release for 1x) | `Tab` (hold) | `X` or `RT` (hold) |

- Rewind keeps an in-RAM savestate ring (~80 s of history by default; no disk I/O, no savestate slots touched). Holding rewinds through your last moves, attacks, or entire turns; releasing resumes play from that point.
- Fast-forward mutes timeline audio while held (no pitch distortion, no drifting sync) and returns to clean 1x playback on release. Both modes show their state in the window title, and are also available from the **File** menu.
- Tunable in `config.ini` under `[Rewind]`: `enabled`, `snapshot_interval` (frames between snapshots), `capacity` (ring size).

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
