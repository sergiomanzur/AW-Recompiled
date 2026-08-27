# AW-Recompiled ⚔️

[![Version](https://img.shields.io/badge/release-v0.1--beta-brightgreen.svg)](https://github.com/sergiomanzur/AW-Recompiled/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%20(x64)-blue.svg)](https://github.com/sergiomanzur/AW-Recompiled/releases)
[![Language](https://img.shields.io/badge/language-C%2B%2B20%20%2F%20C-00599C.svg)](https://github.com/sergiomanzur/AW-Recompiled)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

**AW-Recompiled** is a high-performance native desktop port and C++ static recompilation of *Advance Wars* (Game Boy Advance). Built as an **AI-assisted passion project**, it translates original GBA ARM/Thumb machine code and hardware calls into native instructions. The result is zero-latency 60 FPS gameplay, accurate software rendering, instant rewind mechanics, and modern quality-of-life additions without traditional emulation overhead.

---

## 🌟 Quick Start (No Building Required!)

1. Download the pre-built **`advance-wars-v0.1b-windows.zip`** from the [Latest Release](https://github.com/sergiomanzur/AW-Recompiled/releases).
2. Extract the archive to any folder.
3. Launch `advance-wars-native.exe`.
4. **First Launch ROM Prompt**: On your first launch, the executable will automatically pop up a native Windows file dialog asking you to select your legally acquired *Advance Wars (USA)* GBA ROM. Once selected, your path is saved to `config.ini` for all future launches!

---

## ✨ Features (v0.1 Beta Release)

- 🎮 **Native Desktop Engine & Dual Execution Backends**:
  - **mGBA Core Bridge (Default)**: Embedded high-precision GBA execution pipeline.
  - **`gba-recomp` Static Recompilation**: Direct native ARM/Thumb translation layer running compiled GBA machine code as a native library (`AW_BACKEND=recomp`).
- 🖱️ **Mouse & Pointer Navigation**: Direct point-and-click map navigation, unit selection, and command execution alongside classic gamepad/keyboard controls.
- ↩️ **Undo Last Order (`Ctrl+Z` / `L3`)**: Single-press instant restoration to the exact frame prior to your last confirmed movement or attack order.
- ⏪ **Instant Rewind (Time Travel)**: Hold `Backspace` (or `Y`/`LT` on controller) to seamlessly step backward through up to 5 seconds of in-RAM gameplay history.
- ⚡ **Zero-Latency Fast-Forward**: Hold `Tab` (or `X`/`RT` on controller) to accelerate AI turns at hundreds of FPS with pitch-correct 1x audio restoration upon release.
- 📊 **Tactical Sidebar Telemetry**: Integrated widescreen telemetry panel (`F4` toggle) presenting live cursor tile reads, playback mode, undo depth, and emulation stats.
- 🎬 **Input Replays & Speedrun Overlay**:
  - `F6` records frame-accurate input logs to lightweight `.awr` files (zero ROM data included).
  - Play replays back via **File → Play Replay**.
  - `F8` toggles an on-screen speedrun input display and frame counter.
- 🎲 **Randomizer Hooks & Live IPS Patching**:
  - Apply community IPS patches on the fly via **File → Apply IPS Patch** or `--ips` command-line argument.
  - Boot-time RAM write injection hooks configured under `[Randomize]` in `config.ini`.
- ✍️ **HD Text Replacement**: Custom 16×16 font rendering engine with palette-independent ink-hash tile matching.
- 🧪 **Per-Frame Cheat Engine**: Armed RAM cheat code injection (`[Cheats]` in `config.ini`) evaluated continuously every frame.
- 🖼️ **Framebuffer Pre-Scaling**: Native resolution pre-scaling (4×/6×/9× for 720p/1080p/4K display output) with nearest-neighbor clarity.
- 🔊 **Stereo Audio Engine**: 16-bit PCM stereo audio via a low-latency Windows `waveOut` audio pipeline.

---

## 🎮 Controls

| GBA Button | Keyboard Key | Xbox / XInput Controller |
| :--- | :--- | :--- |
| **D-Pad Up / Down / Left / Right** | Arrow Keys (`Up` / `Down` / `Left` / `Right`) | D-Pad / Left Analog Stick |
| **Button A** | `Z` / `Space` | `A` Button |
| **Button B** | `X` | `B` Button |
| **Start** | `Enter` | `Start` / Menu Button |
| **Select** | `Shift` | `Back` / View Button |
| **L Shoulder** | `Q` | `LB` Trigger |
| **R Shoulder** | `E` | `RB` Trigger |

### ⏪ Hotkeys & Telemetry

| Action | Key / Input |
| :--- | :--- |
| **Undo Last Order** | `Ctrl+Z` / Left Stick Click (`L3`) |
| **Instant Rewind (Hold)** | `Backspace` / `Y` / `LT` |
| **Fast-Forward (Hold)** | `Tab` / `X` / `RT` |
| **Toggle Tactical Sidebar** | `F4` |
| **Save State / Load State** | `F5` / `F9` |
| **Record Replay / Stop** | `F6` / `F7` |
| **Toggle Speedrun Key Display** | `F8` |

---

## 📜 ROM Policy & First Launch

This repository and its binary releases do **NOT** contain any copyrighted ROM files, game code, or proprietary graphics/audio assets.

- **Required ROM**: *Advance Wars (USA) (Rev 1)* GBA ROM.
- **Expected SHA-1 Hash**: `15053499D5B3F49128A941D7F2D84876F5424D0C`
- **First-Launch Prompt**: When launching `advance-wars-native.exe` for the first time, a Windows dialog will ask you to select your GBA ROM file. Your selection is automatically recorded in `config.ini`.

---

## 🤝 External Repositories & Acknowledgments

AW-Recompiled stands on the shoulders of these incredible open-source projects:

- **[mGBA](https://github.com/mgba-emu/mgba)** by endrift & mGBA contributors — Provides the high-precision GBA emulation core bridge, PPU timing, and hardware memory map interface (`third-party/mgba`).
- **[gba-recomp](https://github.com/JRickey/gba-recomp)** by JRickey — The static recompilation framework that translates ARM/Thumb GBA machine code into native executable code blocks.

---

## 🛠️ Building from Source

### Prerequisites
- **CMake** (v3.25 or newer)
- **C++20 Compiler** (MSVC 2022, Clang, or GCC)
- **Ninja** or Visual Studio build tools
- **Python** (3.10+)

### Build Instructions (Windows - mGBA Default Backend)

```powershell
# Configure build output
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build native executable
cmake --build build/native --target advance-wars-native
```

The resulting executable will be located at:
`build\native\runtime\advance-wars-native.exe`

---

## 🚀 Future Roadmap

We are actively expanding platform support! Look out for future release builds targeting:

- 📱 **Android**: Touch-native interface, mobile renderer, and low-latency audio.
- 🎮 **SteamOS**: Native Linux binaries optimized for Steam Deck controls and performance profiles.
- 🍎 **macOS**: Universal native builds for Apple Silicon (M-series) and Intel Macs.

---

## 👐 Open Source & Community Contributions

AW-Recompiled is 100% **free and open source**.

- **Forks & Pull Requests**: All contributions, bug fixes, feature requests, and forks are warmly welcomed! Feel free to submit PRs or open issues.
- **License**: Distributed under the permissive [MIT License](LICENSE).

---

*Advance Wars is a registered trademark of Nintendo / Intelligent Systems. This project is an independent open-source recreation and is not affiliated with or endorsed by Nintendo.*
