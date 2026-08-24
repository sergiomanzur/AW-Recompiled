#include "aw/input_config.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>
#endif

namespace aw {

const char* gba_button_name(GbaButton btn) {
  switch (btn) {
    case Gba_A:      return "A Button";
    case Gba_B:      return "B Button";
    case Gba_Select: return "Select";
    case Gba_Start:  return "Start";
    case Gba_Right:  return "D-Pad Right";
    case Gba_Left:   return "D-Pad Left";
    case Gba_Up:     return "D-Pad Up";
    case Gba_Down:   return "D-Pad Down";
    case Gba_R:      return "R Shoulder";
    case Gba_L:      return "L Shoulder";
    default:         return "Unknown";
  }
}

void InputMapping::reset_to_defaults() {
  controller_index = 0;
  mouse_enabled = false;

  // Defaults: Keyboard
  bindings[Gba_A].key_vk      = 'Z';
  bindings[Gba_B].key_vk      = 'X';
  // Shift, not Backspace: Backspace is the engine-level Time Travel (rewind)
  // hotkey and must not double as a GBA button.
  bindings[Gba_Select].key_vk = 0x10; // VK_SHIFT
  bindings[Gba_Start].key_vk  = 0x0D; // VK_RETURN
  bindings[Gba_Right].key_vk  = 0x27; // VK_RIGHT
  bindings[Gba_Left].key_vk   = 0x25; // VK_LEFT
  bindings[Gba_Up].key_vk     = 0x26; // VK_UP
  bindings[Gba_Down].key_vk   = 0x28; // VK_DOWN
  bindings[Gba_R].key_vk      = 'E';
  bindings[Gba_L].key_vk      = 'Q';

  // Defaults: Gamepad (XInput masks)
  bindings[Gba_A].pad_button      = 0x1000; // XINPUT_GAMEPAD_A
  bindings[Gba_B].pad_button      = 0x2000; // XINPUT_GAMEPAD_B
  bindings[Gba_Select].pad_button = 0x0020; // XINPUT_GAMEPAD_BACK
  bindings[Gba_Start].pad_button  = 0x0010; // XINPUT_GAMEPAD_START
  bindings[Gba_Right].pad_button  = 0x0008; // XINPUT_GAMEPAD_DPAD_RIGHT
  bindings[Gba_Left].pad_button   = 0x0004; // XINPUT_GAMEPAD_DPAD_LEFT
  bindings[Gba_Up].pad_button     = 0x0001; // XINPUT_GAMEPAD_DPAD_UP
  bindings[Gba_Down].pad_button   = 0x0002; // XINPUT_GAMEPAD_DPAD_DOWN
  bindings[Gba_R].pad_button      = 0x0200; // XINPUT_GAMEPAD_RIGHT_SHOULDER
  bindings[Gba_L].pad_button      = 0x0100; // XINPUT_GAMEPAD_LEFT_SHOULDER
}

void InputMapping::load_from_config(const ConfigFile& config) {
  reset_to_defaults();

  controller_index = config.get_int("Input", "controller_index", 0);
  mouse_enabled = config.get_int("Input", "mouse_enabled", 0) != 0;

  static const char* keys[Gba_Count] = {
    "key_a", "key_b", "key_select", "key_start",
    "key_right", "key_left", "key_up", "key_down", "key_r", "key_l"
  };

  static const char* pad_keys[Gba_Count] = {
    "pad_a", "pad_b", "pad_select", "pad_start",
    "pad_right", "pad_left", "pad_up", "pad_down", "pad_r", "pad_l"
  };

  for (int i = 0; i < Gba_Count; ++i) {
    const int vk = config.get_int("Input", keys[i], -1);
    if (vk > 0) {
      bindings[i].key_vk = static_cast<std::uint32_t>(vk);
    }
    const int pad = config.get_int("Input", pad_keys[i], -1);
    if (pad > 0) {
      bindings[i].pad_button = static_cast<std::uint16_t>(pad);
    }
  }

  // Migration: configs written before Backspace became the Time Travel
  // hotkey still bind Select to VK_BACK (0x08). That would both rewind and
  // press Select at once, so move those bindings to the new default.
  if (bindings[Gba_Select].key_vk == 0x08) {
    bindings[Gba_Select].key_vk = 0x10; // VK_SHIFT
  }
}

void InputMapping::save_to_config(ConfigFile& config) const {
  config.set_int("Input", "controller_index", controller_index);
  config.set_int("Input", "mouse_enabled", mouse_enabled ? 1 : 0);

  static const char* keys[Gba_Count] = {
    "key_a", "key_b", "key_select", "key_start",
    "key_right", "key_left", "key_up", "key_down", "key_r", "key_l"
  };

  static const char* pad_keys[Gba_Count] = {
    "pad_a", "pad_b", "pad_select", "pad_start",
    "pad_right", "pad_left", "pad_up", "pad_down", "pad_r", "pad_l"
  };

  for (int i = 0; i < Gba_Count; ++i) {
    config.set_int("Input", keys[i], static_cast<int>(bindings[i].key_vk));
    config.set_int("Input", pad_keys[i], static_cast<int>(bindings[i].pad_button));
  }
}

std::string vk_to_string(std::uint32_t vk) {
  switch (vk) {
    case 0x08: return "Backspace";
    case 0x09: return "Tab";
    case 0x0D: return "Enter";
    case 0x10: return "Shift";
    case 0x11: return "Ctrl";
    case 0x12: return "Alt";
    case 0x20: return "Space";
    case 0x25: return "Left Arrow";
    case 0x26: return "Up Arrow";
    case 0x27: return "Right Arrow";
    case 0x28: return "Down Arrow";
    case 'W': return "W";
    case 'A': return "A";
    case 'S': return "S";
    case 'D': return "D";
    case 'J': return "J";
    case 'K': return "K";
    default:
      if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        return std::string(1, static_cast<char>(vk));
      }
      return "VK " + std::to_string(vk);
  }
}

std::string xinput_button_to_string(std::uint16_t button_mask) {
  switch (button_mask) {
    case 0x0001: return "D-Pad Up";
    case 0x0002: return "D-Pad Down";
    case 0x0004: return "D-Pad Left";
    case 0x0008: return "D-Pad Right";
    case 0x0010: return "Start / Menu";
    case 0x0020: return "Back / View";
    case 0x0040: return "Left Thumb Press";
    case 0x0080: return "Right Thumb Press";
    case 0x0100: return "LB (Left Bumper)";
    case 0x0200: return "RB (Right Bumper)";
    case 0x1000: return "Button A";
    case 0x2000: return "Button B";
    case 0x4000: return "Button X";
    case 0x8000: return "Button Y";
    default:     return "Gamepad (" + std::to_string(button_mask) + ")";
  }
}

}  // namespace aw
