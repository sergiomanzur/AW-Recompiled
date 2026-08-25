#include "aw/render/sidebar.hpp"
#include "aw/tactical_intel.hpp"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

void require(bool condition, const std::string& label) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label.c_str());
  }
}

void require_eq(int actual, int expected, const std::string& label) {
  if (actual != expected) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s: expected %d, got %d\n", label.c_str(), expected, actual);
  }
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

void test_layout_splits_16_by_9() {
  const auto layout = aw::calculate_sidebar_layout(1152, 648, true, aw::AspectRatio::Ratio_16_9);
  require(layout.sidebar_shown, "sidebar shown in 16:9");
  require_eq(layout.sidebar.x, 1152 - aw::kSidebarWidth, "sidebar pinned right");
  require_eq(layout.sidebar.width, aw::kSidebarWidth, "sidebar width");
  require_eq(layout.game.x + layout.game.width + aw::kSidebarWidth, 1152,
             "game area plus sidebar fills width");
  // Game rect is 3:2: 872 wide x 648 high -> 3:2 fit is 972 > 872, so
  // width-bound: 872 x ~581.
  require_eq(layout.game.width, 1152 - aw::kSidebarWidth, "game area width");
  require_eq(layout.game.height, static_cast<int>((1152 - aw::kSidebarWidth) / 1.5 + 0.5),
             "game rect height is 3:2");
}

void test_layout_hides_when_disabled_or_stretch() {
  const auto off = aw::calculate_sidebar_layout(1152, 648, false, aw::AspectRatio::Ratio_16_9);
  require(!off.sidebar_shown, "disabled sidebar hidden");
  // With the sidebar off, the window's own aspect setting governs: a 16:9
  // window in 16:9 mode fills the client (the window's stretch semantics).
  require_eq(off.game.width, 1152, "falls back to the aspect viewport");

  const auto stretch = aw::calculate_sidebar_layout(1152, 648, true, aw::AspectRatio::Stretch);
  require(!stretch.sidebar_shown, "no sidebar in Stretch mode");
  require_eq(stretch.game.width, 1152, "Stretch fills client");
}

void test_layout_hides_when_too_narrow() {
  const auto narrow = aw::calculate_sidebar_layout(600, 648, true, aw::AspectRatio::Ratio_16_9);
  require(!narrow.sidebar_shown, "no sidebar on narrow clients");
}

void test_layout_zero_client_is_safe() {
  const auto empty = aw::calculate_sidebar_layout(0, 0, true, aw::AspectRatio::Ratio_16_9);
  require(!empty.sidebar_shown, "degenerate client hidden");
  require_eq(empty.game.width, 0, "degenerate game rect");
}

void test_panel_lines_show_real_data() {
  aw::SidebarData data;
  data.in_map = true;
  data.cursor_valid = true;
  data.cursor_x = 12;
  data.cursor_y = 7;
  data.undo_depth = 3;
  data.undo_capacity = 10;
  data.rewind_snapshots = 14;
  data.rewind_window_seconds = 5.0;
  data.fps = 431.0;
  data.emu_speed_pct = 721.0;
  data.frames_run = 12345;

  const auto lines = aw::sidebar_panel_lines(data);
  require(!lines.empty(), "panel has lines");
  std::string all;
  for (const auto& [text, heading] : lines) {
    all += text;
    all += '\n';
    (void)heading;
  }
  require(contains(all, "MAP COMMAND"), "map mode shown");
  require(contains(all, "(12, 7)"), "cursor tile shown");
  require(contains(all, "3/10 orders"), "undo depth shown");
  require(contains(all, "14 states"), "rewind depth shown");
  require(contains(all, "721%"), "speed shown");
  require(contains(all, "12345"), "frame count shown");
  require(!contains(all, "FORECAST"), "forecast hidden without data");
}

void test_panel_lines_forecast_when_valid() {
  aw::SidebarData data;
  data.forecast = aw::compute_forecast(3, 1, 10, 2);  // Tank vs Mech on 2 stars
  const auto lines = aw::sidebar_panel_lines(data);
  std::string all;
  for (const auto& [text, heading] : lines) {
    all += text;
    all += '\n';
  }
  require(contains(all, "TANK vs MECH"), "forecast names shown");
  require(contains(all, "DMG 52-62%"), "forecast damage shown");
}

void test_forecast_math() {
  // Tank (base 70) vs Mech on 2 terrain stars: 70 * 10 * 80 / 1000 = 56,
  // displayed as 52-62 after the +/- spread.
  const auto fc = aw::compute_forecast(3, 1, 10, 2);
  require(fc.valid, "forecast valid");
  require_eq(fc.min_damage, 52, "min damage");
  require_eq(fc.max_damage, 62, "max damage");

  // Infantry cannot shoot a Fighter: no damage line at all.
  const auto zero = aw::compute_forecast(0, 12, 10, 0);
  require(zero.valid, "zero-damage forecast still valid");
  require_eq(zero.min_damage, 0, "no damage vs air");
  require_eq(zero.counter_damage, 0, "no counter vs air");
}

void test_intel_defaults_are_invalid() {
  aw::TacticalIntel intel;
  require(!intel.active_co().valid, "CO invalid by default");
  require(!intel.hovered_unit().valid, "hovered unit invalid by default");
  require(!intel.forecast().valid, "forecast invalid by default");
  require(!intel.cursor_valid(), "cursor invalid by default");
  require(!intel.in_gameplay(), "not in gameplay by default");
}

}  // namespace

int main() {
  test_layout_splits_16_by_9();
  test_layout_hides_when_disabled_or_stretch();
  test_layout_hides_when_too_narrow();
  test_layout_zero_client_is_safe();
  test_panel_lines_show_real_data();
  test_panel_lines_forecast_when_valid();
  test_forecast_math();
  test_intel_defaults_are_invalid();

  if (failures == 0) {
    std::printf("sidebar_tests: all tests passed\n");
    return 0;
  }
  std::fprintf(stderr, "sidebar_tests: %d failure(s)\n", failures);
  return 1;
}
