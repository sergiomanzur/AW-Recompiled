#include "aw/render/sidebar.hpp"

#include <cmath>

namespace aw {

SidebarLayout calculate_sidebar_layout(int client_width, int client_height,
                                       bool sidebar_enabled, AspectRatio aspect) {
  SidebarLayout layout;
  if (client_width <= 0 || client_height <= 0) {
    return layout;
  }

  const bool too_narrow =
      client_width - kSidebarWidth < kSidebarMinGameWidth;
  const bool eligible = sidebar_enabled && aspect != AspectRatio::Stretch && !too_narrow;
  if (!eligible) {
    // Game keeps the whole client (existing aspect behaviour).
    layout.game = calculate_viewport_rect(client_width, client_height, aspect);
    layout.sidebar = {0, 0, 0, 0};
    layout.sidebar_shown = false;
    return layout;
  }

  const int game_area_w = client_width - kSidebarWidth;
  // 3:2 fit inside the game area.
  const double game_aspect = 3.0 / 2.0;
  const double area_aspect = static_cast<double>(game_area_w) / client_height;
  if (area_aspect > game_aspect) {
    const int h = client_height;
    const int w = static_cast<int>(client_height * game_aspect + 0.5);
    layout.game = {(game_area_w - w) / 2, 0, w, h};
  } else {
    const int w = game_area_w;
    const int h = static_cast<int>(game_area_w / game_aspect + 0.5);
    layout.game = {0, (client_height - h) / 2, w, h};
  }
  layout.sidebar = {client_width - kSidebarWidth, 0, kSidebarWidth, client_height};
  layout.sidebar_shown = true;
  return layout;
}

namespace {

std::string format_speed(const SidebarData& d) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.0f%%  %.0f fps", d.emu_speed_pct, d.fps);
  return buffer;
}

}  // namespace

std::vector<std::pair<std::string, bool>> sidebar_panel_lines(const SidebarData& data) {
  std::vector<std::pair<std::string, bool>> lines;
  lines.emplace_back("TACTICAL CONSOLE", true);

  lines.emplace_back("MODE      " + std::string(data.rewinding ? "<< REWIND"
                                          : data.fast_forward ? ">> FAST FWD"
                                                              : "NORMAL"), false);
  lines.emplace_back("VIEW      " + std::string(data.in_map ? "MAP COMMAND" : "-"), false);
  lines.emplace_back("CURSOR    " + (data.cursor_valid
                                         ? ("(" + std::to_string(data.cursor_x) + ", " +
                                            std::to_string(data.cursor_y) + ")")
                                         : std::string("-")),
                     false);

  lines.emplace_back("TIME CONTROL", true);
  lines.emplace_back("UNDO      " + std::to_string(data.undo_depth) + "/" +
                         std::to_string(data.undo_capacity) + " orders (Ctrl+Z)",
                     false);
  lines.emplace_back("REWIND    " + std::to_string(data.rewind_snapshots) + " states, " +
                         std::to_string(static_cast<int>(data.rewind_window_seconds + 0.5)) +
                         "s window",
                     false);

  lines.emplace_back("ENGINE", true);
  lines.emplace_back("SPEED     " + format_speed(data), false);
  lines.emplace_back("FRAMES    " + std::to_string(data.frames_run), false);

  if (data.forecast.valid) {
    lines.emplace_back("FORECAST", true);
    lines.emplace_back(std::string(data.forecast.attacker_name) + " vs " +
                           data.forecast.defender_name,
                       false);
    lines.emplace_back("DMG " + std::to_string(data.forecast.min_damage) + "-" +
                           std::to_string(data.forecast.max_damage) + "%  CNTR " +
                           std::to_string(data.forecast.counter_damage) + "%",
                       false);
  }
  return lines;
}

}  // namespace aw
