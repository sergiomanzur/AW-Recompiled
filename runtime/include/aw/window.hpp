#pragma once

#include "aw/hardware.hpp"
#include "aw/ppu.hpp"

#include <string>

namespace aw {

enum class AspectRatio {
  Original_3_2,  // 3:2 (GBA Native 240x160)
  Ratio_4_3,     // 4:3 (Standard SD)
  Ratio_16_9,    // 16:9 (HD Widescreen)
  Ratio_21_9,    // 21:9 (Ultrawide)
  Ratio_21_10,   // 21:10 (Cinematic Ultrawide)
  Stretch        // Fill window without aspect lock
};

struct ViewportRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

// Pure calculation function for letterbox/pillarbox viewport geometry
ViewportRect calculate_viewport_rect(int client_width, int client_height, AspectRatio ratio);

class Window {
public:
  Window(int width = 960, int height = 640, const char* title = "Advance Wars (Native Recomp)");
  ~Window();

  bool process_events(Hardware& hardware);
  void render(const Ppu& ppu);
  bool is_open() const { return is_open_; }

  void set_aspect_ratio(AspectRatio ratio);
  AspectRatio aspect_ratio() const { return aspect_ratio_; }

  // Returns true if the user selected a new ROM from the File -> Open ROM menu
  bool has_pending_rom() const { return !pending_rom_path_.empty(); }
  std::string consume_pending_rom();

  // Launches native Windows File Open Dialog for selecting a .gba ROM file
  static std::string open_file_dialog(void* parent_hwnd = nullptr);

  void set_pending_rom(const std::string& path) { pending_rom_path_ = path; }

private:
  void update_menu_checks();

  void* hwnd_ = nullptr;
  void* hdc_ = nullptr;
  void* menu_ = nullptr;
  bool is_open_ = false;
  int width_ = 960;
  int height_ = 640;
  AspectRatio aspect_ratio_ = AspectRatio::Original_3_2;
  std::string pending_rom_path_;

  // Viewport caching for clean letterboxing
  int last_client_w_ = 0;
  int last_client_h_ = 0;
  AspectRatio last_aspect_ratio_ = AspectRatio::Original_3_2;
  ViewportRect cached_viewport_{};
};

}  // namespace aw
