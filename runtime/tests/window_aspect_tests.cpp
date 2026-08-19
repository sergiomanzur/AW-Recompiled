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

  // Test 2: Original 3:2 aspect ratio in 16:9 window (1920x1080)
  // Target aspect = 1.5. Client aspect = 1.7778 -> Pillarboxing (vp_height = 1080, vp_width = 1620, vp_x = 150)
  {
    auto vp = aw::calculate_viewport_rect(1920, 1080, aw::AspectRatio::Original_3_2);
    assert(vp.y == 0);
    assert(vp.height == 1080);
    assert(vp.width == 1620);
    assert(vp.x == 150);
  }

  // Test 3: 4:3 aspect ratio in 16:9 window (1920x1080)
  // Target aspect = 1.333333. Client aspect = 1.7778 -> Pillarboxing (vp_height = 1080, vp_width = 1440, vp_x = 240)
  {
    auto vp = aw::calculate_viewport_rect(1920, 1080, aw::AspectRatio::Ratio_4_3);
    assert(vp.y == 0);
    assert(vp.height == 1080);
    assert(vp.width == 1440);
    assert(vp.x == 240);
  }

  // Test 4: 16:9 aspect ratio in 4:3 window (1024x768)
  // Target aspect = 16/9 = 1.7778. Client aspect = 1.3333 -> Letterboxing (vp_width = 1024, vp_height = 576, vp_y = 96)
  {
    auto vp = aw::calculate_viewport_rect(1024, 768, aw::AspectRatio::Ratio_16_9);
    assert(vp.x == 0);
    assert(vp.width == 1024);
    assert(vp.height == 576);
    assert(vp.y == 96);
  }

  // Test 5: 21:9 aspect ratio in 16:9 window (1920x1080)
  // Target aspect = 21/9 = 2.3333. Client aspect = 1.7778 -> Letterboxing
  {
    auto vp = aw::calculate_viewport_rect(1920, 1080, aw::AspectRatio::Ratio_21_9);
    assert(vp.x == 0);
    assert(vp.width == 1920);
    assert(vp.height == static_cast<int>(1920.0 / (21.0 / 9.0) + 0.5));
    assert(vp.y == (1080 - vp.height) / 2);
  }

  // Test 6: 21:10 aspect ratio in 16:9 window (1920x1080)
  // Target aspect = 21/10 = 2.10. Client aspect = 1.7778 -> Letterboxing
  {
    auto vp = aw::calculate_viewport_rect(1920, 1080, aw::AspectRatio::Ratio_21_10);
    assert(vp.x == 0);
    assert(vp.width == 1920);
    assert(vp.height == static_cast<int>(1920.0 / (21.0 / 10.0) + 0.5));
    assert(vp.y == (1080 - vp.height) / 2);
  }

  std::cout << "All window aspect ratio tests passed!\n";
  return 0;
}
