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
