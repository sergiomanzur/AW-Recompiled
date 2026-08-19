#include "aw/window.hpp"
#include <cassert>
#include <iostream>

int main() {
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

  // Test 3: 4:3 Window Mode (960x720)
  // 960x720 client rect -> 3:2 viewport is 960x640, vp_y = 40 (letterboxed top & bottom)
  {
    auto vp = aw::calculate_viewport_rect(960, 720, aw::AspectRatio::Ratio_4_3);
    assert(vp.x == 0);
    assert(vp.width == 960);
    assert(vp.height == 640);
    assert(vp.y == 40);
  }

  // Test 4: 16:9 Window Mode (1152x648)
  // 1152x648 client rect -> 3:2 viewport is 972x648, vp_x = 90 (pillarboxed left & right)
  {
    auto vp = aw::calculate_viewport_rect(1152, 648, aw::AspectRatio::Ratio_16_9);
    assert(vp.y == 0);
    assert(vp.height == 648);
    assert(vp.width == 972);
    assert(vp.x == 90);
  }

  // Test 5: 21:9 Window Mode (1260x540)
  // 1260x540 client rect -> 3:2 viewport is 810x540, vp_x = 225 (pillarboxed left & right)
  {
    auto vp = aw::calculate_viewport_rect(1260, 540, aw::AspectRatio::Ratio_21_9);
    assert(vp.y == 0);
    assert(vp.height == 540);
    assert(vp.width == 810);
    assert(vp.x == 225);
  }

  // Test 6: 21:10 Window Mode (1134x540)
  // 1134x540 client rect -> 3:2 viewport is 810x540, vp_x = 162 (pillarboxed left & right)
  {
    auto vp = aw::calculate_viewport_rect(1134, 540, aw::AspectRatio::Ratio_21_10);
    assert(vp.y == 0);
    assert(vp.height == 540);
    assert(vp.width == 810);
    assert(vp.x == 162);
  }

  std::cout << "All window aspect ratio tests passed!\n";
  return 0;
}
