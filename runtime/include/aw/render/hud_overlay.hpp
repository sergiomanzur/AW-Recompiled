#pragma once

#include "aw/tactical_intel.hpp"
#include <cstdint>
#include <vector>

namespace aw {

// Draws a native C++ Tactical Intel HUD overlay directly into the framebuffer
void draw_hud_overlay(std::uint32_t* framebuffer, int fb_width, int fb_height,
                      const TacticalIntel& intel, bool widescreen_mode);

// Speedrun-style status strip at the top-right of the framebuffer: the
// emulated frame counter since power-on plus one box per GBA button, lit
// while held. A REC/PLAY marker sits beside it during replays. Pure pixel
// drawing over whatever the game rendered this frame.
void draw_status_overlay(std::uint32_t* framebuffer, int fb_width, int fb_height,
                         std::uint16_t keys, std::uint64_t frame_count,
                         bool recording, bool playing);

}  // namespace aw
