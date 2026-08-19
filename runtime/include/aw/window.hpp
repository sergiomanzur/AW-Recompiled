#pragma once

#include "aw/hardware.hpp"
#include "aw/ppu.hpp"

namespace aw {

class Window {
public:
  Window(int width = 960, int height = 640, const char* title = "Advance Wars (Native Recomp)");
  ~Window();

  bool process_events(Hardware& hardware);
  void render(const Ppu& ppu);
  bool is_open() const { return is_open_; }

private:
  void* hwnd_ = nullptr;
  void* hdc_ = nullptr;
  bool is_open_ = false;
  int width_ = 960;
  int height_ = 640;
};

}  // namespace aw
