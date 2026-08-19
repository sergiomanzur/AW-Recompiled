#include "aw/ppu.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace aw {

std::uint32_t Ppu::bgr555_to_rgba32(std::uint16_t bgr) {
  const std::uint32_t r = (bgr & 0x1F) << 3;
  const std::uint32_t g = ((bgr >> 5) & 0x1F) << 3;
  const std::uint32_t b = ((bgr >> 10) & 0x1F) << 3;
  // Windows BI_RGB 32bpp: BGRX in memory, which is 0x00RRGGBB as uint32_t on LE
  return 0xFF000000u | (r << 16) | (g << 8) | b;
}

namespace {

struct PixelInfo {
  std::uint32_t color = 0;
  int priority = 4;
  bool is_opaque = false;
};

void get_obj_size(int shape, int size, int& w, int& h) {
  static const int sizes[3][4][2] = {
      {{8, 8}, {16, 16}, {32, 32}, {64, 64}},    // Square
      {{16, 8}, {32, 8}, {32, 16}, {64, 32}},   // Horizontal
      {{8, 16}, {8, 32}, {16, 32}, {32, 64}}    // Vertical
  };
  shape = std::clamp(shape, 0, 2);
  size = std::clamp(size, 0, 3);
  w = sizes[shape][size][0];
  h = sizes[shape][size][1];
}

}  // namespace

void Ppu::render_scanline(Memory& memory, int scanline) {
  if (scanline < 0 || scanline >= kGbaHeight) return;

  const std::uint16_t dispcnt = read16(memory, 0x04000000);
  const std::uint16_t backdrop_bgr = read16(memory, 0x05000000);
  const std::uint32_t backdrop_color = bgr555_to_rgba32(backdrop_bgr);

  std::array<PixelInfo, kGbaWidth> line_pixels;
  for (int x = 0; x < kGbaWidth; ++x) {
    line_pixels[x].color = backdrop_color;
    line_pixels[x].priority = 4;
    line_pixels[x].is_opaque = false;
  }

  // Forced blank
  if (dispcnt & (1 << 7)) {
    for (int x = 0; x < kGbaWidth; ++x) {
      framebuffer[scanline * kGbaWidth + x] = 0xFFFFFFFFu;
    }
    return;
  }

  const int bg_mode = dispcnt & 7;

  // Render Backgrounds (Mode 0/1/2)
  for (int bg = 3; bg >= 0; --bg) {
    if (!(dispcnt & (1 << (8 + bg)))) continue;
    if (bg_mode != 0 && bg >= 2) continue; // Mode 1/2 restrictions

    const std::uint16_t bgcnt = read16(memory, 0x04000008 + bg * 2);
    const int priority = bgcnt & 3;
    const std::uint32_t char_base = ((bgcnt >> 2) & 3) * 0x4000;
    const bool is_8bpp = (bgcnt & (1 << 7)) != 0;
    const std::uint32_t screen_base = ((bgcnt >> 8) & 0x1F) * 0x800;
    const int screen_size = (bgcnt >> 14) & 3;

    const std::uint16_t hofs = read16(memory, 0x04000010 + bg * 4) & 0x1FF;
    const std::uint16_t vofs = read16(memory, 0x04000012 + bg * 4) & 0x1FF;

    const int py = (scanline + vofs) & 0x1FF;

    for (int x = 0; x < kGbaWidth; ++x) {
      const int px = (x + hofs) & 0x1FF;

      int tile_x = px / 8;
      int tile_y = py / 8;

      int map_block = 0;
      if (screen_size == 1) { // 64x32
        if (tile_x >= 32) { map_block = 1; tile_x -= 32; }
      } else if (screen_size == 2) { // 32x64
        if (tile_y >= 32) { map_block = 1; tile_y -= 32; }
      } else if (screen_size == 3) { // 64x64
        if (tile_x >= 32) { map_block += 1; tile_x -= 32; }
        if (tile_y >= 32) { map_block += 2; tile_y -= 32; }
      }

      const std::uint32_t map_addr = 0x06000000 + screen_base + map_block * 0x800 + (tile_y * 32 + tile_x) * 2;
      const std::uint16_t entry = read16(memory, map_addr);

      const std::uint32_t tile_idx = entry & 0x3FF;
      const bool hflip = (entry & (1 << 10)) != 0;
      const bool vflip = (entry & (1 << 11)) != 0;
      const std::uint32_t palette_bank = (entry >> 12) & 0xF;

      int fine_x = px % 8;
      int fine_y = py % 8;
      if (hflip) fine_x = 7 - fine_x;
      if (vflip) fine_y = 7 - fine_y;

      std::uint8_t color_idx = 0;
      if (!is_8bpp) { // 4bpp
        const std::uint32_t tile_addr = 0x06000000 + char_base + tile_idx * 32 + fine_y * 4 + fine_x / 2;
        const std::uint8_t byte = read8(memory, tile_addr);
        color_idx = (fine_x % 2 == 0) ? (byte & 0xF) : (byte >> 4);
      } else { // 8bpp
        const std::uint32_t tile_addr = 0x06000000 + char_base + tile_idx * 64 + fine_y * 8 + fine_x;
        color_idx = read8(memory, tile_addr);
      }

      if (color_idx != 0) {
        std::uint32_t pal_addr = 0x05000000;
        if (!is_8bpp) {
          pal_addr += (palette_bank * 16 + color_idx) * 2;
        } else {
          pal_addr += color_idx * 2;
        }

        const std::uint16_t bgr = read16(memory, pal_addr);
        if (priority <= line_pixels[x].priority) {
          line_pixels[x].color = bgr555_to_rgba32(bgr);
          line_pixels[x].priority = priority;
          line_pixels[x].is_opaque = true;
        }
      }
    }
  }

  // Render Sprites (OBJ)
  if (dispcnt & (1 << 12)) {
    const bool mapping_1d = (dispcnt & (1 << 6)) != 0;

    for (int i = 127; i >= 0; --i) {
      const std::uint32_t oam_addr = 0x07000000 + i * 8;
      const std::uint16_t attr0 = read16(memory, oam_addr);
      const std::uint16_t attr1 = read16(memory, oam_addr + 2);
      const std::uint16_t attr2 = read16(memory, oam_addr + 4);

      const bool rot_scale = (attr0 & (1 << 8)) != 0;
      const bool disabled = !rot_scale && ((attr0 & (1 << 9)) != 0);
      if (disabled) continue;

      const int shape = (attr0 >> 14) & 3;
      const int size = (attr1 >> 14) & 3;
      int obj_w = 8, obj_h = 8;
      get_obj_size(shape, size, obj_w, obj_h);

      int sy = attr0 & 0xFF;
      if (sy >= 160) sy -= 256;

      if (scanline < sy || scanline >= sy + obj_h) continue;

      int sx = attr1 & 0x1FF;
      if (sx >= 240) sx -= 512;

      const bool is_8bpp = (attr0 & (1 << 13)) != 0;
      const bool hflip = !rot_scale && ((attr1 & (1 << 12)) != 0);
      const bool vflip = !rot_scale && ((attr1 & (1 << 13)) != 0);
      const int obj_priority = (attr2 >> 10) & 3;
      const std::uint32_t tile_base = attr2 & 0x3FF;
      const std::uint32_t palette_bank = (attr2 >> 12) & 0xF;

      int py = scanline - sy;
      if (vflip) py = obj_h - 1 - py;

      for (int px = 0; px < obj_w; ++px) {
        const int screen_x = sx + px;
        if (screen_x < 0 || screen_x >= kGbaWidth) continue;

        int eff_px = px;
        if (hflip) eff_px = obj_w - 1 - eff_px;

        const int tile_x = eff_px / 8;
        const int tile_y = py / 8;
        const int fine_x = eff_px % 8;
        const int fine_y = py % 8;

        std::uint32_t tile_num = 0;
        if (mapping_1d) {
          tile_num = tile_base + (tile_y * (obj_w / 8) + tile_x) * (is_8bpp ? 2 : 1);
        } else {
          tile_num = (tile_base & ~0x1F) + ((tile_base + tile_y * 32 + tile_x) & 0x3FF);
        }

        std::uint8_t color_idx = 0;
        if (!is_8bpp) {
          const std::uint32_t tile_addr = 0x06010000 + tile_num * 32 + fine_y * 4 + fine_x / 2;
          const std::uint8_t byte = read8(memory, tile_addr);
          color_idx = (fine_x % 2 == 0) ? (byte & 0xF) : (byte >> 4);
        } else {
          const std::uint32_t tile_addr = 0x06010000 + tile_num * 32 + fine_y * 8 + fine_x;
          color_idx = read8(memory, tile_addr);
        }

        if (color_idx != 0) {
          std::uint32_t pal_addr = 0x05000200;
          if (!is_8bpp) pal_addr += (palette_bank * 16 + color_idx) * 2;
          else pal_addr += color_idx * 2;

          const std::uint16_t bgr = read16(memory, pal_addr);
          if (obj_priority <= line_pixels[screen_x].priority) {
            line_pixels[screen_x].color = bgr555_to_rgba32(bgr);
            line_pixels[screen_x].priority = obj_priority;
            line_pixels[screen_x].is_opaque = true;
          }
        }
      }
    }
  }

  for (int x = 0; x < kGbaWidth; ++x) {
    framebuffer[scanline * kGbaWidth + x] = line_pixels[x].color;
  }
}

void Ppu::render_frame(Memory& memory) {
  for (int y = 0; y < kGbaHeight; ++y) {
    render_scanline(memory, y);
  }
}

}  // namespace aw
