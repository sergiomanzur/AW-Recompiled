# Native Input & Pointer Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hovering the mouse over the game's UI moves the game's own selection to whatever is under the pointer, in every UI context, by tracking the game's selection indicator through OAM and steering it with a closed-loop D-pad controller.

**Architecture:** Platform bindings produce a neutral `InputFrame`. A `ProbeBackend` (mGBA today) exposes OAM, EWRAM and IO registers. `OamTracker` locates the selection indicator in screen space; `ContextProbe` says which UI context is active. `PointerNav` is a pure function of (pointer, indicator, scroll) that emits GBA button bits — no game memory is ever written. Everything except `source_win32.cpp` and `backend_mgba.cpp` is platform-neutral so Spec 2 (SDL3) and Spec 3 (Android/touch) reuse it unchanged.

**Tech Stack:** C++20, CMake + Ninja, CTest, Win32 (`GetAsyncKeyState`, XInput), libmgba (`mCore::getMemoryBlock`, `rawRead16`, `mCoreSaveStateNamed`), Python 3 for offline log analysis.

**Spec:** `docs/superpowers/specs/2026-08-23-native-input-mouse-design.md`

## Global Constraints

- **Never write game memory.** Reads only. No `rawWrite8`/`rawWrite16` on game state.
- **Target ROM:** `rom/Advance Wars (USA) (Rev 1).gba`, SHA-1 `15053499D5B3F49128A941D7F2D84876F5424D0C`.
- **Neutral modules must not include Windows or mGBA headers.** `aw/input/input_frame.hpp`, `aw/input/input_source.hpp`, `aw/input/viewport.hpp`, everything under `aw/probe/` except `backend_mgba.*`, and everything under `aw/nav/` include only the C++ standard library.
- **Steering constants** (spec §Steering Loop): `kBlockedFrames = 8`, `kSnapRadius = 8`, `kReleaseFrames = 1`. Tunable, but these are the starting values.
- **Test style:** follow `runtime/tests/hardware_tests.cpp` — a plain `int main()` with a `require_equal` helper that throws, returning 1 on failure. No test framework dependency.
- **Every new test registers in `runtime/CMakeLists.txt`** with `add_executable` + `target_link_libraries(... aw_runtime)` + `add_test`, matching the existing blocks.
- **Do not commit savestates or ROM-derived binaries.** `.gitignore` already covers `*.gba`; add savestate and log patterns as needed.
- **Build command:** `cmake --build build/native --target advance-wars-native`
- **Test command:** `ctest --test-dir build/native --output-on-failure`

## Deviations From The Spec (deliberate, approved under "use the next recommended fix")

1. **Symbol table format is INI, not JSON.** The project has no JSON dependency and already ships `aw::ConfigFile` (INI) used for `config.ini`. Adding a JSON library for one small data file is unjustified. Same schema, INI syntax.
2. **Steering is enabled with no symbol file.** The spec's error table said a missing symbol file disables steering. That would mean the mouse does not work until Task 9 mines symbols. `OamTracker`'s correlation mode needs no symbols at all, so steering is enabled from Task 8 and the symbol table becomes a refinement (faster lock-on, and suppressing steering during cutscenes). The spec has been amended to match.

---

### Task 1: Remove the dead decomp code and the old mouse implementation

Clears the working tree of invented "decomp" files and the runtime RAM-scanning mouse before new code lands, so later tasks build on a green tree. No new behaviour; the mouse simply stops existing until Task 8.

**Files:**
- Delete: `include/aw/cursor.h`, `include/aw/decomp.h`, `include/aw/game_state.h`, `include/aw/map.h`, `include/aw/math.h`, `include/aw/oam.h`, `include/aw/proc.h`, `include/aw/types.h`
- Delete: `runtime/include/aw/game_state.h`, `runtime/include/aw/proc.h`, `runtime/include/aw/types.h`, `runtime/include/aw/mouse_cursor.hpp`
- Delete: `runtime/src/cursor.cpp`, `runtime/src/map.cpp`, `runtime/src/oam.cpp`, `runtime/src/math.cpp`, `runtime/src/proc.cpp`, `runtime/src/decomp.cpp`, `runtime/src/mouse_cursor.cpp`
- Modify: `runtime/CMakeLists.txt` (remove deleted sources from `aw_runtime`)
- Modify: `runtime/include/aw/window.hpp` (remove mouse members)
- Modify: `runtime/src/window.cpp` (remove mouse steering block and `set_mgba_core`)
- Modify: `runtime/src/main.cpp` (remove `set_mgba_core` / `mouse_cursor().reset()` calls)

**Interfaces:**
- Consumes: nothing.
- Produces: a tree where `aw::Window` no longer knows about mice or mGBA cores. `Window::client_to_gba` still exists (Task 2 moves it).

- [ ] **Step 1: Confirm the deletion set is self-contained**

Run:
```bash
grep -rn "aw/types.h\|aw/game_state.h\|aw/proc.h\|aw/mouse_cursor.hpp\|aw/cursor.h\|aw/map.h\|aw/oam.h\|aw/math.h\|aw/decomp.h" runtime/src runtime/include runtime/tests
```

Expected: the only hits outside the files being deleted are `runtime/include/aw/window.hpp:6` (`#include "aw/mouse_cursor.hpp"`) and self-references among the deleted files. If anything else appears, stop and report it — the deletion set is wrong.

- [ ] **Step 2: Delete the files**

```bash
git rm -r --cached include/aw 2>/dev/null; rm -rf include/aw
rm -f runtime/include/aw/game_state.h runtime/include/aw/proc.h \
      runtime/include/aw/types.h runtime/include/aw/mouse_cursor.hpp
rm -f runtime/src/cursor.cpp runtime/src/map.cpp runtime/src/oam.cpp \
      runtime/src/math.cpp runtime/src/proc.cpp runtime/src/decomp.cpp \
      runtime/src/mouse_cursor.cpp
```

- [ ] **Step 3: Remove them from the build**

In `runtime/CMakeLists.txt`, delete these lines from the `add_library(aw_runtime STATIC ...)` list:

```cmake
  src/cursor.cpp
  src/decomp.cpp
  src/map.cpp
  src/math.cpp
  src/oam.cpp
  src/proc.cpp
  src/mouse_cursor.cpp
```

In the same file, remove `${PROJECT_SOURCE_DIR}/include` from `target_include_directories(aw_runtime PUBLIC ...)` — that directory no longer exists.

- [ ] **Step 4: Strip the mouse from `window.hpp`**

Remove the include:

```c++
#include "aw/mouse_cursor.hpp"
```

Remove these public members:

```c++
  // mGBA core pointer for direct memory access (mouse cursor support)
  void set_mgba_core(void* core);
  MouseCursor& mouse_cursor() { return mouse_cursor_; }
```

Remove these private members:

```c++
  // PC Native Touchscreen & Direct Mouse Pointer Navigation (Absolute Target Steering)
  MouseCursor mouse_cursor_;
  int cur_grid_x_ = 0;           // Tracked in-game cursor X tile (0..14)
  int cur_grid_y_ = 0;           // Tracked in-game cursor Y tile (0..9)
  int mouse_step_timer_ = 0;     // Frame timer between steps towards target
  bool mouse_grid_init_ = false; // True once target coordinates are initialized
  bool mouse_left_was_down_ = false;   // Edge detection for left click
  bool mouse_right_was_down_ = false;  // Edge detection for right click
```

- [ ] **Step 5: Strip the mouse from `window.cpp`**

Delete the whole `Window::set_mgba_core` definition:

```c++
void Window::set_mgba_core(void* core) {
  mouse_cursor_.set_core(reinterpret_cast<mCore*>(core));
}
```

In `Window::process_events`, delete everything from the comment

```c++
  // Update tracked grid position when manual D-pad keys are pressed
```

through the line

```c++
  mouse_cursor_.update_passive_scan(hardware.keys_pressed);
```

inclusive. Keep the keyboard block (section 1), the XInput block (section 2), and the closing `return is_open_;`.

- [ ] **Step 6: Strip the mouse from `main.cpp`**

Delete these three lines (they appear at the two places noted):

```c++
    // Give the window access to the mGBA core for direct memory mouse cursor support
    window.set_mgba_core(core);
```

```c++
        // Update the core pointer for mouse cursor support
        window.set_mgba_core(core);
        window.mouse_cursor().reset();
```

- [ ] **Step 7: Build and run the full suite**

Run:
```bash
cmake --build build/native --target advance-wars-native
ctest --test-dir build/native --output-on-failure
```

Expected: build succeeds, all existing tests pass. Keyboard and gamepad input still work; the mouse does nothing.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "refactor: remove invented decomp files and RAM-scanning mouse

The deleted headers and sources were not derived from any decompilation;
struct offsets were guesses and the globals they defined were never read
by anything, since real game state lives in mGBA's emulated memory. The
mouse implementation guessed RAM addresses at runtime and steered
open-loop. Both are replaced by the input/probe/nav stack."
```

---

### Task 2: Neutral input core

The one type every platform produces, the source interface they implement, and the viewport transform lifted out of `Window`.

**Files:**
- Create: `runtime/include/aw/input/input_frame.hpp`
- Create: `runtime/include/aw/input/input_source.hpp`
- Create: `runtime/include/aw/input/viewport.hpp`
- Create: `runtime/src/input/viewport.cpp`
- Create: `runtime/tests/input_core_tests.cpp`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Consumes: `aw::kKeyA`..`aw::kKeyL` from `runtime/include/aw/hardware.hpp`.
- Produces: `aw::PointerState`, `aw::InputFrame`, `aw::kMaxPointers`, `aw::InputSource`, `aw::viewport_to_gba`, `aw::kDpadMask`.

- [ ] **Step 1: Write the failing test**

Create `runtime/tests/input_core_tests.cpp`:

```c++
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/hardware.hpp"
#include "aw/input/input_frame.hpp"
#include "aw/input/viewport.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

void tests_frame_starts_empty() {
  aw::InputFrame frame;
  require_equal(frame.gba_keys, std::uint16_t{0}, "keys start clear");
  require_equal(frame.pointer_count, std::size_t{0}, "no pointers");
  require_equal(frame.primary_pointer() == nullptr, true, "no primary pointer");
}

void tests_frame_clear_resets_everything() {
  aw::InputFrame frame;
  frame.gba_keys = aw::kKeyA;
  frame.device_dpad = aw::kKeyLeft;
  frame.pointer_count = 1;
  frame.pointers[0].kind = aw::PointerKind::Mouse;

  frame.clear();

  require_equal(frame.gba_keys, std::uint16_t{0}, "keys cleared");
  require_equal(frame.device_dpad, std::uint16_t{0}, "device dpad cleared");
  require_equal(frame.pointer_count, std::size_t{0}, "pointer count cleared");
  require_equal(frame.pointers[0].kind == aw::PointerKind::None, true, "pointer reset");
}

void tests_primary_pointer_is_first_active() {
  aw::InputFrame frame;
  frame.pointer_count = 2;
  frame.pointers[0].kind = aw::PointerKind::None;
  frame.pointers[1].kind = aw::PointerKind::Touch;
  frame.pointers[1].gba_x = 77;

  const aw::PointerState* p = frame.primary_pointer();
  require_equal(p != nullptr, true, "found a primary pointer");
  require_equal(p->gba_x, 77, "primary is the active one");
}

void tests_dpad_mask_covers_four_directions() {
  const std::uint16_t expected =
      aw::kKeyUp | aw::kKeyDown | aw::kKeyLeft | aw::kKeyRight;
  require_equal(aw::kDpadMask, expected, "dpad mask");
}

void tests_viewport_maps_corners_and_centre() {
  int gx = 0, gy = 0;

  // Exact 4x scale, no letterboxing: 960x640 viewport at origin.
  require_equal(aw::viewport_to_gba(0, 0, 960, 640, 0, 0, gx, gy), true, "top-left inside");
  require_equal(gx, 0, "top-left x");
  require_equal(gy, 0, "top-left y");

  require_equal(aw::viewport_to_gba(0, 0, 960, 640, 959, 639, gx, gy), true, "bottom-right inside");
  require_equal(gx, 239, "bottom-right x");
  require_equal(gy, 159, "bottom-right y");

  require_equal(aw::viewport_to_gba(0, 0, 960, 640, 480, 320, gx, gy), true, "centre inside");
  require_equal(gx, 120, "centre x");
  require_equal(gy, 80, "centre y");
}

void tests_viewport_rejects_outside_and_offsets() {
  int gx = 0, gy = 0;

  // Letterboxed: viewport starts at x=150.
  require_equal(aw::viewport_to_gba(150, 0, 1620, 1080, 149, 500, gx, gy), false, "left of viewport");
  require_equal(aw::viewport_to_gba(150, 0, 1620, 1080, 1770, 500, gx, gy), false, "right of viewport");
  require_equal(aw::viewport_to_gba(150, 0, 1620, 1080, 500, -1, gx, gy), false, "above viewport");

  require_equal(aw::viewport_to_gba(150, 0, 1620, 1080, 150, 0, gx, gy), true, "viewport origin");
  require_equal(gx, 0, "offset origin x");
  require_equal(gy, 0, "offset origin y");
}

void tests_viewport_rejects_degenerate() {
  int gx = 0, gy = 0;
  require_equal(aw::viewport_to_gba(0, 0, 0, 640, 5, 5, gx, gy), false, "zero width");
  require_equal(aw::viewport_to_gba(0, 0, 960, 0, 5, 5, gx, gy), false, "zero height");
}

}  // namespace

int main() {
  try {
    tests_frame_starts_empty();
    tests_frame_clear_resets_everything();
    tests_primary_pointer_is_first_active();
    tests_dpad_mask_covers_four_directions();
    tests_viewport_maps_corners_and_centre();
    tests_viewport_rejects_outside_and_offsets();
    tests_viewport_rejects_degenerate();
    std::cout << "input_core_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
```

- [ ] **Step 2: Register the test and run it to verify it fails**

Append to `runtime/CMakeLists.txt`, immediately before the `add_executable(advance-wars-native ...)` block:

```cmake
add_executable(input_core_tests
  tests/input_core_tests.cpp
)

target_include_directories(input_core_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(input_core_tests PRIVATE aw_runtime)

add_test(NAME input_core_tests COMMAND input_core_tests)
```

Run: `cmake --build build/native --target input_core_tests`
Expected: FAIL — `aw/input/input_frame.hpp: No such file or directory`

- [ ] **Step 3: Write `input_frame.hpp`**

Create `runtime/include/aw/input/input_frame.hpp`:

```c++
#pragma once

#include "aw/hardware.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace aw {

// The four D-pad bits, as a mask. Pointer navigation disarms when any of
// these arrives from a physical device.
constexpr std::uint16_t kDpadMask = kKeyUp | kKeyDown | kKeyLeft | kKeyRight;

enum class PointerKind : std::uint8_t {
  None,   // Slot unused
  Mouse,  // Relative device with a persistent on-screen position
  Touch,  // Absolute device; position only meaningful while in contact
};

// One pointing device, resolved to GBA screen coordinates. A mouse and a
// touch contact are the same thing to everything downstream, which is what
// lets Spec 3 add touch without changing the navigation loop.
struct PointerState {
  PointerKind kind = PointerKind::None;
  bool in_viewport = false;     // Position lies inside the game viewport
  int gba_x = 0;                // 0..239 when in_viewport
  int gba_y = 0;                // 0..159 when in_viewport
  bool moved = false;           // Position changed since the previous poll
  bool primary_down = false;    // Left button, or touch contact
  bool secondary_down = false;  // Right button, or two-finger contact
  bool primary_edge = false;    // primary_down went false -> true this poll
  bool secondary_edge = false;  // secondary_down went false -> true this poll
};

constexpr std::size_t kMaxPointers = 4;

// Everything the platform layer produces for one frame. Sources OR their
// contributions into the same frame, so keyboard, gamepad and touch overlay
// can coexist.
struct InputFrame {
  std::uint16_t gba_keys = 0;    // Combined aw::kKey* bitmask
  std::uint16_t device_dpad = 0; // D-pad bits that came from a physical device
  std::array<PointerState, kMaxPointers> pointers{};
  std::size_t pointer_count = 0;

  void clear() {
    gba_keys = 0;
    device_dpad = 0;
    pointers = {};
    pointer_count = 0;
  }

  // First slot with an active device, or nullptr.
  const PointerState* primary_pointer() const {
    for (std::size_t i = 0; i < pointer_count && i < kMaxPointers; ++i) {
      if (pointers[i].kind != PointerKind::None) {
        return &pointers[i];
      }
    }
    return nullptr;
  }
};

}  // namespace aw
```

- [ ] **Step 4: Write `input_source.hpp`**

Create `runtime/include/aw/input/input_source.hpp`:

```c++
#pragma once

#include "aw/input/input_frame.hpp"

namespace aw {

// A platform binding that contributes to the current frame. Implementations
// OR into `frame` rather than overwriting it, so several sources compose.
class InputSource {
public:
  virtual ~InputSource() = default;

  // Called once per frame, before the emulator runs.
  virtual void poll(InputFrame& frame) = 0;
};

}  // namespace aw
```

- [ ] **Step 5: Write the viewport transform**

Create `runtime/include/aw/input/viewport.hpp`:

```c++
#pragma once

namespace aw {

// Maps a window client coordinate into GBA screen space (0..239, 0..159).
// Returns false, leaving the outputs untouched, when the point lies outside
// the viewport or the viewport is degenerate.
//
// This is the transform formerly known as Window::client_to_gba. It lives in
// the input layer because every platform needs it and none of them need a
// window handle to compute it.
bool viewport_to_gba(int vp_x, int vp_y, int vp_width, int vp_height,
                     int client_x, int client_y,
                     int& out_gba_x, int& out_gba_y);

}  // namespace aw
```

Create `runtime/src/input/viewport.cpp`:

```c++
#include "aw/input/viewport.hpp"

#include <algorithm>

namespace aw {

bool viewport_to_gba(int vp_x, int vp_y, int vp_width, int vp_height,
                     int client_x, int client_y,
                     int& out_gba_x, int& out_gba_y) {
  if (vp_width <= 0 || vp_height <= 0) return false;
  if (client_x < vp_x || client_x >= vp_x + vp_width) return false;
  if (client_y < vp_y || client_y >= vp_y + vp_height) return false;

  out_gba_x = std::clamp(((client_x - vp_x) * 240) / vp_width, 0, 239);
  out_gba_y = std::clamp(((client_y - vp_y) * 160) / vp_height, 0, 159);
  return true;
}

}  // namespace aw
```

- [ ] **Step 6: Add the source to the library and run the tests**

Add `src/input/viewport.cpp` to the `add_library(aw_runtime STATIC ...)` list in `runtime/CMakeLists.txt`, keeping the list alphabetical (after `src/input_config.cpp`).

Run: `cmake --build build/native --target input_core_tests && ctest --test-dir build/native -R input_core_tests --output-on-failure`
Expected: PASS

- [ ] **Step 7: Point `Window` at the shared transform**

In `runtime/src/window.cpp`, replace the body of `Window::client_to_gba` so there is one implementation of this maths, not two:

```c++
bool Window::client_to_gba(int client_x, int client_y, int& gba_x, int& gba_y) const {
  const ViewportRect& vp = cached_viewport_;
  return viewport_to_gba(vp.x, vp.y, vp.width, vp.height, client_x, client_y, gba_x, gba_y);
}
```

Add `#include "aw/input/viewport.hpp"` to the includes at the top of `runtime/src/window.cpp`.

- [ ] **Step 8: Run the full suite and commit**

Run: `ctest --test-dir build/native --output-on-failure`
Expected: all tests pass, including `window_aspect_tests`.

```bash
git add runtime/include/aw/input runtime/src/input runtime/tests/input_core_tests.cpp \
        runtime/CMakeLists.txt runtime/src/window.cpp
git commit -m "feat: add platform-neutral input frame, source interface and viewport transform"
```

---

### Task 3: `ProbeBackend` and its mGBA implementation

Gives the probe layer direct pointers to OAM and EWRAM instead of per-byte virtual calls, behind an interface that the static recompiler can implement later.

**Files:**
- Create: `runtime/include/aw/probe/backend.hpp`
- Create: `runtime/include/aw/probe/backend_mgba.hpp`
- Create: `runtime/src/probe/backend_mgba.cpp`
- Modify: `runtime/include/aw/mgba_adapter.h`
- Modify: `runtime/src/mgba_adapter.c`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `aw::ProbeBackend` (pure virtual: `available()`, `oam()`, `ewram(std::size_t&)`, `read_io16(std::uint32_t)`), `aw::MgbaProbeBackend(void* core)`, and the C helpers `aw_mgba_memory_block(struct mCore*, const char* internal_name, size_t* size_out)`.

- [ ] **Step 1: Declare the C-side memory-block accessor**

Add to `runtime/include/aw/mgba_adapter.h`, before the closing `#ifdef __cplusplus`:

```c
// Returns a pointer to a live core memory block looked up by mGBA's internal
// name ("wram", "iwram", "oam", "io"), or NULL. Looking blocks up by name
// avoids depending on mGBA's internal region enum.
void* aw_mgba_memory_block(struct mCore* core, const char* internal_name, size_t* size_out);
```

- [ ] **Step 2: Implement it**

Add to `runtime/src/mgba_adapter.c`:

```c
#include <string.h>

void* aw_mgba_memory_block(struct mCore* core, const char* internal_name, size_t* size_out) {
    if (size_out) *size_out = 0;
    if (!core || !internal_name) return NULL;
    if (!core->listMemoryBlocks || !core->getMemoryBlock) return NULL;

    const struct mCoreMemoryBlock* blocks = NULL;
    const size_t count = core->listMemoryBlocks(core, &blocks);
    if (!blocks) return NULL;

    for (size_t i = 0; i < count; ++i) {
        if (!blocks[i].internalName) continue;
        if (strcmp(blocks[i].internalName, internal_name) != 0) continue;

        size_t actual = 0;
        void* ptr = core->getMemoryBlock(core, blocks[i].id, &actual);
        if (!ptr) return NULL;
        if (size_out) *size_out = actual;
        return ptr;
    }
    return NULL;
}
```

- [ ] **Step 3: Write the neutral interface**

Create `runtime/include/aw/probe/backend.hpp`:

```c++
#pragma once

#include <cstddef>
#include <cstdint>

namespace aw {

// Read-only access to the running game's memory. mGBA implements this today;
// the static recompiler can implement it later without the probe, tracker or
// navigation layers noticing.
//
// Everything here is read-only by design: the navigation loop never writes
// game state, so the game's own cursor logic, camera and sound cannot desync.
class ProbeBackend {
public:
  virtual ~ProbeBackend() = default;

  // False when the backend cannot supply memory access at all. Callers must
  // disable themselves rather than fabricating data.
  virtual bool available() = 0;

  // 1 KB of OBJ attribute memory, or nullptr.
  virtual const std::uint8_t* oam() = 0;

  // 256 KB of external work RAM, or nullptr. `size_out` receives the real size.
  virtual const std::uint8_t* ewram(std::size_t& size_out) = 0;

  // A 16-bit memory-mapped IO register by absolute address, e.g. 0x04000010
  // for REG_BG0HOFS. Returns 0 when unavailable.
  virtual std::uint16_t read_io16(std::uint32_t addr) = 0;
};

// GBA BG scroll registers. These are hardware, so camera offset needs no
// reverse engineering.
constexpr std::uint32_t kRegBg0Hofs = 0x04000010;
constexpr std::uint32_t kRegBg0Vofs = 0x04000012;

// Byte offset from kRegBg0Hofs to layer `bg`'s horizontal scroll register.
constexpr std::uint32_t bg_hofs_reg(int bg) {
  return kRegBg0Hofs + static_cast<std::uint32_t>(bg) * 4u;
}
constexpr std::uint32_t bg_vofs_reg(int bg) {
  return kRegBg0Vofs + static_cast<std::uint32_t>(bg) * 4u;
}

}  // namespace aw
```

- [ ] **Step 4: Write the mGBA implementation**

Create `runtime/include/aw/probe/backend_mgba.hpp`:

```c++
#pragma once

#include "aw/probe/backend.hpp"

namespace aw {

// ProbeBackend over a live mGBA core. Block pointers are resolved once and
// cached; call set_core() again after a ROM switch to invalidate them.
class MgbaProbeBackend final : public ProbeBackend {
public:
  MgbaProbeBackend() = default;
  explicit MgbaProbeBackend(void* core) { set_core(core); }

  void set_core(void* core);

  bool available() override;
  const std::uint8_t* oam() override;
  const std::uint8_t* ewram(std::size_t& size_out) override;
  std::uint16_t read_io16(std::uint32_t addr) override;

private:
  void resolve();

  void* core_ = nullptr;
  bool resolved_ = false;
  const std::uint8_t* oam_ = nullptr;
  const std::uint8_t* ewram_ = nullptr;
  std::size_t ewram_size_ = 0;
};

}  // namespace aw
```

Create `runtime/src/probe/backend_mgba.cpp`:

```c++
#include "aw/probe/backend_mgba.hpp"

#include "aw/mgba_adapter.h"

#include <iostream>

namespace aw {

namespace {
constexpr std::size_t kOamBytes = 1024;
}

void MgbaProbeBackend::set_core(void* core) {
  core_ = core;
  resolved_ = false;
  oam_ = nullptr;
  ewram_ = nullptr;
  ewram_size_ = 0;
}

void MgbaProbeBackend::resolve() {
  if (resolved_ || core_ == nullptr) return;
  resolved_ = true;

  auto* core = static_cast<struct mCore*>(core_);

  std::size_t oam_size = 0;
  void* oam_ptr = aw_mgba_memory_block(core, "oam", &oam_size);
  if (oam_ptr != nullptr && oam_size >= kOamBytes) {
    oam_ = static_cast<const std::uint8_t*>(oam_ptr);
  }

  std::size_t ewram_size = 0;
  void* ewram_ptr = aw_mgba_memory_block(core, "wram", &ewram_size);
  if (ewram_ptr != nullptr && ewram_size > 0) {
    ewram_ = static_cast<const std::uint8_t*>(ewram_ptr);
    ewram_size_ = ewram_size;
  }

  if (oam_ == nullptr || ewram_ == nullptr) {
    std::cerr << "[probe] mGBA memory blocks unavailable (oam="
              << (oam_ != nullptr) << ", wram=" << (ewram_ != nullptr)
              << "); pointer navigation disabled\n";
  }
}

bool MgbaProbeBackend::available() {
  resolve();
  return oam_ != nullptr && ewram_ != nullptr;
}

const std::uint8_t* MgbaProbeBackend::oam() {
  resolve();
  return oam_;
}

const std::uint8_t* MgbaProbeBackend::ewram(std::size_t& size_out) {
  resolve();
  size_out = ewram_size_;
  return ewram_;
}

std::uint16_t MgbaProbeBackend::read_io16(std::uint32_t addr) {
  if (core_ == nullptr) return 0;
  return aw_mgba_read16(static_cast<struct mCore*>(core_), addr);
}

}  // namespace aw
```

- [ ] **Step 5: Build**

Add `src/probe/backend_mgba.cpp` to the `aw_runtime` source list in `runtime/CMakeLists.txt`.

Run: `cmake --build build/native --target advance-wars-native`
Expected: builds clean.

- [ ] **Step 6: Verify the blocks resolve against the real ROM**

Temporarily add to `run_game_loop` in `runtime/src/main.cpp`, right after `window.load_config(config)`:

```c++
    aw::MgbaProbeBackend probe_check(core);
    std::size_t probe_ewram_size = 0;
    probe_check.ewram(probe_ewram_size);
    std::cout << "[probe] available=" << probe_check.available()
              << " oam=" << (probe_check.oam() != nullptr)
              << " ewram_size=" << probe_ewram_size
              << " bg0hofs=" << probe_check.read_io16(aw::kRegBg0Hofs) << "\n";
```

(with `#include "aw/probe/backend_mgba.hpp"` at the top). Note this must go *after* the `core` is created — move it below the `aw_mgba_create` call if the compiler complains about ordering.

Run: `build/native/runtime/advance-wars-native.exe "rom/Advance Wars (USA) (Rev 1).gba" --frames 120`
Expected: `[probe] available=1 oam=1 ewram_size=262144 bg0hofs=<some number>`

If `available=0`, the block names are wrong — print every `internalName` from `listMemoryBlocks` and use the real ones.

- [ ] **Step 7: Remove the temporary probe and commit**

Delete the temporary block added in Step 6 (Task 8 wires the probe in properly).

```bash
git add runtime/include/aw/probe runtime/src/probe runtime/include/aw/mgba_adapter.h \
        runtime/src/mgba_adapter.c runtime/CMakeLists.txt
git commit -m "feat: add ProbeBackend interface and mGBA implementation

Resolves memory blocks by mGBA internal name and caches the pointers, so
OAM and EWRAM reads are pointer dereferences instead of the 262144
rawRead8 calls the old scanner made per snapshot."
```

---

### Task 4: OAM decoding, plus the diagnostic that answers the design's open risk

The whole design assumes the selection indicator is an OAM sprite in each of the four contexts. This task produces the pure decoder (production code) and a logging mode that answers the question against the real game.

**Files:**
- Create: `runtime/include/aw/probe/oam.hpp`
- Create: `runtime/src/probe/oam.cpp`
- Create: `runtime/tests/oam_tests.cpp`
- Create: `scripts/analyze-oam-log.py`
- Create: `docs/oam-indicator-findings.md`
- Modify: `runtime/src/main.cpp` (add `--oam-log <path>`)
- Modify: `runtime/CMakeLists.txt`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: `aw::ProbeBackend` (Task 3).
- Produces: `aw::OamEntry` (fields `x`, `y`, `tile`, `palette`, `priority`, `visible`, `on_screen()`), `aw::kOamEntryCount`, `aw::decode_oam_entry(const std::uint8_t* oam, std::size_t index)`.

- [ ] **Step 1: Write the failing test**

Create `runtime/tests/oam_tests.cpp`:

```c++
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/probe/oam.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

// Builds a 1 KB OAM buffer and writes one entry's four attribute halfwords.
struct OamBuffer {
  std::array<std::uint8_t, 1024> bytes{};

  void set(std::size_t index, std::uint16_t attr0, std::uint16_t attr1, std::uint16_t attr2) {
    const std::size_t base = index * 8;
    bytes[base + 0] = static_cast<std::uint8_t>(attr0 & 0xFF);
    bytes[base + 1] = static_cast<std::uint8_t>(attr0 >> 8);
    bytes[base + 2] = static_cast<std::uint8_t>(attr1 & 0xFF);
    bytes[base + 3] = static_cast<std::uint8_t>(attr1 >> 8);
    bytes[base + 4] = static_cast<std::uint8_t>(attr2 & 0xFF);
    bytes[base + 5] = static_cast<std::uint8_t>(attr2 >> 8);
  }
};

void tests_decodes_position_tile_and_palette() {
  OamBuffer oam;
  // y = 80, x = 120, tile = 0x123, palette bank 5, priority 2.
  oam.set(3, /*attr0=*/80, /*attr1=*/120,
          /*attr2=*/static_cast<std::uint16_t>(0x123 | (2 << 10) | (5 << 12)));

  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 3);
  require_equal(e.y, 80, "y");
  require_equal(e.x, 120, "x");
  require_equal(e.tile, 0x123, "tile");
  require_equal(e.priority, 2, "priority");
  require_equal(e.palette, 5, "palette");
  require_equal(e.visible, true, "visible");
  require_equal(e.on_screen(), true, "on screen");
}

void tests_x_is_nine_bit_signed() {
  OamBuffer oam;
  // attr1 X field is 9 bits; 0x1F0 (496) means -16.
  oam.set(0, /*attr0=*/40, /*attr1=*/0x1F0, /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 0);
  require_equal(e.x, -16, "negative x");
}

void tests_disabled_objects_are_not_visible() {
  OamBuffer oam;
  // attr0 bits 8-9 == 0b10 is the "disabled" object mode.
  oam.set(1, /*attr0=*/static_cast<std::uint16_t>(60 | (2 << 8)), /*attr1=*/50, /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 1);
  require_equal(e.visible, false, "disabled is invisible");
  require_equal(e.on_screen(), false, "disabled is off screen");
}

void tests_offscreen_y_is_not_on_screen() {
  OamBuffer oam;
  // Y = 200 is the usual "parked below the screen" idiom.
  oam.set(2, /*attr0=*/200, /*attr1=*/50, /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 2);
  require_equal(e.visible, true, "not disabled");
  require_equal(e.on_screen(), false, "parked below screen");
}

void tests_out_of_range_index_is_invisible() {
  OamBuffer oam;
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), aw::kOamEntryCount);
  require_equal(e.visible, false, "out of range is invisible");
}

void tests_null_buffer_is_invisible() {
  const aw::OamEntry e = aw::decode_oam_entry(nullptr, 0);
  require_equal(e.visible, false, "null buffer is invisible");
}

}  // namespace

int main() {
  try {
    tests_decodes_position_tile_and_palette();
    tests_x_is_nine_bit_signed();
    tests_disabled_objects_are_not_visible();
    tests_offscreen_y_is_not_on_screen();
    tests_out_of_range_index_is_invisible();
    tests_null_buffer_is_invisible();
    std::cout << "oam_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
```

- [ ] **Step 2: Register and run it to verify it fails**

Append to `runtime/CMakeLists.txt` before the `advance-wars-native` block:

```cmake
add_executable(oam_tests
  tests/oam_tests.cpp
)

target_include_directories(oam_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(oam_tests PRIVATE aw_runtime)

add_test(NAME oam_tests COMMAND oam_tests)
```

Run: `cmake --build build/native --target oam_tests`
Expected: FAIL — `aw/probe/oam.hpp: No such file or directory`

- [ ] **Step 3: Write the decoder**

Create `runtime/include/aw/probe/oam.hpp`:

```c++
#pragma once

#include <cstddef>
#include <cstdint>

namespace aw {

constexpr std::size_t kOamEntryCount = 128;
constexpr std::size_t kOamBytes = kOamEntryCount * 8;

// One decoded OBJ attribute entry. GBA OAM stores sprite position in *screen*
// coordinates, which is why tracking a sprite gives the selection indicator's
// position in the same space the mouse reports.
struct OamEntry {
  int x = 0;             // Screen X, 9-bit signed (-256..255)
  int y = 0;             // Screen Y, 0..255 as stored
  int tile = 0;          // Character name, attr2 bits 0-9
  int palette = 0;       // Palette bank, attr2 bits 12-15
  int priority = 0;      // attr2 bits 10-11
  bool visible = false;  // Not in the disabled object mode

  bool on_screen() const {
    return visible && y < 160 && x > -64 && x < 240;
  }
};

// Decodes entry `index` from a 1 KB OAM block. Returns a default-constructed
// (invisible) entry for a null buffer or an out-of-range index.
OamEntry decode_oam_entry(const std::uint8_t* oam, std::size_t index);

}  // namespace aw
```

Create `runtime/src/probe/oam.cpp`:

```c++
#include "aw/probe/oam.hpp"

namespace aw {

namespace {

std::uint16_t read16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

}  // namespace

OamEntry decode_oam_entry(const std::uint8_t* oam, std::size_t index) {
  OamEntry entry;
  if (oam == nullptr || index >= kOamEntryCount) return entry;

  const std::uint8_t* p = oam + index * 8;
  const std::uint16_t attr0 = read16(p + 0);
  const std::uint16_t attr1 = read16(p + 2);
  const std::uint16_t attr2 = read16(p + 4);

  // attr0 bits 8-9 select the object mode; 0b10 means the object is disabled.
  entry.visible = ((attr0 >> 8) & 0x3) != 0x2;

  entry.y = attr0 & 0xFF;

  const int raw_x = attr1 & 0x1FF;
  entry.x = (raw_x >= 256) ? raw_x - 512 : raw_x;

  entry.tile = attr2 & 0x3FF;
  entry.priority = (attr2 >> 10) & 0x3;
  entry.palette = (attr2 >> 12) & 0xF;
  return entry;
}

}  // namespace aw
```

- [ ] **Step 4: Run the tests**

Add `src/probe/oam.cpp` to the `aw_runtime` source list.

Run: `cmake --build build/native --target oam_tests && ctest --test-dir build/native -R oam_tests --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit the decoder**

```bash
git add runtime/include/aw/probe/oam.hpp runtime/src/probe/oam.cpp \
        runtime/tests/oam_tests.cpp runtime/CMakeLists.txt
git commit -m "feat: add pure OAM entry decoder with 9-bit signed X handling"
```

- [ ] **Step 6: Add the `--oam-log` diagnostic mode**

In `runtime/src/main.cpp`, add to `struct Options`:

```c++
  std::string oam_log_path;  // Non-empty enables per-frame OAM delta logging
```

In `parse_options`, inside the argument loop, before the final `else`:

```c++
    } else if (arg.rfind("--oam-log=", 0) == 0) {
      options.oam_log_path = arg.substr(10);
    } else if (arg == "--oam-log" && i + 1 < argc) {
      options.oam_log_path = argv[++i];
```

Change `run_game_loop`'s signature to accept the path, and update its one call site in `main`:

```c++
void run_game_loop(std::filesystem::path rom_path, aw::RomImage rom, int max_frames,
                   const std::string& oam_log_path)
```

```c++
      run_game_loop(options.rom_path, rom, options.max_frames, options.oam_log_path);
```

Add these includes at the top of `main.cpp`:

```c++
#include "aw/probe/backend_mgba.hpp"
#include "aw/probe/oam.hpp"
```

Inside `run_game_loop`, after the `core` is created and `window.set_...` calls are done:

```c++
    aw::MgbaProbeBackend probe(core);
    std::ofstream oam_log;
    std::vector<aw::OamEntry> oam_prev(aw::kOamEntryCount);
    if (!oam_log_path.empty()) {
      oam_log.open(oam_log_path);
      oam_log << "# frame keys index dx dy x y tile palette\n";
    }
```

At the end of the frame body, immediately after `window.render(ppu);`:

```c++
      if (oam_log.is_open()) {
        const std::uint8_t* oam_bytes = probe.oam();
        if (oam_bytes != nullptr) {
          for (std::size_t i = 0; i < aw::kOamEntryCount; ++i) {
            const aw::OamEntry cur = aw::decode_oam_entry(oam_bytes, i);
            const aw::OamEntry& prev = oam_prev[i];
            const int dx = cur.x - prev.x;
            const int dy = cur.y - prev.y;
            if (cur.on_screen() && prev.on_screen() && (dx != 0 || dy != 0)) {
              oam_log << frames_run << ' ' << hardware.keys_pressed << ' ' << i << ' '
                      << dx << ' ' << dy << ' ' << cur.x << ' ' << cur.y << ' '
                      << cur.tile << ' ' << cur.palette << '\n';
            }
            oam_prev[i] = cur;
          }
        }
      }
```

`hardware.keys_pressed` is still both in scope and holding this frame's keys at that point: the loop sets it to 0 at the top, `window.process_events(hardware)` fills it, `aw_mgba_run_frame` consumes it, and nothing clears it before `window.render(ppu)`.

Add `*.oamlog` to `.gitignore`.

- [ ] **Step 7: Build and collect the data**

Run:
```bash
cmake --build build/native --target advance-wars-native
build/native/runtime/advance-wars-native.exe "rom/Advance Wars (USA) (Rev 1).gba" --oam-log oam.oamlog
```

While it runs, visit all four contexts and move the selection with the arrow keys in each:
1. **Map view** — start any mission, move the cursor around the map, including to the screen edge so the camera scrolls.
2. **Action / list menu** — select a unit to open the action menu, move up and down.
3. **Name entry** — start a new game / rename, move across the letter grid.
4. **Front end** — title menu, CO select, map select.

Then close the window.

- [ ] **Step 8: Write the analysis script**

Create `scripts/analyze-oam-log.py`:

```python
#!/usr/bin/env python3
"""Summarise an --oam-log capture: which OAM entries move with the D-pad.

Usage: python scripts/analyze-oam-log.py oam.oamlog
"""
import collections
import sys

KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN = 1 << 4, 1 << 5, 1 << 6, 1 << 7


def main(path):
    # (tile, palette) -> counts of moves that agreed / disagreed with the D-pad
    agree = collections.Counter()
    disagree = collections.Counter()
    seen = collections.Counter()

    with open(path) as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) != 9:
                continue
            _frame, keys, _index, dx, dy, _x, _y, tile, palette = (int(p) for p in parts)
            sig = (tile, palette)
            seen[sig] += 1
            if keys & KEY_RIGHT and dx > 0 or keys & KEY_LEFT and dx < 0 \
               or keys & KEY_DOWN and dy > 0 or keys & KEY_UP and dy < 0:
                agree[sig] += 1
            elif keys & (KEY_RIGHT | KEY_LEFT | KEY_UP | KEY_DOWN):
                disagree[sig] += 1

    rows = []
    for sig, total in seen.items():
        a, d = agree[sig], disagree[sig]
        score = a - d
        rows.append((score, a, d, total, sig))
    rows.sort(reverse=True)

    print(f"{'score':>6} {'agree':>6} {'disagr':>6} {'moves':>6}  tile  palette")
    for score, a, d, total, (tile, palette) in rows[:20]:
        print(f"{score:>6} {a:>6} {d:>6} {total:>6}  0x{tile:03X}  {palette}")

    if not rows:
        print("No moving on-screen sprites recorded. The indicator is probably")
        print("a background tile, not a sprite: use the BG-tilemap fallback.")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "oam.oamlog")
```

- [ ] **Step 9: Analyse and record the findings**

Run: `python scripts/analyze-oam-log.py oam.oamlog`

Create `docs/oam-indicator-findings.md` recording, for each of the four contexts, whether a sprite signature moved in agreement with the D-pad, and the winning `(tile, palette)` pair. State the verdict explicitly.

**Decision gate:**
- If a clear winning signature exists for a context, that context uses OAM tracking. Proceed to Task 5 unchanged.
- If a context has no agreeing sprite (empty output, or every candidate's score near zero), that context's indicator is a background tile. Note it in the findings file, **do not** block the plan: Tasks 5-8 still deliver working steering for the contexts that do work, and the BG-tilemap fallback becomes a follow-up task appended to this plan. `ContextProbe` already carries a `steerable` flag to disable steering where tracking is impossible.

- [ ] **Step 10: Commit**

```bash
git add runtime/src/main.cpp scripts/analyze-oam-log.py \
        docs/oam-indicator-findings.md .gitignore
git commit -m "feat: add --oam-log diagnostic and record indicator findings

Answers the design's open risk: whether the selection indicator is an OAM
sprite in each UI context. Findings in docs/oam-indicator-findings.md."
```

---

### Task 5: `OamTracker`

Locates the selection indicator each frame — by signature when one is known, and by correlating movement against the D-pad we emitted when one is not. Correlation is what makes steering work before any symbols are mined.

**Files:**
- Create: `runtime/include/aw/probe/oam_tracker.hpp`
- Create: `runtime/src/probe/oam_tracker.cpp`
- Create: `runtime/tests/oam_tracker_tests.cpp`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Consumes: `aw::decode_oam_entry`, `aw::OamEntry`, `aw::kOamEntryCount` (Task 4); `aw::kDpadMask` and `aw::kKey*` (Task 2).
- Produces: `aw::IndicatorSignature { int tile; int palette; }` (`-1` = wildcard), `aw::Indicator { bool found; int screen_x; int screen_y; std::size_t oam_index; }`, `aw::OamTracker` with `reset()`, `set_signature(const IndicatorSignature&)`, `Indicator update(const std::uint8_t* oam, std::uint16_t emitted_dpad)`, `bool locked() const`, `IndicatorSignature signature() const`.

- [ ] **Step 1: Write the failing test**

Create `runtime/tests/oam_tracker_tests.cpp`:

```c++
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/hardware.hpp"
#include "aw/probe/oam_tracker.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

struct OamBuffer {
  std::array<std::uint8_t, 1024> bytes{};

  OamBuffer() {
    // Park every entry off-screen by default.
    for (std::size_t i = 0; i < aw::kOamEntryCount; ++i) {
      place(i, 0, 200, 0, 0);
    }
  }

  void place(std::size_t index, int x, int y, int tile, int palette) {
    const std::uint16_t attr0 = static_cast<std::uint16_t>(y & 0xFF);
    const std::uint16_t attr1 = static_cast<std::uint16_t>(x & 0x1FF);
    const std::uint16_t attr2 =
        static_cast<std::uint16_t>((tile & 0x3FF) | ((palette & 0xF) << 12));
    const std::size_t base = index * 8;
    bytes[base + 0] = static_cast<std::uint8_t>(attr0 & 0xFF);
    bytes[base + 1] = static_cast<std::uint8_t>(attr0 >> 8);
    bytes[base + 2] = static_cast<std::uint8_t>(attr1 & 0xFF);
    bytes[base + 3] = static_cast<std::uint8_t>(attr1 >> 8);
    bytes[base + 4] = static_cast<std::uint8_t>(attr2 & 0xFF);
    bytes[base + 5] = static_cast<std::uint8_t>(attr2 >> 8);
  }
};

void tests_signature_lock_reports_position_immediately() {
  aw::OamTracker tracker;
  tracker.set_signature({/*tile=*/0x040, /*palette=*/3});

  OamBuffer oam;
  oam.place(7, /*x=*/96, /*y=*/64, /*tile=*/0x040, /*palette=*/3);

  const aw::Indicator ind = tracker.update(oam.bytes.data(), 0);
  require_equal(ind.found, true, "found by signature");
  require_equal(ind.screen_x, 96, "signature x");
  require_equal(ind.screen_y, 64, "signature y");
  require_equal(ind.oam_index, std::size_t{7}, "signature index");
}

void tests_signature_wildcards_match_any_palette() {
  aw::OamTracker tracker;
  tracker.set_signature({/*tile=*/0x040, /*palette=*/-1});

  OamBuffer oam;
  oam.place(2, 48, 32, 0x040, /*palette=*/9);

  const aw::Indicator ind = tracker.update(oam.bytes.data(), 0);
  require_equal(ind.found, true, "wildcard palette matches");
  require_equal(ind.oam_index, std::size_t{2}, "wildcard index");
}

void tests_correlation_locks_on_after_consistent_agreement() {
  aw::OamTracker tracker;  // No signature set.

  OamBuffer oam;
  // Entry 5 is the indicator; entry 6 is a distractor that never moves.
  int x = 32;
  oam.place(5, x, 64, /*tile=*/0x111, /*palette=*/1);
  oam.place(6, 200, 100, /*tile=*/0x222, /*palette=*/2);

  // Prime the tracker with the first frame (no previous frame to compare).
  tracker.update(oam.bytes.data(), 0);
  require_equal(tracker.locked(), false, "not locked before any motion");

  // Three frames of Right, with entry 5 moving right by 16 each time.
  for (int i = 0; i < 3; ++i) {
    x += 16;
    oam.place(5, x, 64, 0x111, 1);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }

  require_equal(tracker.locked(), true, "locked after consistent agreement");
  require_equal(tracker.signature().tile, 0x111, "locked onto the mover");

  const aw::Indicator ind = tracker.update(oam.bytes.data(), 0);
  require_equal(ind.found, true, "reports position once locked");
  require_equal(ind.screen_x, x, "locked x");
}

void tests_correlation_ignores_sprites_moving_the_wrong_way() {
  aw::OamTracker tracker;

  OamBuffer oam;
  int good = 32;
  int bad = 180;
  oam.place(1, good, 64, /*tile=*/0x300, /*palette=*/1);
  oam.place(2, bad, 90, /*tile=*/0x400, /*palette=*/2);
  tracker.update(oam.bytes.data(), 0);

  // Press Right repeatedly. Entry 1 moves right; entry 2 moves *left*.
  for (int i = 0; i < 3; ++i) {
    good += 16;
    bad -= 16;
    oam.place(1, good, 64, 0x300, 1);
    oam.place(2, bad, 90, 0x400, 2);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }

  require_equal(tracker.locked(), true, "locked");
  require_equal(tracker.signature().tile, 0x300, "locked onto the agreeing sprite");
}

void tests_indicator_absent_is_reported_not_crashed() {
  aw::OamTracker tracker;
  tracker.set_signature({/*tile=*/0x555, /*palette=*/1});

  OamBuffer oam;  // Nothing on screen.
  const aw::Indicator ind = tracker.update(oam.bytes.data(), 0);
  require_equal(ind.found, false, "absent indicator reported as not found");
}

void tests_null_oam_is_safe() {
  aw::OamTracker tracker;
  const aw::Indicator ind = tracker.update(nullptr, aw::kKeyRight);
  require_equal(ind.found, false, "null oam is not found");
}

void tests_reset_clears_the_lock() {
  aw::OamTracker tracker;
  tracker.set_signature({0x040, 3});
  OamBuffer oam;
  oam.place(0, 10, 10, 0x040, 3);
  tracker.update(oam.bytes.data(), 0);

  tracker.reset();
  require_equal(tracker.locked(), false, "reset unlocks");
}

}  // namespace

int main() {
  try {
    tests_signature_lock_reports_position_immediately();
    tests_signature_wildcards_match_any_palette();
    tests_correlation_locks_on_after_consistent_agreement();
    tests_correlation_ignores_sprites_moving_the_wrong_way();
    tests_indicator_absent_is_reported_not_crashed();
    tests_null_oam_is_safe();
    tests_reset_clears_the_lock();
    std::cout << "oam_tracker_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
```

- [ ] **Step 2: Register and run it to verify it fails**

Append to `runtime/CMakeLists.txt` before the `advance-wars-native` block:

```cmake
add_executable(oam_tracker_tests
  tests/oam_tracker_tests.cpp
)

target_include_directories(oam_tracker_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(oam_tracker_tests PRIVATE aw_runtime)

add_test(NAME oam_tracker_tests COMMAND oam_tracker_tests)
```

Run: `cmake --build build/native --target oam_tracker_tests`
Expected: FAIL — `aw/probe/oam_tracker.hpp: No such file or directory`

- [ ] **Step 3: Write the header**

Create `runtime/include/aw/probe/oam_tracker.hpp`:

```c++
#pragma once

#include "aw/probe/oam.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace aw {

// Identifies the indicator sprite by its character and palette. -1 is a
// wildcard, so a context can pin the tile and let the palette vary by army.
struct IndicatorSignature {
  int tile = -1;
  int palette = -1;

  bool wildcard() const { return tile < 0 && palette < 0; }
  bool matches(const OamEntry& e) const {
    return (tile < 0 || e.tile == tile) && (palette < 0 || e.palette == palette);
  }
};

struct Indicator {
  bool found = false;
  int screen_x = 0;
  int screen_y = 0;
  std::size_t oam_index = kOamEntryCount;
};

// Tracks the game's selection indicator through OAM.
//
// Two modes. With a signature (from the symbol table) the indicator is found
// by matching tile/palette. Without one, the tracker correlates movement
// against the D-pad it was told we emitted: the sprite that keeps moving the
// way we commanded is the indicator. Correlation means steering works with no
// mined symbols at all, and it re-locks by itself if a stale signature stops
// matching.
class OamTracker {
public:
  void reset();

  // Pins the signature explicitly. Pass a wildcard signature to fall back to
  // correlation.
  void set_signature(const IndicatorSignature& sig);

  // Call once per frame *after* the emulator has run. `emitted_dpad` is the
  // D-pad mask sent to the core for the frame that just executed.
  Indicator update(const std::uint8_t* oam, std::uint16_t emitted_dpad);

  bool locked() const { return locked_; }
  IndicatorSignature signature() const { return signature_; }

private:
  Indicator find_by_signature(const std::uint8_t* oam) const;
  void correlate(const std::uint8_t* oam, std::uint16_t emitted_dpad);

  static constexpr int kLockScore = 3;      // Net agreements needed to lock
  static constexpr int kMaxStepPixels = 32; // Larger jumps are not cursor steps
  static constexpr int kUnlockFrames = 60;  // Frames absent before re-locking
  static constexpr std::size_t kMaxCandidates = 32;

  struct Candidate {
    IndicatorSignature sig;
    int score = 0;
    bool used = false;
  };

  Candidate* candidate_for(const IndicatorSignature& sig);

  IndicatorSignature signature_{};
  bool locked_ = false;
  bool has_prev_ = false;
  int missing_frames_ = 0;
  std::array<OamEntry, kOamEntryCount> prev_{};
  std::array<Candidate, kMaxCandidates> candidates_{};
};

}  // namespace aw
```

- [ ] **Step 4: Write the implementation**

Create `runtime/src/probe/oam_tracker.cpp`:

```c++
#include "aw/probe/oam_tracker.hpp"

#include "aw/hardware.hpp"

#include <cstdlib>

namespace aw {

void OamTracker::reset() {
  signature_ = {};
  locked_ = false;
  has_prev_ = false;
  missing_frames_ = 0;
  prev_ = {};
  candidates_ = {};
}

void OamTracker::set_signature(const IndicatorSignature& sig) {
  signature_ = sig;
  locked_ = !sig.wildcard();
  missing_frames_ = 0;
}

OamTracker::Candidate* OamTracker::candidate_for(const IndicatorSignature& sig) {
  Candidate* free_slot = nullptr;
  for (auto& c : candidates_) {
    if (c.used && c.sig.tile == sig.tile && c.sig.palette == sig.palette) {
      return &c;
    }
    if (!c.used && free_slot == nullptr) {
      free_slot = &c;
    }
  }
  if (free_slot != nullptr) {
    free_slot->used = true;
    free_slot->sig = sig;
    free_slot->score = 0;
    return free_slot;
  }
  return nullptr;
}

Indicator OamTracker::find_by_signature(const std::uint8_t* oam) const {
  Indicator result;
  for (std::size_t i = 0; i < kOamEntryCount; ++i) {
    const OamEntry e = decode_oam_entry(oam, i);
    if (!e.on_screen()) continue;
    if (!signature_.matches(e)) continue;
    result.found = true;
    result.screen_x = e.x;
    result.screen_y = e.y;
    result.oam_index = i;
    return result;  // Lowest index wins.
  }
  return result;
}

void OamTracker::correlate(const std::uint8_t* oam, std::uint16_t emitted_dpad) {
  const bool right = (emitted_dpad & kKeyRight) != 0;
  const bool left = (emitted_dpad & kKeyLeft) != 0;
  const bool down = (emitted_dpad & kKeyDown) != 0;
  const bool up = (emitted_dpad & kKeyUp) != 0;
  if (!right && !left && !down && !up) return;
  if (!has_prev_) return;

  for (std::size_t i = 0; i < kOamEntryCount; ++i) {
    const OamEntry cur = decode_oam_entry(oam, i);
    const OamEntry& prev = prev_[i];
    if (!cur.on_screen() || !prev.on_screen()) continue;
    // A sprite that changed identity is a different object reusing the slot.
    if (cur.tile != prev.tile || cur.palette != prev.palette) continue;

    const int dx = cur.x - prev.x;
    const int dy = cur.y - prev.y;
    if (dx == 0 && dy == 0) continue;
    if (std::abs(dx) > kMaxStepPixels || std::abs(dy) > kMaxStepPixels) continue;

    const bool agrees = (right && dx > 0) || (left && dx < 0) ||
                        (down && dy > 0) || (up && dy < 0);
    const bool opposes = (right && dx < 0) || (left && dx > 0) ||
                         (down && dy < 0) || (up && dy > 0);
    if (!agrees && !opposes) continue;

    IndicatorSignature sig{cur.tile, cur.palette};
    Candidate* c = candidate_for(sig);
    if (c == nullptr) continue;
    c->score += agrees ? 1 : -1;

    if (!locked_ && c->score >= kLockScore) {
      signature_ = sig;
      locked_ = true;
      missing_frames_ = 0;
    }
  }
}

Indicator OamTracker::update(const std::uint8_t* oam, std::uint16_t emitted_dpad) {
  if (oam == nullptr) {
    has_prev_ = false;
    return {};
  }

  correlate(oam, emitted_dpad);

  Indicator result;
  if (locked_) {
    result = find_by_signature(oam);
    if (result.found) {
      missing_frames_ = 0;
    } else if (++missing_frames_ >= kUnlockFrames) {
      // The signature stopped matching: probably a context change. Drop the
      // lock and let correlation find the new indicator.
      locked_ = false;
      signature_ = {};
      candidates_ = {};
      missing_frames_ = 0;
    }
  }

  for (std::size_t i = 0; i < kOamEntryCount; ++i) {
    prev_[i] = decode_oam_entry(oam, i);
  }
  has_prev_ = true;
  return result;
}

}  // namespace aw
```

- [ ] **Step 5: Run the tests**

Add `src/probe/oam_tracker.cpp` to the `aw_runtime` source list.

Run: `cmake --build build/native --target oam_tracker_tests && ctest --test-dir build/native -R oam_tracker_tests --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add runtime/include/aw/probe/oam_tracker.hpp runtime/src/probe/oam_tracker.cpp \
        runtime/tests/oam_tracker_tests.cpp runtime/CMakeLists.txt
git commit -m "feat: add OamTracker with signature and correlation lock-on

Correlation mode locks onto whichever sprite consistently moves the way the
emitted D-pad commanded, so the indicator is found with no mined symbols."
```

---

### Task 6: Symbol table and `ContextProbe`

Loads the mined per-context data and classifies the active context. Everything degrades safely: no file, wrong ROM, or no matching rule all resolve to `Unknown` with a wildcard signature, which is exactly correlation mode.

**Files:**
- Create: `runtime/include/aw/probe/context.hpp`
- Create: `runtime/src/probe/context.cpp`
- Create: `runtime/tests/context_probe_tests.cpp`
- Create: `data/symbols/README.md`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Consumes: `aw::ProbeBackend` (Task 3), `aw::IndicatorSignature` (Task 5), `aw::ConfigFile` from `runtime/include/aw/config_file.hpp`.
- Produces: `aw::ContextId` (`Unknown`, `MapView`, `ListMenu`, `NameEntry`, `FrontEnd`, `Cutscene`), `aw::ContextRule { ContextId id; std::vector<Predicate> predicates; IndicatorSignature signature; int scroll_bg; bool steerable; }`, `aw::SymbolTable` with `load_from_file`, `matches_rom`, `contexts`, and `aw::ContextProbe` with `set_table`, `classify(ProbeBackend&)`, `rule_for(ContextId)`.

- [ ] **Step 1: Write the failing test**

Create `runtime/tests/context_probe_tests.cpp`:

```c++
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <vector>

#include "aw/probe/context.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

// A ProbeBackend backed by a plain vector, so the probe is testable with no
// emulator.
class FakeBackend final : public aw::ProbeBackend {
public:
  FakeBackend() : ewram_(0x1000, 0) {}

  void poke(std::uint32_t addr, std::uint8_t value) {
    ewram_[addr - 0x02000000] = value;
  }

  bool available() override { return true; }
  const std::uint8_t* oam() override { return nullptr; }
  const std::uint8_t* ewram(std::size_t& size_out) override {
    size_out = ewram_.size();
    return ewram_.data();
  }
  std::uint16_t read_io16(std::uint32_t) override { return 0; }

private:
  std::vector<std::uint8_t> ewram_;
};

aw::SymbolTable two_context_table() {
  aw::SymbolTable table;
  table.rom_sha1 = "15053499D5B3F49128A941D7F2D84876F5424D0C";

  aw::ContextRule map;
  map.id = aw::ContextId::MapView;
  map.predicates.push_back({0x02000100, 0x03});
  map.signature = {0x040, -1};
  map.scroll_bg = 1;
  map.steerable = true;
  table.contexts.push_back(map);

  aw::ContextRule cutscene;
  cutscene.id = aw::ContextId::Cutscene;
  cutscene.predicates.push_back({0x02000100, 0x09});
  cutscene.steerable = false;
  table.contexts.push_back(cutscene);

  return table;
}

void tests_classifies_by_predicate() {
  aw::ContextProbe probe;
  probe.set_table(two_context_table());

  FakeBackend backend;
  backend.poke(0x02000100, 0x03);
  require_equal(probe.classify(backend) == aw::ContextId::MapView, true, "map view");

  backend.poke(0x02000100, 0x09);
  require_equal(probe.classify(backend) == aw::ContextId::Cutscene, true, "cutscene");
}

void tests_unmatched_value_is_unknown() {
  aw::ContextProbe probe;
  probe.set_table(two_context_table());

  FakeBackend backend;
  backend.poke(0x02000100, 0x77);
  require_equal(probe.classify(backend) == aw::ContextId::Unknown, true, "unmatched");
}

void tests_empty_table_is_unknown_but_steerable() {
  aw::ContextProbe probe;  // No table loaded at all.

  FakeBackend backend;
  require_equal(probe.classify(backend) == aw::ContextId::Unknown, true, "no table");

  // Unknown must remain steerable so correlation mode works with no symbols.
  const aw::ContextRule& rule = probe.rule_for(aw::ContextId::Unknown);
  require_equal(rule.steerable, true, "unknown is steerable");
  require_equal(rule.signature.wildcard(), true, "unknown uses wildcard signature");
}

void tests_rule_lookup_returns_the_matching_rule() {
  aw::ContextProbe probe;
  probe.set_table(two_context_table());

  const aw::ContextRule& map = probe.rule_for(aw::ContextId::MapView);
  require_equal(map.scroll_bg, 1, "map scroll bg");
  require_equal(map.signature.tile, 0x040, "map signature tile");

  const aw::ContextRule& cut = probe.rule_for(aw::ContextId::Cutscene);
  require_equal(cut.steerable, false, "cutscene not steerable");
}

void tests_rom_matching_is_case_insensitive() {
  aw::SymbolTable table = two_context_table();
  require_equal(table.matches_rom("15053499d5b3f49128a941d7f2d84876f5424d0c"), true, "lowercase");
  require_equal(table.matches_rom("15053499D5B3F49128A941D7F2D84876F5424D0C"), true, "uppercase");
  require_equal(table.matches_rom("D0A0A4CFE9B95AC7118F7EF476F014CA0242EB65"), false, "rev 0");
}

void tests_predicate_outside_ewram_never_matches() {
  aw::SymbolTable table;
  aw::ContextRule rule;
  rule.id = aw::ContextId::MapView;
  rule.predicates.push_back({0x02FFFFFF, 0x00});  // Beyond the fake's 0x1000 bytes
  table.contexts.push_back(rule);

  aw::ContextProbe probe;
  probe.set_table(table);

  FakeBackend backend;
  require_equal(probe.classify(backend) == aw::ContextId::Unknown, true, "out of range");
}

void tests_missing_file_reports_error_and_leaves_table_empty() {
  aw::SymbolTable table;
  std::string err;
  const bool ok = table.load_from_file("data/symbols/definitely-not-here.ini", err);
  require_equal(ok, false, "missing file fails");
  require_equal(err.empty(), false, "error message set");
  require_equal(table.contexts.empty(), true, "no contexts loaded");
}

}  // namespace

int main() {
  try {
    tests_classifies_by_predicate();
    tests_unmatched_value_is_unknown();
    tests_empty_table_is_unknown_but_steerable();
    tests_rule_lookup_returns_the_matching_rule();
    tests_rom_matching_is_case_insensitive();
    tests_predicate_outside_ewram_never_matches();
    tests_missing_file_reports_error_and_leaves_table_empty();
    std::cout << "context_probe_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
```

- [ ] **Step 2: Register and run it to verify it fails**

Append to `runtime/CMakeLists.txt` before the `advance-wars-native` block:

```cmake
add_executable(context_probe_tests
  tests/context_probe_tests.cpp
)

target_include_directories(context_probe_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(context_probe_tests PRIVATE aw_runtime)

add_test(NAME context_probe_tests COMMAND context_probe_tests)
```

Run: `cmake --build build/native --target context_probe_tests`
Expected: FAIL — `aw/probe/context.hpp: No such file or directory`

- [ ] **Step 3: Write the header**

Create `runtime/include/aw/probe/context.hpp`:

```c++
#pragma once

#include "aw/probe/backend.hpp"
#include "aw/probe/oam_tracker.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aw {

enum class ContextId : std::uint8_t {
  Unknown,   // Not recognised: correlation tracking, steering allowed
  MapView,   // The gameplay tile grid
  ListMenu,  // Unit action menus, in-game menu, options
  NameEntry, // Letter grid
  FrontEnd,  // Title, CO select, map select, save slots
  Cutscene,  // Dialogue and animation: clicks only, no steering
};

const char* context_name(ContextId id);

// How to drive one UI context. Everything here is mined data, not code, so
// adding a context costs a data edit rather than a rebuild.
struct ContextRule {
  struct Predicate {
    std::uint32_t addr = 0;  // Absolute EWRAM address
    std::uint8_t value = 0;
  };

  ContextId id = ContextId::Unknown;
  std::vector<Predicate> predicates;  // All must match
  IndicatorSignature signature{};     // Wildcard means "use correlation"
  int scroll_bg = -1;                 // BG layer tracking content, -1 = none
  bool steerable = true;
};

struct SymbolTable {
  std::string rom_sha1;
  std::vector<ContextRule> contexts;

  // Loads the INI form described in data/symbols/README.md. Returns false and
  // sets `err` on failure, leaving the table untouched.
  bool load_from_file(const std::string& path, std::string& err);

  bool matches_rom(const std::string& sha1) const;
};

// Classifies the active UI context by evaluating mined predicates against
// EWRAM. With no table, everything is Unknown, which is steerable with a
// wildcard signature so correlation mode still works.
class ContextProbe {
public:
  void set_table(SymbolTable table);
  const SymbolTable& table() const { return table_; }

  ContextId classify(ProbeBackend& backend) const;

  // Never returns null; unmatched ids resolve to a permissive default.
  const ContextRule& rule_for(ContextId id) const;

private:
  SymbolTable table_;
  ContextRule default_rule_{};  // Unknown, wildcard signature, steerable
};

}  // namespace aw
```

- [ ] **Step 4: Write the implementation**

Create `runtime/src/probe/context.cpp`:

```c++
#include "aw/probe/context.hpp"

#include "aw/config_file.hpp"

#include <algorithm>
#include <cctype>

namespace aw {

namespace {

constexpr std::uint32_t kEwramBase = 0x02000000;

std::string to_upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
}

ContextId context_from_name(const std::string& name) {
  if (name == "MapView") return ContextId::MapView;
  if (name == "ListMenu") return ContextId::ListMenu;
  if (name == "NameEntry") return ContextId::NameEntry;
  if (name == "FrontEnd") return ContextId::FrontEnd;
  if (name == "Cutscene") return ContextId::Cutscene;
  return ContextId::Unknown;
}

}  // namespace

const char* context_name(ContextId id) {
  switch (id) {
    case ContextId::MapView:   return "MapView";
    case ContextId::ListMenu:  return "ListMenu";
    case ContextId::NameEntry: return "NameEntry";
    case ContextId::FrontEnd:  return "FrontEnd";
    case ContextId::Cutscene:  return "Cutscene";
    case ContextId::Unknown:   break;
  }
  return "Unknown";
}

bool SymbolTable::matches_rom(const std::string& sha1) const {
  return !rom_sha1.empty() && to_upper(rom_sha1) == to_upper(sha1);
}

bool SymbolTable::load_from_file(const std::string& path, std::string& err) {
  ConfigFile file;
  if (!file.load(path)) {
    err = "cannot open symbol table: " + path;
    return false;
  }

  const std::string sha1 = file.get_string("Rom", "sha1", "");
  if (sha1.empty()) {
    err = "symbol table has no [Rom] sha1: " + path;
    return false;
  }

  SymbolTable parsed;
  parsed.rom_sha1 = sha1;

  static const char* kNames[] = {"MapView", "ListMenu", "NameEntry", "FrontEnd", "Cutscene"};
  for (const char* name : kNames) {
    // A context is present when it declares at least a predicate address.
    const int addr = file.get_int(name, "predicate_addr", 0);
    if (addr == 0) continue;

    ContextRule rule;
    rule.id = context_from_name(name);
    rule.predicates.push_back({static_cast<std::uint32_t>(addr),
                               static_cast<std::uint8_t>(file.get_int(name, "predicate_value", 0))});

    const int addr2 = file.get_int(name, "predicate2_addr", 0);
    if (addr2 != 0) {
      rule.predicates.push_back({static_cast<std::uint32_t>(addr2),
                                 static_cast<std::uint8_t>(file.get_int(name, "predicate2_value", 0))});
    }

    rule.signature.tile = file.get_int(name, "indicator_tile", -1);
    rule.signature.palette = file.get_int(name, "indicator_palette", -1);
    rule.scroll_bg = file.get_int(name, "scroll_bg", -1);
    rule.steerable = file.get_int(name, "steerable", 1) != 0;
    parsed.contexts.push_back(rule);
  }

  if (parsed.contexts.empty()) {
    err = "symbol table declares no contexts: " + path;
    return false;
  }

  *this = std::move(parsed);
  return true;
}

void ContextProbe::set_table(SymbolTable table) {
  table_ = std::move(table);
}

ContextId ContextProbe::classify(ProbeBackend& backend) const {
  if (table_.contexts.empty()) return ContextId::Unknown;

  std::size_t size = 0;
  const std::uint8_t* ewram = backend.ewram(size);
  if (ewram == nullptr || size == 0) return ContextId::Unknown;

  for (const ContextRule& rule : table_.contexts) {
    if (rule.predicates.empty()) continue;

    bool all_match = true;
    for (const ContextRule::Predicate& p : rule.predicates) {
      if (p.addr < kEwramBase) { all_match = false; break; }
      const std::uint32_t offset = p.addr - kEwramBase;
      if (offset >= size) { all_match = false; break; }
      if (ewram[offset] != p.value) { all_match = false; break; }
    }
    if (all_match) return rule.id;
  }
  return ContextId::Unknown;
}

const ContextRule& ContextProbe::rule_for(ContextId id) const {
  for (const ContextRule& rule : table_.contexts) {
    if (rule.id == id) return rule;
  }
  return default_rule_;
}

}  // namespace aw
```

- [ ] **Step 5: Run the tests**

Add `src/probe/context.cpp` to the `aw_runtime` source list.

Run: `cmake --build build/native --target context_probe_tests && ctest --test-dir build/native -R context_probe_tests --output-on-failure`
Expected: PASS

- [ ] **Step 6: Document the symbol table format**

Create `data/symbols/README.md`:

```markdown
# Symbol tables

One INI file per ROM revision, named `<rom-sha1-lowercase>.ini`. The runtime
loads the file matching the ROM it booted and ignores any other. A missing or
mismatched file is not an error: the runtime falls back to correlation-based
indicator tracking, which needs no symbols.

INI rather than JSON because the project already ships `aw::ConfigFile` and
adding a JSON dependency for one small data file is not worth it.

## Format

    [Rom]
    sha1 = 15053499D5B3F49128A941D7F2D84876F5424D0C

    [MapView]
    predicate_addr   = 33556224   ; decimal EWRAM address (0x02000100)
    predicate_value  = 3
    predicate2_addr  = 0          ; optional second predicate, 0 = unused
    predicate2_value = 0
    indicator_tile    = 64        ; -1 = match any tile
    indicator_palette = -1        ; -1 = match any palette bank
    scroll_bg         = 1         ; BG layer whose scroll tracks content, -1 = none
    steerable         = 1         ; 0 disables pointer steering in this context

Recognised section names: `MapView`, `ListMenu`, `NameEntry`, `FrontEnd`,
`Cutscene`. A section with `predicate_addr = 0` (or absent) is skipped.

Addresses are decimal because `ConfigFile::get_int` does not parse hex. Put the
hex value in a trailing comment.

Generate these with `aw-symbol-miner` (see `tools/`).
```

- [ ] **Step 7: Commit**

```bash
git add runtime/include/aw/probe/context.hpp runtime/src/probe/context.cpp \
        runtime/tests/context_probe_tests.cpp data/symbols/README.md \
        runtime/CMakeLists.txt
git commit -m "feat: add symbol table loading and context classification

Unknown context stays steerable with a wildcard signature so a missing or
mismatched symbol table degrades to correlation tracking rather than
disabling the mouse."
```

---

### Task 7: `PointerNav` — the closed-loop steering controller

The heart of the fix, and a pure function so it is fully testable. This is the regression suite for every bug in the `47d0245`..`1d58783` commit range.

**Files:**
- Create: `runtime/include/aw/nav/pointer_nav.hpp`
- Create: `runtime/src/nav/pointer_nav.cpp`
- Create: `runtime/tests/pointer_nav_tests.cpp`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Consumes: `aw::kKey*`, `aw::kDpadMask` (Task 2).
- Produces: `aw::NavConfig { int blocked_frames; int snap_radius; int release_frames; }`, `aw::NavInput`, `aw::NavOutput { std::uint16_t keys; }`, `aw::PointerNav` with `explicit PointerNav(NavConfig)`, `NavOutput step(const NavInput&)`, `void reset()`, `bool steering() const`.

- [ ] **Step 1: Write the failing test**

Create `runtime/tests/pointer_nav_tests.cpp`:

```c++
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/hardware.hpp"
#include "aw/nav/pointer_nav.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

// A stand-in for the game: the indicator moves 16 px in the commanded
// direction, but only after the key has been held for `latency` frames, and
// only if `frozen` is false.
struct FakeGame {
  int x = 0;
  int y = 0;
  int scroll_x = 0;
  int scroll_y = 0;
  int latency = 1;
  bool frozen = false;
  int held = 0;
  std::uint16_t last_dir = 0;

  void apply(std::uint16_t keys) {
    const std::uint16_t dir = keys & aw::kDpadMask;
    if (dir == 0 || dir != last_dir) {
      held = 0;
      last_dir = dir;
      if (dir == 0) return;
    }
    ++held;
    if (frozen || held < latency) return;
    held = 0;
    if (dir & aw::kKeyRight) x += 16;
    if (dir & aw::kKeyLeft) x -= 16;
    if (dir & aw::kKeyDown) y += 16;
    if (dir & aw::kKeyUp) y -= 16;
  }
};

aw::NavInput make_input(const FakeGame& game, int target_x, int target_y) {
  aw::NavInput in;
  in.armed_pointer = true;
  in.steerable = true;
  in.target_x = target_x;
  in.target_y = target_y;
  in.indicator_found = true;
  in.indicator_x = game.x;
  in.indicator_y = game.y;
  in.scroll_x = game.scroll_x;
  in.scroll_y = game.scroll_y;
  return in;
}

void tests_converges_on_the_target() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 16;
  game.y = 16;

  const int target_x = 128;
  const int target_y = 96;

  int frames = 0;
  for (; frames < 300; ++frames) {
    const aw::NavOutput out = nav.step(make_input(game, target_x, target_y));
    game.apply(out.keys);
    if (std::abs(game.x - target_x) <= 8 && std::abs(game.y - target_y) <= 8) break;
  }

  require_equal(frames < 300, true, "converged within the frame budget");
  require_equal(std::abs(game.x - target_x) <= 8, true, "x within snap radius");
  require_equal(std::abs(game.y - target_y) <= 8, true, "y within snap radius");
}

void tests_deadband_emits_nothing() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 100;
  game.y = 100;

  // Target 4 px away on both axes: inside the 8 px snap radius.
  const aw::NavOutput out = nav.step(make_input(game, 104, 96));
  require_equal(out.keys & aw::kDpadMask, std::uint16_t{0}, "no dpad inside deadband");
}

void tests_releases_between_steps() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 0;
  game.y = 0;

  bool saw_release_after_motion = false;
  int last_x = game.x;
  bool motion_last_frame = false;

  for (int i = 0; i < 40; ++i) {
    const aw::NavOutput out = nav.step(make_input(game, 200, 0));
    if (motion_last_frame && (out.keys & aw::kDpadMask) == 0) {
      saw_release_after_motion = true;
    }
    game.apply(out.keys);
    motion_last_frame = (game.x != last_x);
    last_x = game.x;
  }

  require_equal(saw_release_after_motion, true, "released for a frame after each step");
}

void tests_blocked_axis_stops_emitting() {
  aw::NavConfig cfg;
  cfg.blocked_frames = 4;
  aw::PointerNav nav(cfg);

  FakeGame game;
  game.frozen = true;  // The game never responds.
  game.x = 0;

  // Press for longer than blocked_frames.
  for (int i = 0; i < cfg.blocked_frames + 2; ++i) {
    const aw::NavOutput out = nav.step(make_input(game, 200, 0));
    game.apply(out.keys);
  }

  // Once blocked, it must stay quiet rather than spamming the core.
  for (int i = 0; i < 10; ++i) {
    const aw::NavOutput out = nav.step(make_input(game, 200, 0));
    require_equal(out.keys & aw::kDpadMask, std::uint16_t{0}, "blocked axis stays quiet");
  }
}

void tests_blocked_axis_recovers_when_target_reverses() {
  aw::NavConfig cfg;
  cfg.blocked_frames = 4;
  aw::PointerNav nav(cfg);

  FakeGame game;
  game.frozen = true;
  game.x = 100;

  for (int i = 0; i < cfg.blocked_frames + 2; ++i) {
    nav.step(make_input(game, 200, 100));  // Steering right, blocked.
  }

  // Target moves to the other side: the axis must try again.
  const aw::NavOutput out = nav.step(make_input(game, 0, 100));
  require_equal((out.keys & aw::kKeyLeft) != 0, true, "retries after direction change");
}

void tests_scroll_counts_as_motion() {
  aw::NavConfig cfg;
  cfg.blocked_frames = 4;
  aw::PointerNav nav(cfg);

  FakeGame game;
  game.x = 100;
  game.y = 100;

  // The indicator never moves on screen, but the camera scrolls: the game IS
  // responding, so the axis must never be declared blocked. Count presses over
  // a long run rather than sampling one frame — the controller is legitimately
  // silent during its release frames.
  int presses = 0;
  for (int i = 0; i < 30; ++i) {
    const aw::NavOutput out = nav.step(make_input(game, 220, 100));
    if (out.keys & aw::kKeyRight) {
      ++presses;
      game.scroll_x += 16;
    }
  }

  // A blocked axis would have fallen silent after about blocked_frames presses.
  require_equal(presses > cfg.blocked_frames, true, "scrolling is not treated as blocked");
}

void tests_physical_dpad_disarms_steering() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 0;

  aw::NavInput in = make_input(game, 200, 0);
  in.device_dpad = aw::kKeyUp;  // The player touched the keyboard or pad.

  const aw::NavOutput out = nav.step(in);
  require_equal(out.keys & aw::kDpadMask, std::uint16_t{0}, "device dpad wins");
  require_equal(nav.steering(), false, "steering disarmed");
}

void tests_unarmed_pointer_emits_nothing() {
  aw::PointerNav nav;
  FakeGame game;

  aw::NavInput in = make_input(game, 200, 0);
  in.armed_pointer = false;

  const aw::NavOutput out = nav.step(in);
  require_equal(out.keys, std::uint16_t{0}, "unarmed pointer is silent");
}

void tests_missing_indicator_emits_nothing() {
  aw::PointerNav nav;
  FakeGame game;

  aw::NavInput in = make_input(game, 200, 0);
  in.indicator_found = false;

  const aw::NavOutput out = nav.step(in);
  require_equal(out.keys & aw::kDpadMask, std::uint16_t{0}, "no indicator, no steering");
}

void tests_unsteerable_context_emits_no_dpad_but_still_clicks() {
  aw::PointerNav nav;
  FakeGame game;

  aw::NavInput in = make_input(game, 200, 0);
  in.steerable = false;
  in.primary_edge = true;

  const aw::NavOutput out = nav.step(in);
  require_equal(out.keys & aw::kDpadMask, std::uint16_t{0}, "cutscene does not steer");
  require_equal((out.keys & aw::kKeyA) != 0, true, "cutscene still clicks");
}

void tests_click_edges_map_to_a_and_b() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 100;
  game.y = 100;

  aw::NavInput in = make_input(game, 100, 100);
  in.primary_edge = true;
  require_equal((nav.step(in).keys & aw::kKeyA) != 0, true, "left click is A");

  in.primary_edge = false;
  in.secondary_edge = true;
  require_equal((nav.step(in).keys & aw::kKeyB) != 0, true, "right click is B");
}

void tests_clicks_work_even_when_unarmed() {
  aw::PointerNav nav;
  FakeGame game;

  aw::NavInput in = make_input(game, 100, 100);
  in.armed_pointer = false;
  in.primary_edge = true;

  require_equal((nav.step(in).keys & aw::kKeyA) != 0, true, "click without motion");
}

void tests_reset_clears_state() {
  aw::NavConfig cfg;
  cfg.blocked_frames = 2;
  aw::PointerNav nav(cfg);

  FakeGame game;
  game.frozen = true;
  for (int i = 0; i < 5; ++i) nav.step(make_input(game, 200, 0));

  nav.reset();

  const aw::NavOutput out = nav.step(make_input(game, 200, 0));
  require_equal((out.keys & aw::kKeyRight) != 0, true, "reset clears the blocked axis");
}

}  // namespace

int main() {
  try {
    tests_converges_on_the_target();
    tests_deadband_emits_nothing();
    tests_releases_between_steps();
    tests_blocked_axis_stops_emitting();
    tests_blocked_axis_recovers_when_target_reverses();
    tests_scroll_counts_as_motion();
    tests_physical_dpad_disarms_steering();
    tests_unarmed_pointer_emits_nothing();
    tests_missing_indicator_emits_nothing();
    tests_unsteerable_context_emits_no_dpad_but_still_clicks();
    tests_click_edges_map_to_a_and_b();
    tests_clicks_work_even_when_unarmed();
    tests_reset_clears_state();
    std::cout << "pointer_nav_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
```

Add `#include <cstdlib>` at the top for `std::abs`.

- [ ] **Step 2: Register and run it to verify it fails**

Append to `runtime/CMakeLists.txt` before the `advance-wars-native` block:

```cmake
add_executable(pointer_nav_tests
  tests/pointer_nav_tests.cpp
)

target_include_directories(pointer_nav_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(pointer_nav_tests PRIVATE aw_runtime)

add_test(NAME pointer_nav_tests COMMAND pointer_nav_tests)
```

Run: `cmake --build build/native --target pointer_nav_tests`
Expected: FAIL — `aw/nav/pointer_nav.hpp: No such file or directory`

- [ ] **Step 3: Write the header**

Create `runtime/include/aw/nav/pointer_nav.hpp`:

```c++
#pragma once

#include <cstdint>

namespace aw {

struct NavConfig {
  // Frames of unanswered pressing before an axis is declared blocked.
  int blocked_frames = 8;
  // Error, in screen pixels, that counts as "arrived". Half a 16 px tile.
  int snap_radius = 8;
  // Frames to release between steps. The game needs a gap to register a
  // second discrete move.
  int release_frames = 1;
};

// Everything the controller needs for one frame. All positions are GBA screen
// pixels; scroll is the BG scroll offset, used only to tell whether the game
// responded (a camera scroll is a response even when the sprite holds still).
struct NavInput {
  bool armed_pointer = false;  // Pointer is present, in the viewport, and has moved
  int target_x = 0;
  int target_y = 0;

  bool indicator_found = false;
  int indicator_x = 0;
  int indicator_y = 0;

  int scroll_x = 0;
  int scroll_y = 0;

  bool steerable = true;           // False in cutscenes: clicks only
  std::uint16_t device_dpad = 0;   // D-pad from a physical device this frame
  bool primary_edge = false;       // Left button / touch press edge
  bool secondary_edge = false;     // Right button press edge
};

struct NavOutput {
  std::uint16_t keys = 0;
};

// Closed-loop pointer steering.
//
// The predecessor was open-loop: it assumed each emitted D-pad pulse moved the
// cursor, so dropped inputs during animations desynced its model permanently.
// This controller only believes the indicator's observed position, so there is
// no model to desync.
class PointerNav {
public:
  PointerNav() = default;
  explicit PointerNav(NavConfig cfg) : cfg_(cfg) {}

  NavOutput step(const NavInput& in);
  void reset();

  // True while the pointer owns the D-pad.
  bool steering() const { return armed_; }

private:
  enum class Phase : std::uint8_t { Idle, Pressing, Releasing, Blocked };

  struct Axis {
    Phase phase = Phase::Idle;
    std::uint16_t dir = 0;   // Key mask currently being pressed
    int press_frames = 0;
    int release_frames = 0;
    int world_at_press = 0;  // Indicator + scroll when the press began
  };

  std::uint16_t drive_axis(Axis& axis, int error, int world,
                           std::uint16_t positive, std::uint16_t negative);

  NavConfig cfg_{};
  Axis x_{};
  Axis y_{};
  bool armed_ = false;
};

}  // namespace aw
```

- [ ] **Step 4: Write the implementation**

Create `runtime/src/nav/pointer_nav.cpp`:

```c++
#include "aw/nav/pointer_nav.hpp"

#include "aw/hardware.hpp"

#include <cstdlib>

namespace aw {

void PointerNav::reset() {
  x_ = {};
  y_ = {};
  armed_ = false;
}

std::uint16_t PointerNav::drive_axis(Axis& axis, int error, int world,
                                     std::uint16_t positive, std::uint16_t negative) {
  // Arrived: stand down.
  if (std::abs(error) <= cfg_.snap_radius) {
    axis.phase = Phase::Idle;
    axis.dir = 0;
    return 0;
  }

  const std::uint16_t wanted = (error > 0) ? positive : negative;

  // A blocked axis retries only when the direction we want changes.
  if (axis.phase == Phase::Blocked) {
    if (wanted == axis.dir) return 0;
    axis.phase = Phase::Idle;
  }

  // Changing direction mid-press restarts the press.
  if (axis.phase == Phase::Pressing && wanted != axis.dir) {
    axis.phase = Phase::Idle;
  }

  switch (axis.phase) {
    case Phase::Idle:
      axis.phase = Phase::Pressing;
      axis.dir = wanted;
      axis.press_frames = 0;
      axis.world_at_press = world;
      return wanted;

    case Phase::Pressing:
      if (world != axis.world_at_press) {
        // The game responded. Release for a frame so the next press registers
        // as a distinct move.
        axis.phase = Phase::Releasing;
        axis.release_frames = 0;
        return 0;
      }
      if (++axis.press_frames >= cfg_.blocked_frames) {
        axis.phase = Phase::Blocked;
        return 0;
      }
      return axis.dir;

    case Phase::Releasing:
      if (++axis.release_frames >= cfg_.release_frames) {
        axis.phase = Phase::Idle;
      }
      return 0;

    case Phase::Blocked:
      return 0;
  }
  return 0;
}

NavOutput PointerNav::step(const NavInput& in) {
  NavOutput out;

  // Clicks are unconditional: they work in unrecognised contexts, in
  // cutscenes, and before the pointer has armed.
  if (in.primary_edge) out.keys |= kKeyA;
  if (in.secondary_edge) out.keys |= kKeyB;

  // A physical D-pad always wins, and hands control back to the player.
  if ((in.device_dpad & kDpadMask) != 0) {
    armed_ = false;
    x_ = {};
    y_ = {};
    return out;
  }

  if (!in.armed_pointer || !in.steerable || !in.indicator_found) {
    return out;
  }

  armed_ = true;

  // The scroll offset cancels out of the error (both points share it), so it
  // is needed only for the "did the game respond" test below.
  const int error_x = in.target_x - in.indicator_x;
  const int error_y = in.target_y - in.indicator_y;

  out.keys |= drive_axis(x_, error_x, in.indicator_x + in.scroll_x, kKeyRight, kKeyLeft);
  out.keys |= drive_axis(y_, error_y, in.indicator_y + in.scroll_y, kKeyDown, kKeyUp);
  return out;
}

}  // namespace aw
```

- [ ] **Step 5: Run the tests**

Add `src/nav/pointer_nav.cpp` to the `aw_runtime` source list.

Run: `cmake --build build/native --target pointer_nav_tests && ctest --test-dir build/native -R pointer_nav_tests --output-on-failure`
Expected: PASS

If `tests_converges_on_the_target` fails to converge, the likely cause is the release phase and the fake game's latency interacting so no press ever lasts long enough. Print the emitted keys per frame to see the pattern before changing the controller.

- [ ] **Step 6: Commit**

```bash
git add runtime/include/aw/nav runtime/src/nav runtime/tests/pointer_nav_tests.cpp \
        runtime/CMakeLists.txt
git commit -m "feat: add closed-loop PointerNav steering controller

Replaces the open-loop model that could not recover from inputs the game
dropped during animations. Presses until observed motion, releases for a
frame, and declares an axis blocked rather than spamming the core. Treats a
camera scroll as a response so edge scrolling is not mistaken for a block."
```

---

### Task 8: Win32 input source and wiring — the mouse starts working

Everything built so far is joined up behind the existing `config.ini` `mouse_enabled` flag.

**Files:**
- Create: `runtime/include/aw/input/source_win32.hpp`
- Create: `runtime/src/input/source_win32.cpp`
- Create: `runtime/include/aw/nav/nav_controller.hpp`
- Create: `runtime/src/nav/nav_controller.cpp`
- Modify: `runtime/include/aw/window.hpp`
- Modify: `runtime/src/window.cpp`
- Modify: `runtime/src/main.cpp`
- Modify: `runtime/src/input_config.cpp` (default `mouse_enabled` back on)
- Modify: `config.ini`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Consumes: `aw::InputFrame`, `aw::InputSource`, `aw::viewport_to_gba` (Task 2); `aw::MgbaProbeBackend` (Task 3); `aw::OamTracker` (Task 5); `aw::ContextProbe`, `aw::SymbolTable` (Task 6); `aw::PointerNav` (Task 7); `aw::InputMapping` from `aw/input_config.hpp`.
- Produces: `aw::Win32InputSource` with `set_window(void* hwnd)`, `set_viewport(int,int,int,int)`, `set_mapping(const InputMapping*)`, `poll(InputFrame&)`; and `aw::NavController` with `set_core(void*)`, `load_symbols(const std::string& rom_sha1)`, `std::uint16_t update(const InputFrame&)`, `reset()`.

- [ ] **Step 1: Write the Win32 source**

Create `runtime/include/aw/input/source_win32.hpp`:

```c++
#pragma once

#include "aw/input/input_source.hpp"
#include "aw/input_config.hpp"

namespace aw {

// Keyboard, mouse and XInput on Windows. Deleted in Spec 2 when the SDL3
// source replaces it; nothing downstream changes when it goes.
class Win32InputSource final : public InputSource {
public:
  void set_window(void* hwnd) { hwnd_ = hwnd; }
  void set_mapping(const InputMapping* mapping) { mapping_ = mapping; }

  // The game viewport in window client coordinates, refreshed each frame by
  // the renderer so letterboxing stays correct after a resize.
  void set_viewport(int x, int y, int width, int height) {
    vp_x_ = x;
    vp_y_ = y;
    vp_w_ = width;
    vp_h_ = height;
  }

  void poll(InputFrame& frame) override;

private:
  void* hwnd_ = nullptr;
  const InputMapping* mapping_ = nullptr;
  int vp_x_ = 0, vp_y_ = 0, vp_w_ = 0, vp_h_ = 0;

  bool has_last_pos_ = false;
  int last_gba_x_ = 0;
  int last_gba_y_ = 0;
  bool last_primary_ = false;
  bool last_secondary_ = false;
};

}  // namespace aw
```

Create `runtime/src/input/source_win32.cpp`:

```c++
#include "aw/input/source_win32.hpp"

#ifdef _WIN32

#include "aw/input/viewport.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xinput.h>

#include <cstring>

namespace aw {

void Win32InputSource::poll(InputFrame& frame) {
  if (mapping_ == nullptr) return;

  // 1. Keyboard.
  for (int i = 0; i < Gba_Count; ++i) {
    const std::uint32_t vk = mapping_->bindings[i].key_vk;
    if (vk != 0 && (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000)) {
      frame.gba_keys |= static_cast<std::uint16_t>(1u << i);
    }
  }

  // 2. XInput gamepad. Covers Xbox pads, Retroid handhelds and anything else
  //    exposing the XInput interface.
  const int ctrl_idx = mapping_->controller_index;
  if (ctrl_idx >= 0 && ctrl_idx < 4) {
    XINPUT_STATE xstate;
    std::memset(&xstate, 0, sizeof(XINPUT_STATE));
    if (XInputGetState(static_cast<DWORD>(ctrl_idx), &xstate) == ERROR_SUCCESS) {
      const WORD btns = xstate.Gamepad.wButtons;
      for (int i = 0; i < Gba_Count; ++i) {
        const std::uint16_t pad_mask = mapping_->bindings[i].pad_button;
        if (pad_mask != 0 && (btns & pad_mask)) {
          frame.gba_keys |= static_cast<std::uint16_t>(1u << i);
        }
      }
      constexpr SHORT kDeadZone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
      if (xstate.Gamepad.sThumbLY > kDeadZone) frame.gba_keys |= kKeyUp;
      if (xstate.Gamepad.sThumbLY < -kDeadZone) frame.gba_keys |= kKeyDown;
      if (xstate.Gamepad.sThumbLX < -kDeadZone) frame.gba_keys |= kKeyLeft;
      if (xstate.Gamepad.sThumbLX > kDeadZone) frame.gba_keys |= kKeyRight;
    }
  }

  // Whatever D-pad we have so far came from a physical device. Pointer
  // navigation uses this to hand control back to the player.
  frame.device_dpad |= static_cast<std::uint16_t>(frame.gba_keys & kDpadMask);

  // 3. Mouse, as a pointer.
  if (!mapping_->mouse_enabled || hwnd_ == nullptr) return;
  if (frame.pointer_count >= kMaxPointers) return;

  POINT pos;
  if (!GetCursorPos(&pos)) return;
  if (!ScreenToClient(static_cast<HWND>(hwnd_), &pos)) return;

  PointerState& p = frame.pointers[frame.pointer_count];
  p.kind = PointerKind::Mouse;

  int gba_x = 0, gba_y = 0;
  p.in_viewport = viewport_to_gba(vp_x_, vp_y_, vp_w_, vp_h_, pos.x, pos.y, gba_x, gba_y);
  if (p.in_viewport) {
    p.gba_x = gba_x;
    p.gba_y = gba_y;
    p.moved = !has_last_pos_ || gba_x != last_gba_x_ || gba_y != last_gba_y_;
    last_gba_x_ = gba_x;
    last_gba_y_ = gba_y;
    has_last_pos_ = true;
  }

  const bool primary = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  const bool secondary = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
  p.primary_down = primary;
  p.secondary_down = secondary;
  // Edges are tracked even outside the viewport so a press that starts on the
  // menu bar cannot leave a stuck button.
  p.primary_edge = primary && !last_primary_ && p.in_viewport;
  p.secondary_edge = secondary && !last_secondary_ && p.in_viewport;
  last_primary_ = primary;
  last_secondary_ = secondary;

  ++frame.pointer_count;
}

}  // namespace aw

#endif  // _WIN32
```

- [ ] **Step 2: Write the controller that joins probe, tracker and nav**

Create `runtime/include/aw/nav/nav_controller.hpp`:

```c++
#pragma once

#include "aw/input/input_frame.hpp"
#include "aw/nav/pointer_nav.hpp"
#include "aw/probe/backend_mgba.hpp"
#include "aw/probe/context.hpp"
#include "aw/probe/oam_tracker.hpp"

#include <cstdint>
#include <string>

namespace aw {

// Owns the probe, tracker and steering controller, and runs them in the right
// order once per frame. This is the only piece that knows the frame ordering:
// the tracker must see the OAM that resulted from the keys we emitted last
// frame, so update() correlates against the previous frame's output.
class NavController {
public:
  void set_core(void* core);
  void reset();

  // Loads data/symbols/<rom_sha1>.ini if present. Missing or mismatched is
  // fine: tracking falls back to correlation and steering still works.
  void load_symbols(const std::string& rom_sha1);

  // Returns extra GBA keys to OR into the frame's keys before running the
  // emulator. Call once per frame, after the input sources have polled.
  std::uint16_t update(const InputFrame& frame);

  ContextId context() const { return context_; }
  bool indicator_found() const { return indicator_.found; }

private:
  MgbaProbeBackend backend_;
  ContextProbe context_probe_;
  OamTracker tracker_;
  PointerNav nav_;

  ContextId context_ = ContextId::Unknown;
  Indicator indicator_{};
  std::uint16_t last_emitted_dpad_ = 0;
  bool symbols_loaded_ = false;
};

}  // namespace aw
```

Create `runtime/src/nav/nav_controller.cpp`:

```c++
#include "aw/nav/nav_controller.hpp"

#include "aw/hardware.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

namespace aw {

namespace {

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

}  // namespace

void NavController::set_core(void* core) {
  backend_.set_core(core);
  reset();
}

void NavController::reset() {
  tracker_.reset();
  nav_.reset();
  context_ = ContextId::Unknown;
  indicator_ = {};
  last_emitted_dpad_ = 0;
}

void NavController::load_symbols(const std::string& rom_sha1) {
  const std::string path = "data/symbols/" + to_lower(rom_sha1) + ".ini";
  SymbolTable table;
  std::string err;
  if (!table.load_from_file(path, err)) {
    std::cout << "[nav] no symbol table (" << err
              << "); using correlation tracking\n";
    symbols_loaded_ = false;
    return;
  }
  if (!table.matches_rom(rom_sha1)) {
    std::cout << "[nav] symbol table ROM mismatch; using correlation tracking\n";
    symbols_loaded_ = false;
    return;
  }
  context_probe_.set_table(std::move(table));
  symbols_loaded_ = true;
  std::cout << "[nav] loaded symbol table " << path << "\n";
}

std::uint16_t NavController::update(const InputFrame& frame) {
  if (!backend_.available()) return 0;

  // The OAM we are about to read is the result of last frame's keys, so
  // correlation is scored against last frame's emitted D-pad.
  indicator_ = tracker_.update(backend_.oam(), last_emitted_dpad_);
  context_ = context_probe_.classify(backend_);
  const ContextRule& rule = context_probe_.rule_for(context_);

  // A mined signature beats correlation; a wildcard leaves the tracker to
  // correlate for itself.
  if (!rule.signature.wildcard()) {
    tracker_.set_signature(rule.signature);
  }

  NavInput in;
  in.steerable = rule.steerable;
  in.device_dpad = frame.device_dpad;

  if (const PointerState* p = frame.primary_pointer()) {
    in.armed_pointer = p->in_viewport && p->moved;
    in.target_x = p->gba_x;
    in.target_y = p->gba_y;
    in.primary_edge = p->primary_edge;
    in.secondary_edge = p->secondary_edge;
  }

  in.indicator_found = indicator_.found;
  in.indicator_x = indicator_.screen_x;
  in.indicator_y = indicator_.screen_y;

  if (rule.scroll_bg >= 0) {
    in.scroll_x = backend_.read_io16(bg_hofs_reg(rule.scroll_bg));
    in.scroll_y = backend_.read_io16(bg_vofs_reg(rule.scroll_bg));
  }

  const NavOutput out = nav_.step(in);
  last_emitted_dpad_ = static_cast<std::uint16_t>(out.keys & kDpadMask);
  return out.keys;
}

}  // namespace aw
```

- [ ] **Step 3: Route `Window` through the new input source**

In `runtime/include/aw/window.hpp`, add the include and members:

```c++
#include "aw/input/input_frame.hpp"
#include "aw/input/source_win32.hpp"
```

Add a public accessor and a private member:

```c++
  // The neutral frame produced by this window's input sources each poll.
  const InputFrame& input_frame() const { return input_frame_; }
```

```c++
  Win32InputSource win32_input_;
  InputFrame input_frame_;
```

In `runtime/src/window.cpp`, replace the body of `Window::process_events` from the end of the message pump to the `return` with:

```c++
  input_frame_.clear();
  win32_input_.set_window(hwnd_);
  win32_input_.set_mapping(&input_mapping_);
  win32_input_.set_viewport(cached_viewport_.x, cached_viewport_.y,
                            cached_viewport_.width, cached_viewport_.height);
  win32_input_.poll(input_frame_);

  hardware.keys_pressed |= input_frame_.gba_keys;
  return is_open_;
```

This deletes the inline keyboard and XInput blocks; `Win32InputSource::poll` now owns them.

- [ ] **Step 4: Drive the controller from the game loop**

In `runtime/src/main.cpp`, add:

```c++
#include "aw/nav/nav_controller.hpp"
```

Inside `run_game_loop`, after the core is created:

```c++
    aw::NavController nav;
    nav.set_core(core);
    nav.load_symbols(aw::sha1_hex(rom.bytes));
```

`sha1_hex` is a free function declared in `runtime/include/aw/rom.hpp` — `RomImage` has no accessor of its own — and `main.cpp` already includes that header.

In the ROM-switch branch, next to the existing `core = aw_mgba_create(...)`:

```c++
        nav.set_core(core);
```

Immediately before `aw_mgba_run_frame(core, hardware.keys_pressed);`:

```c++
      hardware.keys_pressed |= nav.update(window.input_frame());
```

- [ ] **Step 5: Re-enable the mouse by default**

In `runtime/src/input_config.cpp`, in `InputMapping::reset_to_defaults`, change:

```c++
  mouse_enabled = false;
```

to:

```c++
  mouse_enabled = true;
```

In `config.ini`, ensure `mouse_enabled = 1` under `[Input]`.

- [ ] **Step 6: Build and verify the tests still pass**

Run:
```bash
cmake --build build/native --target advance-wars-native
ctest --test-dir build/native --output-on-failure
```
Expected: build clean, all tests pass.

Add `src/input/source_win32.cpp` and `src/nav/nav_controller.cpp` to the `aw_runtime` source list first.

- [ ] **Step 7: Verify against the real game**

Run: `build/native/runtime/advance-wars-native.exe "rom/Advance Wars (USA) (Rev 1).gba"`

Check, in order:
1. Keyboard and gamepad still work exactly as before.
2. In a mission, moving the mouse over the map makes the cursor walk to the tile under the pointer. It may take a moment on the first movement while correlation locks on.
3. Holding the pointer near a screen edge scrolls the camera.
4. Left click acts as A; right click as B.
5. Pressing an arrow key immediately returns control to the keyboard; the cursor stops chasing the mouse until the mouse moves again.
6. Repeat 2 in the action menu, the name entry grid, and the front-end screens.

Record anything that misbehaves. Common causes and their fixes:
- **Cursor chases a unit sprite instead of the cursor** — correlation locked onto the wrong sprite. Mine the signature (Task 9) and set `indicator_tile` for that context.
- **Cursor jitters around the target** — raise `snap_radius`.
- **Steering feels sluggish** — the indicator is moving less than expected per step; check the `--oam-log` output for the real step size.
- **Cursor drifts one cell past the target** — the game is applying a queued input; raise `release_frames` to 2.

- [ ] **Step 8: Commit**

```bash
git add runtime/include/aw/input/source_win32.hpp runtime/src/input/source_win32.cpp \
        runtime/include/aw/nav/nav_controller.hpp runtime/src/nav/nav_controller.cpp \
        runtime/include/aw/window.hpp runtime/src/window.cpp runtime/src/main.cpp \
        runtime/src/input_config.cpp config.ini runtime/CMakeLists.txt
git commit -m "feat: wire closed-loop pointer navigation into the game loop

Win32 keyboard, mouse and XInput now produce a neutral InputFrame, and
NavController runs probe, tracker and steering once per frame. Mouse
navigation is enabled by default again."
```

---

### Task 9: Symbol miner, savestate hotkey, and documentation

Produces the real symbol table so contexts are recognised, and records how to regenerate it.

**Files:**
- Create: `runtime/tools/symbol_miner.cpp`
- Modify: `runtime/include/aw/mgba_adapter.h`
- Modify: `runtime/src/mgba_adapter.c`
- Modify: `runtime/src/window.cpp` (F5/F6 hotkeys)
- Modify: `runtime/include/aw/window.hpp`
- Modify: `runtime/src/main.cpp` (honour the savestate request)
- Modify: `runtime/CMakeLists.txt`
- Modify: `README.md`
- Modify: `.gitignore`
- Create: `data/symbols/15053499d5b3f49128a941d7f2d84876f5424d0c.ini`

**Interfaces:**
- Consumes: `aw::MgbaProbeBackend` (Task 3), `aw::SymbolTable` INI schema (Task 6).
- Produces: `aw_mgba_save_state(struct mCore*, const char* path)`, `aw_mgba_load_state(struct mCore*, const char* path)`, `aw::Window::consume_savestate_request()`, and the `aw-symbol-miner` executable.

- [ ] **Step 1: Add savestate calls to the mGBA adapter**

Add to `runtime/include/aw/mgba_adapter.h`:

```c
// Savestates, used to capture UI contexts for the offline symbol miner.
int aw_mgba_save_state(struct mCore* core, const char* path);
int aw_mgba_load_state(struct mCore* core, const char* path);
```

Add to `runtime/src/mgba_adapter.c`. `VFileOpen(const char*, int)` takes fcntl flags, so add both includes alongside the existing mGBA ones:

```c
#include <mgba/core/serialize.h>
#include <fcntl.h>
```

Then the functions:

```c
int aw_mgba_save_state(struct mCore* core, const char* path) {
    if (!core || !path) return 0;
    struct VFile* vf = VFileOpen(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (!vf) return 0;
    const bool ok = mCoreSaveStateNamed(core, vf, SAVESTATE_SAVEDATA);
    vf->close(vf);
    return ok ? 1 : 0;
}

int aw_mgba_load_state(struct mCore* core, const char* path) {
    if (!core || !path) return 0;
    struct VFile* vf = VFileOpen(path, O_RDONLY);
    if (!vf) return 0;
    const bool ok = mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA);
    vf->close(vf);
    return ok ? 1 : 0;
}
```

- [ ] **Step 2: Add the capture hotkey**

In `runtime/include/aw/window.hpp`, add:

```c++
  // Non-empty when the user asked to write a savestate this frame.
  std::string consume_savestate_request();
```

and the private member:

```c++
  std::string savestate_request_;
  bool savestate_key_was_down_ = false;
```

In `runtime/src/window.cpp`, add the accessor:

```c++
std::string Window::consume_savestate_request() {
  std::string path = std::move(savestate_request_);
  savestate_request_.clear();
  return path;
}
```

and in `process_events`, just before `return is_open_;`:

```c++
  // F5 captures a savestate for the offline symbol miner. The file name is
  // chosen by whichever number key is held: F5 alone writes slot 0.
  const bool f5_down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
  if (f5_down && !savestate_key_was_down_) {
    int slot = 0;
    for (int i = 0; i < 10; ++i) {
      if (GetAsyncKeyState('0' + i) & 0x8000) { slot = i; break; }
    }
    savestate_request_ = "context_" + std::to_string(slot) + ".ss";
    std::cout << "[state] capturing " << savestate_request_ << "\n";
  }
  savestate_key_was_down_ = f5_down;
```

Ensure `<iostream>` and `<string>` are included in `window.cpp`.

In `runtime/src/main.cpp`, inside the frame loop after `window.process_events(hardware)` succeeds:

```c++
      if (const std::string state_path = window.consume_savestate_request(); !state_path.empty()) {
        std::cout << "[state] " << (aw_mgba_save_state(core, state_path.c_str()) ? "saved " : "FAILED ")
                  << state_path << "\n";
      }
```

- [ ] **Step 3: Capture the four contexts**

Run: `build/native/runtime/advance-wars-native.exe "rom/Advance Wars (USA) (Rev 1).gba"`

Reach each context and press F5 with the listed digit held:
- Map view → hold `1`, press F5 → `context_1.ss`
- Action menu open → hold `2`, press F5 → `context_2.ss`
- Name entry → hold `3`, press F5 → `context_3.ss`
- Title / CO select → hold `4`, press F5 → `context_4.ss`
- A cutscene / dialogue → hold `5`, press F5 → `context_5.ss`

Add `*.ss` to `.gitignore` — savestates contain ROM-derived data and must not be committed.

- [ ] **Step 4: Write the miner**

Create `runtime/tools/symbol_miner.cpp`:

```c++
// Offline symbol miner. Loads savestates captured with F5 in the runtime and
// reports EWRAM bytes that reliably distinguish one UI context from another,
// which is all ContextProbe needs.
//
// Usage:
//   aw-symbol-miner <rom> <label>=<state.ss> [<label>=<state.ss> ...]
//
// Example:
//   aw-symbol-miner rom.gba MapView=context_1.ss ListMenu=context_2.ss

#include "aw/mgba_adapter.h"
#include "aw/probe/backend_mgba.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kEwramBase = 0x02000000;

struct Capture {
  std::string label;
  std::vector<std::uint8_t> ewram;
};

bool capture(const std::string& rom, const std::string& state_path, Capture& out) {
  std::vector<std::uint32_t> video(240 * 160, 0);
  struct mCore* core = aw_mgba_create(rom.c_str(), video.data(), 240);
  if (core == nullptr) {
    std::cerr << "cannot create core for " << rom << "\n";
    return false;
  }
  if (!aw_mgba_load_state(core, state_path.c_str())) {
    std::cerr << "cannot load state " << state_path << "\n";
    aw_mgba_destroy(core);
    return false;
  }
  // Run a couple of frames so transient state settles.
  for (int i = 0; i < 2; ++i) aw_mgba_run_frame(core, 0);

  aw::MgbaProbeBackend backend(core);
  std::size_t size = 0;
  const std::uint8_t* ewram = backend.ewram(size);
  if (ewram == nullptr || size == 0) {
    std::cerr << "no EWRAM for " << state_path << "\n";
    aw_mgba_destroy(core);
    return false;
  }
  out.ewram.assign(ewram, ewram + size);
  aw_mgba_destroy(core);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: aw-symbol-miner <rom> <label>=<state.ss> [...]\n";
    return 1;
  }
  const std::string rom = argv[1];

  std::vector<Capture> captures;
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    const std::size_t eq = arg.find('=');
    if (eq == std::string::npos) {
      std::cerr << "expected label=path, got " << arg << "\n";
      return 1;
    }
    Capture cap;
    cap.label = arg.substr(0, eq);
    if (!capture(rom, arg.substr(eq + 1), cap)) return 1;
    captures.push_back(std::move(cap));
    std::cout << "captured " << captures.back().label << "\n";
  }

  if (captures.size() < 2) {
    std::cerr << "need at least two captures to distinguish contexts\n";
    return 1;
  }

  const std::size_t size = captures.front().ewram.size();
  for (const Capture& c : captures) {
    if (c.ewram.size() != size) {
      std::cerr << "capture sizes differ\n";
      return 1;
    }
  }

  // A useful discriminator holds a different value in every capture, so one
  // byte read classifies the context outright.
  std::cout << "\nBytes unique across all " << captures.size() << " contexts:\n";
  int reported = 0;
  for (std::size_t off = 0; off < size && reported < 40; ++off) {
    std::map<std::uint8_t, int> seen;
    for (const Capture& c : captures) seen[c.ewram[off]]++;
    if (seen.size() != captures.size()) continue;

    const std::uint32_t addr = kEwramBase + static_cast<std::uint32_t>(off);
    std::cout << "  addr 0x" << std::hex << addr << std::dec
              << " (decimal " << addr << "):";
    for (const Capture& c : captures) {
      std::cout << ' ' << c.label << '=' << static_cast<int>(c.ewram[off]);
    }
    std::cout << '\n';
    ++reported;
  }
  if (reported == 0) {
    std::cout << "  none. Try pairs of contexts instead of all at once, or\n"
                 "  capture cleaner states (no animation in progress).\n";
  }
  return 0;
}
```

- [ ] **Step 5: Build the miner**

Append to `runtime/CMakeLists.txt`, after the `advance-wars-native` block:

```cmake
add_executable(aw-symbol-miner
  tools/symbol_miner.cpp
)

target_include_directories(aw-symbol-miner PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(aw-symbol-miner PRIVATE aw_runtime)
```

Run: `cmake --build build/native --target aw-symbol-miner`
Expected: builds clean.

- [ ] **Step 6: Mine and write the symbol table**

Run:
```bash
build/native/runtime/aw-symbol-miner.exe "rom/Advance Wars (USA) (Rev 1).gba" \
  MapView=context_1.ss ListMenu=context_2.ss NameEntry=context_3.ss \
  FrontEnd=context_4.ss Cutscene=context_5.ss
```

Pick one address whose values are stable and distinct. Combine it with the winning indicator signatures from `docs/oam-indicator-findings.md` (Task 4) and write `data/symbols/15053499d5b3f49128a941d7f2d84876f5424d0c.ini` following the schema in `data/symbols/README.md`.

Set `steerable = 0` for `Cutscene`. Set `scroll_bg` for `MapView` to the layer identified in Task 4 (the one whose scroll register changed while the camera panned); leave it `-1` elsewhere.

- [ ] **Step 7: Verify the table loads and improves behaviour**

Run: `build/native/runtime/advance-wars-native.exe "rom/Advance Wars (USA) (Rev 1).gba"`

Expected: `[nav] loaded symbol table data/symbols/15053499...ini` on stdout, the cursor locks onto the pointer immediately rather than after a correlation delay, and the mouse does not steer during cutscenes.

- [ ] **Step 8: Update the README**

In `README.md`, replace the Controls section with one that documents the mouse and gamepad, and add a short section on regenerating symbol tables:

```markdown
## 🎮 Controls

| GBA Button | Keyboard | Gamepad (XInput) |
| :--- | :--- | :--- |
| **D-Pad** | Arrow Keys | D-Pad / Left Stick |
| **Button A** | `Z` | A |
| **Button B** | `X` | B |
| **Start** | `Enter` | Start |
| **Select** | `Backspace` | Back |
| **L / R** | `Q` / `E` | LB / RB |

### Mouse

Moving the mouse over the game steers the game's own cursor to whatever is
under the pointer, in the map view, menus, the name-entry grid and the
front-end screens. Left click is A, right click is B. Holding the pointer near
a screen edge scrolls the map. Pressing any D-pad key or gamepad direction
hands control straight back to the keyboard or pad.

The mouse never writes to game memory: it watches where the game draws its
selection indicator and steers with ordinary D-pad presses, so the game's own
cursor logic, camera and sound effects stay authoritative. Disable it under
Settings → Controls if you prefer.

### Regenerating symbol tables

Symbol tables in `data/symbols/` let the runtime recognise which UI screen is
active. They are optional — without one, the mouse falls back to correlation
tracking. To regenerate for a new ROM revision:

1. Run the game, reach each UI context, and press `F5` while holding a digit
   key to capture `context_<digit>.ss`.
2. Run `aw-symbol-miner <rom> MapView=context_1.ss ...` and pick a
   discriminating address from its output.
3. Write `data/symbols/<rom-sha1-lowercase>.ini` per `data/symbols/README.md`.
```

- [ ] **Step 9: Run the full suite and commit**

Run: `ctest --test-dir build/native --output-on-failure`
Expected: all tests pass.

```bash
git add runtime/tools/symbol_miner.cpp runtime/include/aw/mgba_adapter.h \
        runtime/src/mgba_adapter.c runtime/include/aw/window.hpp \
        runtime/src/window.cpp runtime/src/main.cpp runtime/CMakeLists.txt \
        README.md .gitignore data/symbols/
git commit -m "feat: add symbol miner, savestate capture hotkey and docs

F5 captures a labeled savestate while playing; aw-symbol-miner diffs those
states to find EWRAM bytes that discriminate UI contexts. Symbol tables are
optional: without one the runtime uses correlation tracking."
```

---

---

### Task 10: Headless convergence test against the real game

The spec requires an automated check that a scripted pointer target is actually reached, per context. Unit tests prove the controller converges against a *fake* game; this proves it converges against the real one. It skips cleanly when savestates are absent, so it never breaks a fresh clone or CI.

**Files:**
- Create: `runtime/tests/nav_convergence_tests.cpp`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Consumes: `aw::NavController` (Task 8), `aw_mgba_create`/`aw_mgba_load_state`/`aw_mgba_run_frame`/`aw_mgba_destroy` (Tasks 3 and 9), `aw::InputFrame` (Task 2), `aw::MgbaProbeBackend` and `aw::decode_oam_entry` for the assertion.
- Produces: nothing consumed by later tasks.

- [ ] **Step 1: Expose the tracked indicator position**

The test asserts on where the game's indicator actually landed, so `NavController` must report it. Add to `runtime/include/aw/nav/nav_controller.hpp`, next to `indicator_found()`:

```c++
  // The indicator position the controller steered against on the last update.
  Indicator indicator() const { return indicator_; }
```

- [ ] **Step 2: Write the test**

Create `runtime/tests/nav_convergence_tests.cpp`:

```c++
// Drives NavController against the real game from a savestate and asserts the
// game's own indicator settles on a scripted pointer target.
//
// Skips (exit 0) when the ROM or the savestates are missing, since neither is
// committed. Capture states with F5 in the runtime first.

#include "aw/input/input_frame.hpp"
#include "aw/mgba_adapter.h"
#include "aw/nav/nav_controller.hpp"
#include "aw/rom.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* kRomPath = "rom/Advance Wars (USA) (Rev 1).gba";

// 600 frames is ten seconds of game time, far longer than the ~0.5 s the
// design predicts for a full-screen traversal.
constexpr int kFrameBudget = 600;
constexpr int kTolerance = 8;
// Frames the indicator must hold the target, so a cursor sweeping past does
// not count as arrival.
constexpr int kStableFrames = 10;

struct ContextCase {
  const char* label;
  const char* state_path;
  int target_x;
  int target_y;
};

bool run_case(const ContextCase& test_case, const std::string& rom_sha1) {
  std::vector<std::uint32_t> video(240 * 160, 0);
  struct mCore* core = aw_mgba_create(kRomPath, video.data(), 240);
  if (core == nullptr) {
    std::cerr << "cannot create core
";
    return false;
  }
  if (!aw_mgba_load_state(core, test_case.state_path)) {
    std::cerr << "cannot load " << test_case.state_path << "
";
    aw_mgba_destroy(core);
    return false;
  }

  aw::NavController nav;
  nav.set_core(core);
  nav.load_symbols(rom_sha1);

  // A stationary pointer parked on the target. `moved` stays true so the
  // controller remains armed for the whole run; a real mouse re-arms on each
  // motion event instead.
  aw::InputFrame frame;
  frame.pointer_count = 1;
  frame.pointers[0].kind = aw::PointerKind::Mouse;
  frame.pointers[0].in_viewport = true;
  frame.pointers[0].moved = true;
  frame.pointers[0].gba_x = test_case.target_x;
  frame.pointers[0].gba_y = test_case.target_y;

  bool converged = false;
  int stable = 0;
  for (int i = 0; i < kFrameBudget && !converged; ++i) {
    const std::uint16_t keys = nav.update(frame);
    aw_mgba_run_frame(core, keys);

    const aw::Indicator ind = nav.indicator();
    if (!ind.found) {
      stable = 0;
      continue;
    }

    const int dx = std::abs(ind.screen_x - test_case.target_x);
    const int dy = std::abs(ind.screen_y - test_case.target_y);
    if (dx <= kTolerance && dy <= kTolerance) {
      if (++stable >= kStableFrames) converged = true;
    } else {
      stable = 0;
    }
  }

  aw_mgba_destroy(core);
  return converged;
}

}  // namespace

int main() {
  if (!std::filesystem::exists(kRomPath)) {
    std::cout << "nav_convergence_tests skipped: no ROM at " << kRomPath << "
";
    return 0;
  }

  const aw::RomImage rom = aw::load_rom_file(kRomPath);
  const std::string rom_sha1 = aw::sha1_hex(rom.bytes);

  // Target the middle-right of the screen: far enough from any plausible
  // starting position that reaching it proves real steering.
  const ContextCase cases[] = {
      {"MapView",   "context_1.ss", 176, 112},
      {"ListMenu",  "context_2.ss", 176, 112},
      {"NameEntry", "context_3.ss", 176, 112},
      {"FrontEnd",  "context_4.ss", 176, 112},
  };

  int ran = 0;
  for (const ContextCase& test_case : cases) {
    if (!std::filesystem::exists(test_case.state_path)) {
      std::cout << "  skip " << test_case.label << ": no " << test_case.state_path << "
";
      continue;
    }
    ++ran;
    if (!run_case(test_case, rom_sha1)) {
      std::cerr << "FAILED to converge in " << test_case.label << "
";
      return 1;
    }
    std::cout << "  ok " << test_case.label << "
";
  }

  if (ran == 0) {
    std::cout << "nav_convergence_tests skipped: no savestates captured
";
    return 0;
  }
  std::cout << "nav_convergence_tests passed (" << ran << " contexts)!
";
  return 0;
}
```

Note the target is a *screen* position, not a tile: `ListMenu` and `NameEntry` have no camera, and a menu whose entries do not extend to y=112 will legitimately fail to reach it. If a context fails, first check whether the target is reachable in that context at all and move it if not.

- [ ] **Step 3: Register the test**

Append to `runtime/CMakeLists.txt` before the `advance-wars-native` block:

```cmake
add_executable(nav_convergence_tests
  tests/nav_convergence_tests.cpp
)

target_include_directories(nav_convergence_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(nav_convergence_tests PRIVATE aw_runtime)

add_test(NAME nav_convergence_tests COMMAND nav_convergence_tests)
set_tests_properties(nav_convergence_tests PROPERTIES TIMEOUT 180)
```

Note the existing file uses `set_tests_properties` (not the standard `set_tests_property`); match whichever form the surrounding blocks use so the configure step stays clean.

- [ ] **Step 4: Run it**

Run: `cmake --build build/native --target nav_convergence_tests && ctest --test-dir build/native -R nav_convergence_tests --output-on-failure`

Expected with savestates present: `nav_convergence_tests passed (4 contexts)!`
Expected without: a skip message and exit 0.

If a context fails to converge, that context's indicator tracking is wrong — not the controller. Check `docs/oam-indicator-findings.md` for whether that context had a winning sprite signature at all, and set `steerable = 0` for it in the symbol table if it did not.

- [ ] **Step 5: Run the full suite and commit**

Run: `ctest --test-dir build/native --output-on-failure`
Expected: all tests pass.

```bash
git add runtime/tests/nav_convergence_tests.cpp \
        runtime/include/aw/nav/nav_controller.hpp runtime/CMakeLists.txt
git commit -m "test: add headless convergence test against the real game

Drives NavController from captured savestates and asserts the game's own
indicator settles on a scripted pointer target. Skips when the ROM or
savestates are absent, since neither is committed."
```

---

## Follow-Up (only if Task 4's gate says so)

If Task 4 found a context whose indicator is a background tile rather than a
sprite, add a task here implementing `BgTilemapTracker` behind the same
interface as `OamTracker`: read the BG tilemap through
`ProbeBackend` (add a `vram()` accessor mirroring `oam()`), diff it frame to
frame, and report the screen position of the changed region as the indicator
position. `PointerNav` consumes it unchanged, because both trackers produce a
screen-space `Indicator`. Set `steerable = 0` for that context in the symbol
table until the tracker exists, so the mouse degrades to clicks there instead
of misbehaving.
