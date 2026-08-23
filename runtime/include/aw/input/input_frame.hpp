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
