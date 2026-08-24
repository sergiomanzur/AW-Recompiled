#pragma once

#include "aw/memory.hpp"
#include <cstdint>
#include <vector>

namespace aw {

constexpr int kGbaWidth = 240;
constexpr int kGbaHeight = 160;

struct Ppu {
  int width = kGbaWidth;   // 240
  int height = kGbaHeight; // 160

  std::vector<std::uint32_t> framebuffer;

  Ppu() : framebuffer(kGbaWidth * kGbaHeight, 0xFF000000u) {}

  static std::uint32_t bgr555_to_rgba32(std::uint16_t bgr);

  void render_scanline(Memory& memory, int scanline);
  void render_frame(Memory& memory);
};

}  // namespace aw
