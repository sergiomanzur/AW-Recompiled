#pragma once

#include <cstdint>

namespace aw {

// Draws the mouse pointer into a 32-bit framebuffer, clipped to its bounds.
// Pure and platform-neutral: the caller supplies the buffer, so this is
// testable without a window, and reusable by the SDL and Android backends.
// The arrow's tip is the hotspot: it lands exactly on (x, y).
//
// A null framebuffer or non-positive width/height is a no-op. Any part of
// the arrow that would fall outside [0, width) x [0, height) is clipped
// pixel-by-pixel rather than skipped wholesale, so the pointer can be drawn
// right at the edge of the screen safely.
// Draws the mouse/touch pointer arrow into a 32-bit framebuffer.
void draw_pointer(std::uint32_t* framebuffer, int width, int height, int x, int y);

// Draws a tactical Age of Empires style 16x16 tile corner reticle on the battlefield.
void draw_rts_tile_reticle(std::uint32_t* framebuffer, int width, int height, int tile_x, int tile_y, int scroll_x, int scroll_y, std::uint32_t color = 0x00F5A623u);

}  // namespace aw
