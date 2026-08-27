#include "aw/render/pointer_overlay.hpp"

#include <cstddef>

namespace aw {

namespace {

// The pointer arrow. Row 0's leading 'X' is the hotspot: it lands exactly on
// the (x, y) the caller passes. 'X' = outline, 'O' = fill, ' ' = leave the
// framebuffer untouched at that pixel (there is no transparency channel;
// a space simply means "don't write here").
const char* const kArrowRows[] = {
    "X",
    "XX",
    "XOX",
    "XOOX",
    "XOOOX",
    "XOOOOX",
    "XOOOOOX",
    "XOOOOOOX",
    "XOOOOOOOX",
    "XOOOOXXXXX",
    "XOOX XOX",
    "XOX   XOX",
    "XX    XOX",
    "X      XX",
};
constexpr int kArrowRowCount = sizeof(kArrowRows) / sizeof(kArrowRows[0]);

// Pure black and pure white: identical under any R/B channel ordering, so a
// caller that byte-swaps channels (main.cpp does, for GDI) doesn't need any
// special handling here.
constexpr std::uint32_t kOutlineColor = 0x00000000u;
constexpr std::uint32_t kFillColor = 0x00FFFFFFu;

}  // namespace

void draw_pointer(std::uint32_t* framebuffer, int width, int height, int x, int y) {
  if (framebuffer == nullptr || width <= 0 || height <= 0) return;

  for (int row = 0; row < kArrowRowCount; ++row) {
    const int fy = y + row;
    if (fy < 0 || fy >= height) continue;

    const char* line = kArrowRows[row];
    for (int col = 0; line[col] != '\0'; ++col) {
      const char c = line[col];
      if (c == ' ') continue;

      const int fx = x + col;
      if (fx < 0 || fx >= width) continue;

      framebuffer[static_cast<std::size_t>(fy) * static_cast<std::size_t>(width) +
                  static_cast<std::size_t>(fx)] = (c == 'X') ? kOutlineColor : kFillColor;
    }
  }
}

void draw_rts_tile_reticle(std::uint32_t* framebuffer, int width, int height,
                           int tile_x, int tile_y, int scroll_x, int scroll_y,
                           std::uint32_t color) {
  if (framebuffer == nullptr || width <= 0 || height <= 0) return;

  const int sx = tile_x * 16 - scroll_x;
  const int sy = tile_y * 16 - scroll_y;
  constexpr int kTileSize = 16;
  constexpr int kArmLen = 4;

  if (sx + kTileSize <= 0 || sx >= width || sy + kTileSize <= 0 || sy >= height) return;

  auto put_pixel = [&](int px, int py, std::uint32_t c) {
    if (px >= 0 && px < width && py >= 0 && py < height) {
      framebuffer[static_cast<std::size_t>(py) * static_cast<std::size_t>(width) +
                  static_cast<std::size_t>(px)] = c;
    }
  };

  // Top-left corner
  for (int i = 0; i < kArmLen; ++i) {
    put_pixel(sx + i, sy, color);
    put_pixel(sx, sy + i, color);
  }
  // Top-right corner
  for (int i = 0; i < kArmLen; ++i) {
    put_pixel(sx + kTileSize - 1 - i, sy, color);
    put_pixel(sx + kTileSize - 1, sy + i, color);
  }
  // Bottom-left corner
  for (int i = 0; i < kArmLen; ++i) {
    put_pixel(sx + i, sy + kTileSize - 1, color);
    put_pixel(sx, sy + kTileSize - 1 - i, color);
  }
  // Bottom-right corner
  for (int i = 0; i < kArmLen; ++i) {
    put_pixel(sx + kTileSize - 1 - i, sy + kTileSize - 1, color);
    put_pixel(sx + kTileSize - 1, sy + kTileSize - 1 - i, color);
  }
}

}  // namespace aw
