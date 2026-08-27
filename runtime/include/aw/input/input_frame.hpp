#pragma once

#include "aw/hardware.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace aw {

// The four D-pad bits, as a mask. Pointer navigation disarms when any of
// these arrives from a physical device.
constexpr std::uint16_t kDpadMask = kKeyUp | kKeyDown | kKeyLeft | kKeyRight;

// Engine-level hotkeys (time travel / fast forward / undo). These never
// reach the GBA; the platform layer ORs them into InputFrame alongside the
// buttons.
constexpr std::uint16_t kHotkeyRewind = 1u << 0;        // Hold: rewind time
constexpr std::uint16_t kHotkeyFastForward = 1u << 1;   // Hold: fast forward
constexpr std::uint16_t kHotkeyUndo = 1u << 2;          // Press: undo last order
constexpr std::uint16_t kHotkeyMask = kHotkeyRewind | kHotkeyFastForward | kHotkeyUndo;

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
  bool middle_down = false;     // Middle button, or 3-finger drag / pan
  bool primary_edge = false;    // primary_down went false -> true this poll
  bool secondary_edge = false;  // secondary_down went false -> true this poll
  bool middle_edge = false;     // middle_down went false -> true this poll
  bool double_click = false;    // Double click or quick double tap
  int drag_dx = 0;              // Drag delta X in GBA pixels
  int drag_dy = 0;              // Drag delta Y in GBA pixels
};

constexpr std::size_t kMaxPointers = 4;

// Everything the platform layer produces for one frame. Sources OR their
// contributions into the same frame, so keyboard, gamepad and touch overlay
// can coexist.
struct InputFrame {
  std::uint16_t gba_keys = 0;    // Combined aw::kKey* bitmask
  std::uint16_t device_dpad = 0; // D-pad bits that came from a physical device
  std::uint16_t hotkeys = 0;     // Combined aw::kHotkey* bitmask
  std::array<PointerState, kMaxPointers> pointers{};
  std::size_t pointer_count = 0;

  void clear() {
    gba_keys = 0;
    device_dpad = 0;
    hotkeys = 0;
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
