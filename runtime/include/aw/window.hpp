#pragma once

#include "aw/config_file.hpp"
#include "aw/hardware.hpp"
#include "aw/input/input_frame.hpp"
#include "aw/input/source_win32.hpp"
#include "aw/input_config.hpp"
#include "aw/ppu.hpp"
#include "aw/render/hd_text.hpp"
#include "aw/render/sidebar.hpp"
#include "aw/viewport.hpp"
#include <string>
#include <vector>

namespace aw {

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

class Window {
public:
  Window(int width = 960, int height = 640, const char* title = "Advance Wars (Native Recomp)");
  ~Window();

  bool process_events(Hardware& hardware);
  void render(const Ppu& ppu, const SidebarData& sidebar);
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

  // Tactical sidebar: the native panel shown beside the game in widescreen
  // aspects. Splits the client area into a 3:2 game rect plus the panel.
  bool sidebar_enabled() const { return sidebar_enabled_; }
  void set_sidebar_enabled(bool enabled);
  void toggle_sidebar() { set_sidebar_enabled(!sidebar_enabled_); }

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

  // Undo last order: Ctrl+Z keyboard, left-stick click on XInput, or the
  // File menu. Edge-triggered; one press = one undo.
  bool consume_undo_press();
  void request_undo() { undo_requested_ = true; }

  // Replays. F6 toggles recording (starting resets the console so the file
  // replays from power-on); playback starts from a file chosen in the File
  // menu and stops on F7 or when the input stream runs out.
  bool consume_record_toggle();
  void request_record_toggle() { record_toggle_requested_ = true; }
  bool consume_playback_stop();
  void request_playback_stop() { playback_stop_requested_ = true; }
  bool has_pending_replay() const { return !pending_replay_path_.empty(); }
  std::string consume_pending_replay();
  void request_replay_playback(const std::string& path) { pending_replay_path_ = path; }
  void set_recording_ui(bool recording);

  // Speedrun-style status strip over the game: frame counter + held buttons.
  bool input_display() const { return input_display_; }
  void set_input_display(bool on);
  void toggle_input_display() { set_input_display(!input_display_); }

  // HD text/UI replacement (see aw/render/hd_text.hpp). Enabling lazily
  // loads data/hd/tiles/tiles.ini; with no pack the game renders unchanged.
  bool hd_text_enabled() const { return hd_text_enabled_; }
  void set_hd_text_enabled(bool enabled);
  void toggle_hd_text() { set_hd_text_enabled(!hd_text_enabled_); }

  // -1 = rewinding, 0 = normal, +1 = fast forwarding. Shown in the title bar
  // so the mode is visible without on-screen clutter.
  void set_playback_indicator(int indicator);

  // File dialog helpers
  static std::string open_file_dialog(void* parent_hwnd = nullptr);
  static std::string open_savestate_dialog(void* parent_hwnd = nullptr);
  static std::string save_savestate_dialog(void* parent_hwnd = nullptr);
  static std::string open_replay_dialog(void* parent_hwnd = nullptr);
  static std::string open_patch_dialog(void* parent_hwnd = nullptr);

  void set_pending_rom(const std::string& path) { pending_rom_path_ = path; }

  // The neutral frame produced by this window's input sources each poll.
  const InputFrame& input_frame() const { return input_frame_; }

private:
  void update_menu_checks();
  void draw_sidebar_panel(void* hdc, const SidebarData& data, const SidebarLayout& layout);

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
  bool sidebar_enabled_ = true;
  bool input_display_ = true;
  bool hd_text_enabled_ = false;
  bool hd_text_pack_loaded_ = false;
  HdTextPack hd_pack_;
  bool f2_key_was_down_ = false;
  bool f4_key_was_down_ = false;
  bool f6_key_was_down_ = false;
  bool f7_key_was_down_ = false;
  bool f8_key_was_down_ = false;
  std::string pending_rom_path_;
  std::string pending_save_state_path_;
  std::string pending_load_state_path_;
  bool f5_key_was_down_ = false;
  bool f9_key_was_down_ = false;

  bool rewind_held_ = false;
  bool fast_forward_held_ = false;
  bool fast_forward_latch_ = false;
  bool rewind_step_requested_ = false;
  bool undo_requested_ = false;
  bool undo_hotkey_was_down_ = false;
  bool record_toggle_requested_ = false;
  bool playback_stop_requested_ = false;
  std::string pending_replay_path_;
  bool recording_ui_ = false;
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
  bool last_sidebar_enabled_ = true;
  ViewportRect cached_viewport_{};
};

}  // namespace aw
