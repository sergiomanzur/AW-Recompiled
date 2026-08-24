#pragma once

#include "aw/config_file.hpp"
#include "aw/hardware.hpp"
#include "aw/input/input_frame.hpp"
#include "aw/input/source_win32.hpp"
#include "aw/input_config.hpp"
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
  bool is_widescreen() const {
    return aspect_ratio_ == AspectRatio::Ratio_16_9 ||
           aspect_ratio_ == AspectRatio::Ratio_21_9 ||
           aspect_ratio_ == AspectRatio::Ratio_21_10;
  }

  void set_internal_resolution(InternalResolution res);
  InternalResolution internal_resolution() const { return internal_resolution_; }

  void set_video_filter(VideoFilter filter);
  VideoFilter video_filter() const { return video_filter_; }

  bool show_hud() const { return show_hud_; }
  void set_show_hud(bool show);
  void toggle_hud() { set_show_hud(!show_hud_); }

  void resize_client(int width, int height);

  // Input mapping dialog
  void show_controls_dialog();

  // Config.ini persistence
  void load_config(const ConfigFile& config);
  void save_config(ConfigFile& config) const;

  // Returns true if the user selected a new ROM from File/Settings menu
  bool has_pending_rom() const { return !pending_rom_path_.empty(); }
  std::string consume_pending_rom();

  // Savestate management
  bool has_pending_save_state() const { return !pending_save_state_path_.empty(); }
  bool has_pending_load_state() const { return !pending_load_state_path_.empty(); }
  std::string consume_pending_save_state();
  std::string consume_pending_load_state();
  void request_save_state(const std::string& path) { pending_save_state_path_ = path; }
  void request_load_state(const std::string& path) { pending_load_state_path_ = path; }

  // Time travel / fast forward. The held flags come from the polled input
  // frame (Backspace/Tab keyboard, Y/X + triggers on XInput); fast forward
  // also engages via its menu toggle. Rewind steps once per consume call.
  bool rewind_held() const { return rewind_held_; }
  bool fast_forward_active() const { return fast_forward_held_ || fast_forward_latch_; }
  bool consume_rewind_step();
  void request_rewind_step() { rewind_step_requested_ = true; }
  void toggle_fast_forward_latch();

  // -1 = rewinding, 0 = normal, +1 = fast forwarding. Shown in the title bar
  // so the mode is visible without on-screen clutter.
  void set_playback_indicator(int indicator);

  // File dialog helpers
  static std::string open_file_dialog(void* parent_hwnd = nullptr);
  static std::string open_savestate_dialog(void* parent_hwnd = nullptr);
  static std::string save_savestate_dialog(void* parent_hwnd = nullptr);

  void set_pending_rom(const std::string& path) { pending_rom_path_ = path; }

  // The neutral frame produced by this window's input sources each poll.
  const InputFrame& input_frame() const { return input_frame_; }

private:
  void update_menu_checks();

  void* hwnd_ = nullptr;
  void* hdc_ = nullptr;
  void* menu_ = nullptr;
  bool is_open_ = false;
  int width_ = 960;
  int height_ = 640;
  AspectRatio aspect_ratio_ = AspectRatio::Original_3_2;
  InternalResolution internal_resolution_ = InternalResolution::Native;
  VideoFilter video_filter_ = VideoFilter::NearestNeighbor;
  bool show_hud_ = true;
  bool f2_key_was_down_ = false;
  std::string pending_rom_path_;
  std::string pending_save_state_path_;
  std::string pending_load_state_path_;
  bool f5_key_was_down_ = false;
  bool f9_key_was_down_ = false;

  bool rewind_held_ = false;
  bool fast_forward_held_ = false;
  bool fast_forward_latch_ = false;
  bool rewind_step_requested_ = false;
  int playback_indicator_ = 0;
  std::string window_title_;

  InputMapping input_mapping_;

  Win32InputSource win32_input_;
  InputFrame input_frame_;

  // High-resolution & Scale2x filtering buffers
  std::vector<std::uint32_t> scale2x_buffer_;

  // Viewport caching for clean letterboxing
  int last_client_w_ = 0;
  int last_client_h_ = 0;
  AspectRatio last_aspect_ratio_ = AspectRatio::Original_3_2;
  ViewportRect cached_viewport_{};
};

}  // namespace aw
