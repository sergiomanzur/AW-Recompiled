#pragma once

#include "aw/config_file.hpp"
#include <cstdint>
#include <string>

namespace aw {

struct ControlBinding {
  std::uint32_t key_vk = 0;     // Windows Virtual Key Code
  std::uint16_t pad_button = 0; // XInput Gamepad Button Mask (e.g. XINPUT_GAMEPAD_A)
};

enum GbaButton {
  Gba_A = 0,
  Gba_B,
  Gba_Select,
  Gba_Start,
  Gba_Right,
  Gba_Left,
  Gba_Up,
  Gba_Down,
  Gba_R,
  Gba_L,
  Gba_Count
};

struct InputMapping {
  ControlBinding bindings[Gba_Count];
  int controller_index = 0; // 0 = Controller 1, 1 = Controller 2, ..., -1 = Disabled
  bool mouse_enabled = true; // PC Native Mouse Navigation enabled

  void reset_to_defaults();
  void load_from_config(const ConfigFile& config);
  void save_to_config(ConfigFile& config) const;
};

std::string vk_to_string(std::uint32_t vk);
std::string xinput_button_to_string(std::uint16_t button_mask);
const char* gba_button_name(GbaButton btn);

}  // namespace aw
