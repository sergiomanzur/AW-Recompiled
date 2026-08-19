#pragma once

#include "aw/config_file.hpp"
#include "aw/hardware.hpp"
#include "aw/input_config.hpp"
#include "aw/mouse_cursor.hpp"
#include "aw/ppu.hpp"
#include <string>
#include <vector>

namespace aw {

enum class AspectRatio {
  Original_3_2,  // 3:2 Window Mode (960x640)
  Ratio_4_3,     // 4:3 Window Mode (960x720)
  Ratio_16_9,    // 16:9 Window Mode (1152x648)
  Ratio_21_9,    // 21:9 Window Mode (1260x540)
  Ratio_21_10,   // 21:10 Window Mode (1134x540)
  Stretch        // Fill Window without aspect lock
};

enum class InternalResolution {
  Native,    // GBA Native (240x160)
  Res_720p,  // 720p (1280x720)
  Res_1080p, // 1080p (1920x1080)
  Res_4K     // 4K (3840x2160)
};

enum class VideoFilter {
  NearestNeighbor, // Sharp, crisp retro pixels
  Bilinear,        // Smooth hardware anti-aliased interpolation
  Scale2x          // HD 2x pixel art smoothing filter
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

  void set_internal_resolution(InternalResolution res);
  InternalResolution internal_resolution() const { return internal_resolution_; }

  void set_video_filter(VideoFilter filter);
  VideoFilter video_filter() const { return video_filter_; }

  void resize_client(int width, int height);

  // Input mapping dialog
  void show_controls_dialog();

  // Config.ini persistence
  void load_config(const ConfigFile& config);
  void save_config(ConfigFile& config) const;

  // Returns true if the user selected a new ROM from File/Settings menu
  bool has_pending_rom() const { return !pending_rom_path_.empty(); }
  std::string consume_pending_rom();

  // Launches native Windows File Open Dialog for selecting a .gba ROM file
  static std::string open_file_dialog(void* parent_hwnd = nullptr);

  void set_pending_rom(const std::string& path) { pending_rom_path_ = path; }

  // mGBA core pointer for direct memory access (mouse cursor support)
  void set_mgba_core(void* core);
  MouseCursor& mouse_cursor() { return mouse_cursor_; }

private:
  void update_menu_checks();

  // Convert window client coordinates to GBA screen coordinates (0-239, 0-159).
  // Returns false if the position is outside the game viewport.
  bool client_to_gba(int client_x, int client_y, int& gba_x, int& gba_y) const;

  void* hwnd_ = nullptr;
  void* hdc_ = nullptr;
  void* menu_ = nullptr;
  bool is_open_ = false;
  int width_ = 960;
  int height_ = 640;
  AspectRatio aspect_ratio_ = AspectRatio::Original_3_2;
  InternalResolution internal_resolution_ = InternalResolution::Native;
  VideoFilter video_filter_ = VideoFilter::Bilinear;
  std::string pending_rom_path_;

  InputMapping input_mapping_;

  // PC Native Touchscreen & Direct Mouse Pointer Navigation (Absolute Target Steering)
  MouseCursor mouse_cursor_;
  int cur_grid_x_ = 0;           // Tracked in-game cursor X tile (0..14)
  int cur_grid_y_ = 0;           // Tracked in-game cursor Y tile (0..9)
  int mouse_step_timer_ = 0;     // Frame timer between steps towards target
  bool mouse_grid_init_ = false; // True once target coordinates are initialized
  bool mouse_left_was_down_ = false;   // Edge detection for left click
  bool mouse_right_was_down_ = false;  // Edge detection for right click

  // High-resolution & Scale2x filtering buffers
  std::vector<std::uint32_t> scale2x_buffer_;

  // Viewport caching for clean letterboxing
  int last_client_w_ = 0;
  int last_client_h_ = 0;
  AspectRatio last_aspect_ratio_ = AspectRatio::Original_3_2;
  ViewportRect cached_viewport_{};
};

}  // namespace aw
