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

  // Engine hotkeys: Backspace rewinds time, Tab fast-forwards. These are
  // polled raw so they stay engine-owned even if the GBA bindings change.
  if (GetAsyncKeyState(VK_BACK) & 0x8000) {
    frame.hotkeys |= kHotkeyRewind;
  }
  if (GetAsyncKeyState(VK_TAB) & 0x8000) {
    frame.hotkeys |= kHotkeyFastForward;
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

      // Engine hotkeys on buttons the GBA layout leaves free: Y/X faces plus
      // the analog triggers (RT feels natural for held fast-forward).
      if (btns & XINPUT_GAMEPAD_Y) frame.hotkeys |= kHotkeyRewind;
      if (btns & XINPUT_GAMEPAD_X) frame.hotkeys |= kHotkeyFastForward;
      if (xstate.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        frame.hotkeys |= kHotkeyRewind;
      }
      if (xstate.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        frame.hotkeys |= kHotkeyFastForward;
      }
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
