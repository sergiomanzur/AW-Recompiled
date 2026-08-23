# Native Input & Pointer Navigation Design

Date: 2026-08-23

Spec 1 of 3. Delivers working native mouse control and the platform-neutral input
core that Spec 2 (SDL3 platform migration) and Spec 3 (touch overlay UI, Android)
build on without modification.

## Findings That Shape This Design

### The `ketsuban/advancewars` decomp contains no game logic

Inventory of the repository at commit `c5ecbd5`:

| Item | Contents |
|---|---|
| `src/main.c` | 186 lines: interrupt enable/disable, a sine table lookup, a slot allocator |
| Named functions | 23, all in `0x0807Axxx`-`0x0807Bxxx` — the AGB/IS library init region |
| `asm/libraries.s` | 4 MB of the stock AGB library (sound driver, `LZ77UnComp`, `Div`, flash drivers) |
| `asm/main.s` | 70 KB covering only the same library region |
| Game logic | None. `grep -i 'cursor\|camera\|scroll'` across the whole repo returns 0 hits |
| Target ROM | `GAME_REVISION := 0`, SHA-1 `D0A0A4CFE9B95AC7118F7EF476F014CA0242EB65` |

This project targets Rev 1 (SHA-1 `15053499D5B3F49128A941D7F2D84876F5424D0C`), a
different binary. There is no cursor variable, map struct, or menu code available
to lift from the decomp. It cannot contribute to input handling.

### mGBA cannot be removed by this work

`runtime/src/main.cpp` runs the game entirely through `aw_mgba_run_frame`. The
static recompiler (`runtime/src/cpu_interpreter.cpp`, `scripts/generate-blocks.py`)
is not in the game loop; its own regression test asserts that it halts with
`Stopped at unresolved target 0x08014C31`. With no game logic in the decomp there
is nothing to link against instead of the emulator.

This design therefore treats mGBA as a **replaceable backend** behind an
interface (`ProbeBackend`, below) rather than attempting to remove it. When the
static recompiler matures it implements the same interface and the input stack is
unaffected.

### The current mouse implementation has two structural defects

1. **Runtime address guessing.** `runtime/src/mouse_cursor.cpp` scans 256 KB of
   EWRAM one byte at a time through `core->rawRead8` (262,144 virtual calls per
   snapshot, twice per frame while scanning), then selects candidates with the
   heuristic `val < 40` and marks them `validated` with no verification.

2. **Open-loop steering.** `runtime/src/window.cpp` tracks `cur_grid_x_` /
   `cur_grid_y_` as a local guess at the game's cursor position and advances it on
   every emitted D-pad pulse. The game drops inputs during animations and screen
   transitions, so the model desyncs and never recovers. Commits `47d0245`,
   `8a522ef`, `e7d6130`, `725d815`, `310d044` and `1d58783` are successive
   attempts to tune around this; `eef3087` disabled the feature.

### Dead code in the working tree

`include/aw/` and the untracked files under `runtime/` (`game_state.h`, `proc.h`,
`cursor.cpp`, `map.cpp`, `oam.cpp`, `math.cpp`, `proc.cpp`, `decomp.cpp`) are not
derived from any decomp. Struct offsets are guesses — `game_state.h` annotates
itself "approx. 16-24 bytes in GBA RAM" — and `proc.h` is the Fire Emblem GBA
proc engine, unverified for Advance Wars. Functionally they are inert:
`gMapCursor`, `gCamera`, `gMap` and `gUnits` are standalone native globals while
the real game state lives in mGBA's emulated memory. Nothing reads them.

## Goal

Hovering the mouse over the game's UI moves the game's own selection to what is
under the pointer, in all four contexts (map view, action/list menus, name entry
letter grid, and title/CO/map-select front-end screens). Left click acts as A,
right click as B.

The implementation must be platform-neutral wherever it is not strictly a
platform binding, so that SDL3, Android touch, and handheld gamepads reuse it.

## Non-Goals

- Removing mGBA (see findings).
- Writing to game memory. This design is read-only with respect to game state.
- Rendering a touch-control overlay (Spec 3).
- Replacing the Win32 window, renderer, menu bar, or dialogs (Spec 2).
- Reproducing the game's own cursor/camera logic natively.

## Core Insight

All four contexts are the same problem: the game draws a **selection indicator**
and D-pad presses move it between discrete positions. Map cursor, menu arrow,
letter highlight and CO carousel are all instances of this.

GBA OAM stores sprite position in screen coordinates (`attr0` bits 0-7 = Y,
`attr1` bits 0-8 = X). The mouse also reports screen coordinates. So if the
indicator is tracked through OAM, steering becomes a 2D error vector between two
points in the same coordinate space, and requires **no per-context geometry** —
no tile-size tables, no letter-grid layout, no menu-entry heights. This is what
keeps four contexts from becoming four separate mining efforts.

Camera scroll is read from hardware, not mined: `REG_BG0HOFS`-`REG_BG3VOFS` at
`0x04000010`-`0x0400001E`. World position is therefore
`sprite_screen_pos + bg_scroll`, available with zero archaeology.

A consequence worth stating because it is desirable rather than accidental: with
screen-space targeting, holding the pointer near a viewport edge keeps the cursor
stepping outward, which makes the game scroll the camera. This yields RTS-style
edge-scroll panning as a property of the design.

The only knowledge that must be mined is **which context is active**, used to
select the indicator's sprite signature and to decide whether to steer at all.

## Architecture

```
mouse / touch / gamepad
        │
        ▼
   InputSource(s)  ──────────────►  InputFrame { u16 gba_keys, PointerState[], axes }
   (platform)                            │
                                         ▼
                                    PointerNav  ◄──────── GameProbe
                                         │                    │
                                         │        ┌───────────┴───────────┐
                                         │   OamTracker            ContextProbe
                                         │   (indicator pos)       (ContextId)
                                         │        │                    │
                                         │        └──── ProbeBackend ──┘
                                         │             (mGBA today)
                                         ▼
                                  u16 gba_keys ──► mGBA setKeys
                                         │
                                         └──── observed next frame ──► GameProbe
```

### Modules

| Path | Role | Neutral |
|---|---|---|
| `runtime/include/aw/input/input_frame.hpp` | `InputFrame`: `u16 gba_keys`, `PointerState pointers[]`, analog axes. The single type all platforms produce. | yes |
| `runtime/include/aw/input/input_source.hpp` | `class InputSource { virtual void poll(InputFrame&) = 0; }`. Sources OR their contributions together. | yes |
| `runtime/src/input/source_win32.cpp` | Keyboard (`GetAsyncKeyState`), mouse, XInput → `InputFrame`. Deleted in Spec 2. | no |
| `runtime/include/aw/probe/backend.hpp` | `ProbeBackend`: `read_oam`, `read_io16`, `read_ewram`. Decouples the probe from mGBA. | yes |
| `runtime/src/probe/backend_mgba.cpp` | `ProbeBackend` over `mCore::getMemoryBlock` and `rawRead16`. | no |
| `runtime/src/probe/oam_tracker.cpp` | OAM block → indicator screen position for the active context. | yes |
| `runtime/src/probe/context_probe.cpp` | Mined EWRAM byte(s) → `ContextId`. | yes |
| `runtime/src/nav/pointer_nav.cpp` | The steering loop. Pure function of (pointer, indicator, scroll, context) → keys. | yes |
| `tools/symbol_miner/` | Offline: savestate + scripted input → `data/symbols/<sha1>.json`. | yes |

`PointerNav` being a pure function is load-bearing: it is unit-tested against
synthetic indicator positions with no ROM, no window and no emulator. The current
implementation's untestability is why it is broken.

### `ProbeBackend`

```c++
struct ProbeBackend {
  virtual ~ProbeBackend() = default;
  // Returns a pointer to the live 1 KB OAM block, or nullptr.
  virtual const u8* oam() = 0;
  // Reads a 16-bit IO register at an absolute GBA address (0x04000000 range).
  virtual u16 read_io16(u32 addr) = 0;
  // Returns a pointer to the live EWRAM block and its size, or nullptr.
  virtual const u8* ewram(std::size_t& size_out) = 0;
};
```

`mCore::getMemoryBlock` (declared at `third-party/mgba/include/mgba/core/core.h:144`)
supplies direct pointers, so OAM and EWRAM access become a pointer dereference or
`memcmp` rather than per-byte virtual calls. This replaces the 262,144-call scan
loop in the current implementation.

## Steering Loop

State per axis (X and Y are independent):

```
enum class AxisState { Idle, Pressing, Releasing, Blocked };
```

Rules:

1. **Press until observed motion.** Hold the direction while the indicator's
   *world* position is unchanged. When motion is observed, release for exactly one
   frame before the next press. The game requires key-release between discrete
   moves; commit `e7d6130` discovered this and encoded it as a fixed timer. Here it
   is event-driven instead.

2. **Blocked-axis detection.** If `kBlockedFrames` consecutive frames of pressing
   produce neither indicator motion nor a change in BG scroll, the axis is at a
   boundary or the game is animation-locked. Enter `Blocked` and stop emitting on
   that axis until the target changes or motion resumes. This eliminates the input
   spam that makes the current implementation feel unresponsive.

3. **Deadband.** Stop when the error on an axis is within `kSnapRadius` pixels.
   Prevents oscillation around a target that the discrete grid cannot land on
   exactly.

Initial constant values, to be confirmed against the running game during
implementation rather than left open:

| Constant | Value | Rationale |
|---|---|---|
| `kBlockedFrames` | 8 | ~130 ms; longer than a dropped-input hiccup, shorter than a noticeable stall |
| `kSnapRadius` | 8 px | Half a 16 px tile, so the nearest cell always wins |
| `kReleaseFrames` | 1 | Minimum gap the game accepts between discrete moves |

4. **Read-only.** No writes to game memory. The game runs its own cursor logic,
   camera panning, sprite updates and sound effects, so no state can desync.
   Worst case traversal is one step per two frames — roughly 0.5 s across a full
   screen; typically faster because a step completes as soon as motion is seen.

5. **Arming.** Pointer steering arms on pointer *motion* and disarms on any
   keyboard or gamepad D-pad input, re-arming on the next motion. The current
   implementation steers whenever a mouse rests over a tile, which fights
   controller input.

6. **Clicks.** Left button edge → `KEY_A` for one frame. Right button edge →
   `KEY_B`. Clicks are emitted regardless of context, so an unsupported or
   unrecognised context degrades to a usable click-only mode rather than to
   nothing.

Motion is evaluated in world space (`indicator_screen + bg_scroll`) so that a
camera scroll counts as the game having responded. Evaluating in screen space
would misread edge scrolling as a blocked axis.

## Contexts

```c++
enum class ContextId : u8 {
  Unknown, MapView, ListMenu, NameEntry, FrontEnd, Cutscene,
};
```

Per-context data, held in the mined symbol file rather than in code:

- Indicator sprite signature (OAM `attr2` tile id and palette bank, plus size
  bits) used by `OamTracker` to locate the indicator among up to 128 entries.
- Which BG layer's scroll registers track content movement, if any.
- Whether steering is enabled (`Cutscene` and `Unknown`: clicks only).

`OamTracker` resolves the indicator by signature match, and falls back to
*correlation*: the entry whose position changed in the direction of the D-pad
input emitted on the previous frame. Correlation also validates a signature at
runtime, so a stale symbol file degrades rather than misbehaves.

## Symbol Mining

`data/symbols/<rom-sha1>.ini` holds the mined data, keyed by ROM SHA-1 so a
different revision cannot silently load the wrong offsets. Absent or mismatched,
the runtime falls back to correlation-only tracking, which still steers — the
symbol table is a refinement (faster lock-on, and suppressing steering during
cutscenes), not a prerequisite.

INI rather than JSON because the project already ships `aw::ConfigFile` for
`config.ini` and has no JSON dependency; adding one for a single small data file
is not worth it. Schema in `data/symbols/README.md`.

The miner is offline and deterministic:

1. A runtime hotkey dumps a labeled savestate while playing, so each context can
   be captured without scripting a path from boot.
2. `tools/symbol_miner` loads a savestate, applies a scripted input sequence,
   diffs EWRAM through `ProbeBackend::ewram` with `memcmp`, and ranks candidate
   context bytes by how cleanly they partition the observed screens.
3. Output is reviewed and committed as JSON. Savestates stay local and gitignored
   (they contain ROM-derived data).

## Error Handling

| Condition | Behaviour |
|---|---|
| No symbol file, or ROM SHA-1 mismatch | Correlation-only tracking; steering still enabled; clicks still work |
| `ContextId::Unknown` | Correlation-only tracking; steering still enabled |
| Indicator not found in OAM | Treated as `Unknown` for that frame; no steering, no crash |
| Pointer outside game viewport | Steering disarmed; click edges still tracked so a release outside is not lost |
| `getMemoryBlock` unavailable | `ProbeBackend` reports unavailable; whole pointer-nav feature disables itself and logs once |

## Testing

Unit tests, no ROM or emulator required:

- `pointer_nav_tests` — synthetic indicator/scroll sequences: converges to
  target; respects deadband; enters `Blocked` after a stuck axis; releases for one
  frame between steps; disarms on D-pad input; emits A/B on click edges. This is
  the regression suite for every bug in the `47d0245`..`1d58783` commit range.
- `oam_tracker_tests` — synthetic 1 KB OAM buffers: signature match, correlation
  fallback, indicator-absent.
- `input_frame_tests` — multiple sources OR together correctly.

Integration, ROM required, following the existing `--frames N` harness pattern:

- Headless run from a savestate asserting that a scripted pointer target is
  reached within a frame budget, per context.

## Deletions

Removed: `include/aw/` (all), `runtime/include/aw/game_state.h`,
`runtime/include/aw/proc.h`, `runtime/include/aw/types.h`,
`runtime/include/aw/mouse_cursor.hpp`, `runtime/src/cursor.cpp`,
`runtime/src/map.cpp`, `runtime/src/oam.cpp`, `runtime/src/math.cpp`,
`runtime/src/proc.cpp`, `runtime/src/decomp.cpp`,
`runtime/src/mouse_cursor.cpp`, and their `runtime/CMakeLists.txt` entries.

Nothing is salvaged from the deleted set. `runtime/include/aw/hardware.hpp`
already defines the canonical `aw::kKeyA`..`aw::kKeyL` masks that the runtime
uses, so `types.h`'s parallel `KEY_*` macros are redundant, and its GBA integer
typedefs are replaced by `<cstdint>` in the new modules.

`runtime/include/aw/window.hpp` is the only file outside the deleted set that
depends on it (`#include "aw/mouse_cursor.hpp"`). It loses that include, the
`MouseCursor mouse_cursor_` member, `set_mgba_core`, `mouse_cursor()`, and the
`cur_grid_*` / `mouse_step_timer_` / `mouse_*_was_down_` / `mouse_grid_init_`
members. `runtime/src/main.cpp` correspondingly stops calling
`window.set_mgba_core()` and `window.mouse_cursor().reset()`, passing the core to
the probe layer instead.

Input handling is removed from `runtime/src/window.cpp`, which keeps window
creation, the menu bar, dialogs and rendering. `Window::client_to_gba` moves to
the input layer as the viewport transform, and remains covered by
`window_aspect_tests`. `InputMapping` in `runtime/include/aw/input_config.hpp`
moves under `aw/input/` unchanged, so existing `config.ini` key/pad bindings and
the remap dialog keep working.

## Risks

**The indicator may not be an OAM sprite in every context.** Very likely for the
map cursor; less certain for menu highlights, which may be background tiles. The
first implementation task is a verification pass across all four contexts using a
throwaway OAM dump tool. Where a context proves BG-based, the fallback is
BG-tilemap diffing behind the same `ProbeBackend` interface — more work, but the
architecture and the steering loop are unchanged because both sources produce a
screen-space indicator position.

**Context detection may need more than one byte.** The miner ranks candidates and
the schema permits a list of (address, value) predicates per context, so this
costs mining effort rather than a redesign.

## Interaction With Later Specs

Spec 2 replaces `source_win32.cpp` with an SDL3 source and moves window/rendering
to SDL3, retaining the Win32 menu bar and dialogs through the `HWND` that SDL3
exposes (`SDL_GetWindowProperties`, `SDL_SetWindowsMessageHook`) rather than
rewriting them. Spec 3 adds a touch-overlay `InputSource` that hit-tests its own
rects. Neither touches `pointer_nav`, `oam_tracker`, `context_probe` or the
symbol format.
