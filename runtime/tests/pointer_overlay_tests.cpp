#include "aw/render/pointer_overlay.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

constexpr std::uint32_t kOutlineColor = 0x00000000u;
constexpr std::uint32_t kFillColor = 0x00FFFFFFu;
constexpr std::uint32_t kSentinel = 0xDEADBEEFu;

void tests_tip_pixel_is_outline_colour() {
  const int width = 32;
  const int height = 32;
  std::vector<std::uint32_t> fb(static_cast<std::size_t>(width) * height, kSentinel);

  aw::draw_pointer(fb.data(), width, height, 10, 10);

  require_equal(fb[10 * width + 10], kOutlineColor, "tip pixel at (x, y)");
}

void tests_pixel_outside_bounding_box_is_untouched() {
  const int width = 50;
  const int height = 50;
  std::vector<std::uint32_t> fb(static_cast<std::size_t>(width) * height, kSentinel);

  // Arrow drawn at (45, 5): bounding box is roughly x in [45, 54], y in
  // [5, 18]. A pixel at (0, 3) is nowhere near it.
  aw::draw_pointer(fb.data(), width, height, 45, 5);

  require_equal(fb[3 * width + 0], kSentinel, "pixel far outside the arrow's bounding box");

  // Same draw doubles as the regression check for M7 (removing the
  // horizontal clip): rows 5-13 of the arrow are wide enough that some of
  // their columns land at x >= 50 when drawn at x=45. Without the `fx >=
  // width` clip, computing a flat index from an out-of-range fx wraps into
  // the start of the *next* scanline instead of being dropped, corrupting
  // pixels near column 0 on rows 11-13 (one past each offending row). With
  // the clip intact, none of that happens and they stay sentinel.
  require_equal(fb[11 * width + 0], kSentinel, "wraparound target for row 5 (catches M7)");
  require_equal(fb[12 * width + 0], kSentinel, "wraparound target for row 6 (catches M7)");
  require_equal(fb[13 * width + 0], kSentinel, "wraparound target for row 7 (catches M7)");
}

void tests_bottom_right_corner_draws_only_the_tip() {
  const int width = 20;
  const int height = 20;
  std::vector<std::uint32_t> fb(static_cast<std::size_t>(width) * height, kSentinel);

  aw::draw_pointer(fb.data(), width, height, width - 1, height - 1);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::uint32_t value = fb[static_cast<std::size_t>(y) * width + x];
      if (x == width - 1 && y == height - 1) {
        require_equal(value, kOutlineColor, "tip drawn at the bottom-right corner");
      } else {
        require_equal(value, kSentinel, "neighbour of the bottom-right corner tip");
      }
    }
  }
}

void tests_negative_position_clips_correctly() {
  const int width = 20;
  const int height = 20;

  // Entirely off the top-left: nothing at all should be written.
  {
    std::vector<std::uint32_t> fb(static_cast<std::size_t>(width) * height, kSentinel);
    aw::draw_pointer(fb.data(), width, height, -100, -100);
    for (std::size_t i = 0; i < fb.size(); ++i) {
      require_equal(fb[i], kSentinel, "fully off-screen draw touches nothing");
    }
  }

  // Partially off the top-left: only the in-bounds tail of the rows that
  // reach y=0 and y=1 survives.
  {
    std::vector<std::uint32_t> fb(static_cast<std::size_t>(width) * height, kSentinel);
    aw::draw_pointer(fb.data(), width, height, -3, -3);

    // Row 3 ("XOOX") is the first row whose fy = -3 + row lands at 0.
    // Columns 0-2 fall off the left edge (fx = -3, -2, -1); column 3 ('X')
    // lands at fx = 0.
    require_equal(fb[0 * width + 0], kOutlineColor, "row 3's only surviving column");

    // Row 4 ("XOOOX") lands at fy = 1. Column 3 ('O') lands at fx = 0,
    // column 4 ('X') lands at fx = 1.
    require_equal(fb[1 * width + 0], kFillColor, "row 4's surviving fill pixel");
    require_equal(fb[1 * width + 1], kOutlineColor, "row 4's surviving outline pixel");

    // A pixel far from the clipped-in corner is untouched.
    require_equal(fb[15 * width + 15], kSentinel, "distant pixel untouched by clipped negative draw");
  }
}

void tests_null_and_zero_size_are_noops() {
  // Must not crash.
  aw::draw_pointer(nullptr, 10, 10, 5, 5);

  std::vector<std::uint32_t> fb(16, kSentinel);

  aw::draw_pointer(fb.data(), 0, 4, 1, 1);
  aw::draw_pointer(fb.data(), 4, 0, 1, 1);
  aw::draw_pointer(fb.data(), -1, 4, 1, 1);
  aw::draw_pointer(fb.data(), 4, -1, 1, 1);

  for (std::size_t i = 0; i < fb.size(); ++i) {
    require_equal(fb[i], kSentinel, "non-positive width/height is a no-op");
  }
}

}  // namespace

int main() {
  try {
    tests_tip_pixel_is_outline_colour();
    tests_pixel_outside_bounding_box_is_untouched();
    tests_bottom_right_corner_draws_only_the_tip();
    tests_negative_position_clips_correctly();
    tests_null_and_zero_size_are_noops();
    std::cout << "pointer_overlay_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
