#include <exception>
#include <iostream>
#include <sstream>

#include "aw/hardware.hpp"
#include "aw/input/input_frame.hpp"
#include "aw/input/viewport.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

void tests_frame_starts_empty() {
  aw::InputFrame frame;
  require_equal(frame.gba_keys, std::uint16_t{0}, "keys start clear");
  require_equal(frame.pointer_count, std::size_t{0}, "no pointers");
  require_equal(frame.primary_pointer() == nullptr, true, "no primary pointer");
}

void tests_frame_clear_resets_everything() {
  aw::InputFrame frame;
  frame.gba_keys = aw::kKeyA;
  frame.device_dpad = aw::kKeyLeft;
  frame.pointer_count = 1;
  frame.pointers[0].kind = aw::PointerKind::Mouse;

  frame.clear();

  require_equal(frame.gba_keys, std::uint16_t{0}, "keys cleared");
  require_equal(frame.device_dpad, std::uint16_t{0}, "device dpad cleared");
  require_equal(frame.pointer_count, std::size_t{0}, "pointer count cleared");
  require_equal(frame.pointers[0].kind == aw::PointerKind::None, true, "pointer reset");
}

void tests_primary_pointer_is_first_active() {
  aw::InputFrame frame;
  frame.pointer_count = 2;
  frame.pointers[0].kind = aw::PointerKind::None;
  frame.pointers[1].kind = aw::PointerKind::Touch;
  frame.pointers[1].gba_x = 77;

  const aw::PointerState* p = frame.primary_pointer();
  require_equal(p != nullptr, true, "found a primary pointer");
  require_equal(p->gba_x, 77, "primary is the active one");
}

void tests_dpad_mask_covers_four_directions() {
  const std::uint16_t expected =
      aw::kKeyUp | aw::kKeyDown | aw::kKeyLeft | aw::kKeyRight;
  require_equal(aw::kDpadMask, expected, "dpad mask");
}

void tests_viewport_maps_corners_and_centre() {
  int gx = 0, gy = 0;

  // Exact 4x scale, no letterboxing: 960x640 viewport at origin.
  require_equal(aw::viewport_to_gba(0, 0, 960, 640, 0, 0, gx, gy), true, "top-left inside");
  require_equal(gx, 0, "top-left x");
  require_equal(gy, 0, "top-left y");

  require_equal(aw::viewport_to_gba(0, 0, 960, 640, 959, 639, gx, gy), true, "bottom-right inside");
  require_equal(gx, 239, "bottom-right x");
  require_equal(gy, 159, "bottom-right y");

  require_equal(aw::viewport_to_gba(0, 0, 960, 640, 480, 320, gx, gy), true, "centre inside");
  require_equal(gx, 120, "centre x");
  require_equal(gy, 80, "centre y");
}

void tests_viewport_rejects_outside_and_offsets() {
  int gx = 0, gy = 0;

  // Letterboxed: viewport starts at x=150.
  require_equal(aw::viewport_to_gba(150, 0, 1620, 1080, 149, 500, gx, gy), false, "left of viewport");
  require_equal(aw::viewport_to_gba(150, 0, 1620, 1080, 1770, 500, gx, gy), false, "right of viewport");
  require_equal(aw::viewport_to_gba(150, 0, 1620, 1080, 500, -1, gx, gy), false, "above viewport");
  require_equal(aw::viewport_to_gba(150, 0, 1620, 1080, 500, 1080, gx, gy), false, "below viewport");

  require_equal(aw::viewport_to_gba(150, 0, 1620, 1080, 150, 0, gx, gy), true, "viewport origin");
  require_equal(gx, 0, "offset origin x");
  require_equal(gy, 0, "offset origin y");

  require_equal(aw::viewport_to_gba(150, 0, 1620, 1080, 500, 1079, gx, gy), true, "last row inside");
  require_equal(gy, 159, "last row maps to bottom scanline");
}

void tests_viewport_rejects_degenerate() {
  int gx = 0, gy = 0;
  require_equal(aw::viewport_to_gba(0, 0, 0, 640, 5, 5, gx, gy), false, "zero width");
  require_equal(aw::viewport_to_gba(0, 0, 960, 0, 5, 5, gx, gy), false, "zero height");
}

}  // namespace

int main() {
  try {
    tests_frame_starts_empty();
    tests_frame_clear_resets_everything();
    tests_primary_pointer_is_first_active();
    tests_dpad_mask_covers_four_directions();
    tests_viewport_maps_corners_and_centre();
    tests_viewport_rejects_outside_and_offsets();
    tests_viewport_rejects_degenerate();
    std::cout << "input_core_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
