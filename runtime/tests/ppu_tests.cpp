#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/memory.hpp"
#include "aw/ppu.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

void tests_bgr555_color_conversion() {
  // Pure Red: BGR555 = 0x001F -> R=248, G=0, B=0 -> Windows ARGB: 0xFFF80000
  const std::uint32_t red = aw::Ppu::bgr555_to_rgba32(0x001F);
  require_equal(red, 0xFFF80000u, "red conversion");

  // Pure Green: BGR555 = 0x03E0 -> R=0, G=248, B=0 -> Windows ARGB: 0xFF00F800
  const std::uint32_t green = aw::Ppu::bgr555_to_rgba32(0x03E0);
  require_equal(green, 0xFF00F800u, "green conversion");

  // Pure Blue: BGR555 = 0x7C00 -> R=0, G=0, B=248 -> Windows ARGB: 0xFF0000F8
  const std::uint32_t blue = aw::Ppu::bgr555_to_rgba32(0x7C00);
  require_equal(blue, 0xFF0000F8u, "blue conversion");
}

void tests_mode0_bg_rendering() {
  aw::Memory memory;
  aw::Ppu ppu;

  // DISPCNT: Enable Mode 0 + BG0 (0x0100)
  aw::write16(memory, 0x04000000, 0x0100);

  // BG0CNT: Screen Base 0 (0x06000000), Char Base 0, 4bpp, 32x32 size
  aw::write16(memory, 0x04000008, 0x0000);

  // BG Palette Bank 0, Entry 1: Blue (0x7C00)
  aw::write16(memory, 0x05000002, 0x7C00);

  // Tile 1, pixel 0,0: Color 1 (low nibble 1) -> byte 0 = 0x01
  aw::write8(memory, 0x06000020, 0x01);

  // Screen Map (0x06000000): Tile 0,0 uses Tile Index 1
  aw::write16(memory, 0x06000000, 0x0001);

  ppu.render_scanline(memory, 0);

  require_equal(ppu.framebuffer[0], 0xFF0000F8u, "tile 1 pixel 0 color");
}

}  // namespace

int main() {
  try {
    tests_bgr555_color_conversion();
    tests_mode0_bg_rendering();
    std::cout << "ppu_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
