#include "aw/window.hpp"
#include <cassert>
#include <iostream>

int main() {
  // Nearest-k pre-scale: each source pixel becomes a k*k block.
  {
    std::uint32_t src[2] = {0xFFAA0044u, 0xFF112233u};
    std::uint32_t dst[8] = {};
    aw::apply_nearest_k(src, dst, 2, 1, 2);
    assert(dst[0] == 0xFFAA0044u && dst[1] == 0xFFAA0044u);
    assert(dst[2] == 0xFF112233u && dst[3] == 0xFF112233u);
    assert(dst[4] == 0xFFAA0044u && dst[5] == 0xFFAA0044u);
    assert(dst[6] == 0xFF112233u && dst[7] == 0xFF112233u);

    std::uint32_t one[2] = {};
    aw::apply_nearest_k(src, one, 2, 1, 1);
    assert(one[0] == src[0] && one[1] == src[1]);

    aw::apply_nearest_k(nullptr, one, 2, 1, 1);
    aw::apply_nearest_k(src, nullptr, 2, 1, 1);
    aw::apply_nearest_k(src, one, 2, 1, 0);
  }

  std::cout << "Running window aspect ratio tests...\n";

  // Test 1: Stretch mode always fills client rect
  {
    auto vp = aw::calculate_viewport_rect(1920, 1080, aw::AspectRatio::Stretch);
    assert(vp.x == 0);
    assert(vp.y == 0);
    assert(vp.width == 1920);
    assert(vp.height == 1080);
  }

  // Test 2: Original 3:2 mode in 16:9 window (1920x1080)
  // Target aspect = 1.5. Client aspect = 1.7778 -> Pillarboxing (vp_height = 1080, vp_width = 1620, vp_x = 150)
  {
    auto vp = aw::calculate_viewport_rect(1920, 1080, aw::AspectRatio::Original_3_2);
    assert(vp.y == 0);
    assert(vp.height == 1080);
    assert(vp.width == 1620);
    assert(vp.x == 150);
  }

  // Test 3: 4:3 Aspect Ratio in 960x720 client rect (exact 4:3 match)
  {
    auto vp = aw::calculate_viewport_rect(960, 720, aw::AspectRatio::Ratio_4_3);
    assert(vp.x == 0);
    assert(vp.y == 0);
    assert(vp.width == 960);
    assert(vp.height == 720);
  }

  // Test 4: 16:9 Aspect Ratio in 1152x648 client rect (exact 16:9 match)
  {
    auto vp = aw::calculate_viewport_rect(1152, 648, aw::AspectRatio::Ratio_16_9);
    assert(vp.x == 0);
    assert(vp.y == 0);
    assert(vp.width == 1152);
    assert(vp.height == 648);
  }

  // Test 5: 21:9 Aspect Ratio in 1260x540 client rect (exact 21:9 match)
  {
    auto vp = aw::calculate_viewport_rect(1260, 540, aw::AspectRatio::Ratio_21_9);
    assert(vp.x == 0);
    assert(vp.y == 0);
    assert(vp.width == 1260);
    assert(vp.height == 540);
  }

  // Test 6: 21:10 Aspect Ratio in 1134x540 client rect (exact 21:10 match)
  {
    auto vp = aw::calculate_viewport_rect(1134, 540, aw::AspectRatio::Ratio_21_10);
    assert(vp.x == 0);
    assert(vp.y == 0);
    assert(vp.width == 1134);
    assert(vp.height == 540);
  }

  std::cout << "All window aspect ratio tests passed!\n";
  return 0;
}
