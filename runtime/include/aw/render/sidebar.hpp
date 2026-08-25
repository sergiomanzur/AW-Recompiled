#pragma once

#include "aw/tactical_intel.hpp"
#include "aw/viewport.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aw {

// Everything the tactical sidebar displays. Filled by the game loop from
// sources that are all real: engine state, the mined cursor read, the
// rewind/undo subsystems. Nothing here is fabricated.
struct SidebarData {
  // Playback
  bool fast_forward = false;
  bool rewinding = false;

  // Map sensor (verified cursor evidence)
  bool in_map = false;
  bool cursor_valid = false;
  int cursor_x = 0;
  int cursor_y = 0;

  // Time travel / undo
  int undo_depth = 0;
  int undo_capacity = 0;
  int rewind_snapshots = 0;
  int rewind_capacity = 0;
  double rewind_window_seconds = 0.0;

  // Engine telemetry
  double fps = 0.0;
  double emu_speed_pct = 100.0;
  std::uint64_t frames_run = 0;

  // Replay / input display
  bool replay_recording = false;
  bool replay_playing = false;
  std::uint32_t replay_frame = 0;
  std::uint32_t replay_total = 0;
  std::uint16_t live_keys = 0;

  // Combat forecast; valid only when real attacker/defender data exists.
  DamageForecast forecast{};
};

// Width of the sidebar panel and the narrowest game area still worth
// letterboxing next to it.
constexpr int kSidebarWidth = 280;
constexpr int kSidebarMinGameWidth = 420;

struct SidebarLayout {
  ViewportRect game;      // Where the GBA framebuffer goes (3:2 fit)
  ViewportRect sidebar;   // The panel; zero-sized when not shown
  bool sidebar_shown = false;
};

// Splits the client area into a 3:2 game rect and a right-hand sidebar.
// The sidebar hides itself when disabled, in Stretch mode, or when the
// client is too narrow to show both.
SidebarLayout calculate_sidebar_layout(int client_width, int client_height,
                                       bool sidebar_enabled, AspectRatio aspect);

// The panel's text lines, in draw order. Pure so tests can pin the content;
// the window layer only renders them. Pairs of (text, is_heading).
std::vector<std::pair<std::string, bool>> sidebar_panel_lines(const SidebarData& data);

}  // namespace aw
