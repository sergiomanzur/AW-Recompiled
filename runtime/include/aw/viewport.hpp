#pragma once

#include <cstdint>

namespace aw {

// Window geometry shared by the window layer and the sidebar. Kept in its
// own header so sidebar.hpp and window.hpp can compose without a cycle.

enum class AspectRatio {
  Original_3_2,  // 3:2 Window Mode (960x640)
  Ratio_4_3,     // 4:3 Window Mode (960x720)
  Ratio_16_9,    // 16:9 Window Mode (1152x648)
  Ratio_21_9,    // 21:9 Window Mode (1260x540)
  Ratio_21_10,   // 21:10 Window Mode (1134x540)
  Stretch        // Fill Window without aspect lock
};

struct ViewportRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

// Pure calculation function for letterbox/pillarbox viewport geometry
ViewportRect calculate_viewport_rect(int client_width, int client_height, AspectRatio ratio);

// Nearest-neighbour integer pre-scale of a framebuffer by factor k
// (k >= 1). The Internal Resolution setting runs the game image through
// this before the window stretch, trading a bigger blit for crisper
// downscale behaviour at large window sizes.
void apply_nearest_k(const std::uint32_t* src, std::uint32_t* dst, int w, int h, int k);

}  // namespace aw
