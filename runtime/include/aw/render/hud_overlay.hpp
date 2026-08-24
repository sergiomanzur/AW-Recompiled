#pragma once

#include "aw/tactical_intel.hpp"
#include <cstdint>
#include <vector>

namespace aw {

// Draws a native C++ Tactical Intel HUD overlay directly into the framebuffer
void draw_hud_overlay(std::uint32_t* framebuffer, int fb_width, int fb_height,
                      const TacticalIntel& intel, bool widescreen_mode);

}  // namespace aw
