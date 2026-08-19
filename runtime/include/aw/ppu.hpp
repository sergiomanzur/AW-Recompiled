#pragma once

#include "aw/memory.hpp"
#include <array>
#include <cstdint>

namespace aw {

constexpr int kGbaWidth = 240;
constexpr int kGbaHeight = 160;

struct Ppu {
  std::array<std::uint32_t, kGbaWidth * kGbaHeight> framebuffer{};

  static std::uint32_t bgr555_to_rgba32(std::uint16_t bgr);

  void render_scanline(Memory& memory, int scanline);
  void render_frame(Memory& memory);
};

}  // namespace aw
