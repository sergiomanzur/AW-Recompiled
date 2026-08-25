#include "aw/render/hud_overlay.hpp"

#include "aw/hardware.hpp"

#include <algorithm>
#include <cstring>
#include <string>

namespace aw {

namespace {

// 5x7 Bitmap Font renderer
const std::uint8_t* get_font_glyph(char c) {
  static const std::uint8_t font[96][5] = {
      {0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
      {0x00, 0x00, 0x5F, 0x00, 0x00}, // '!'
      {0x00, 0x07, 0x00, 0x07, 0x00}, // '"'
      {0x14, 0x7F, 0x14, 0x7F, 0x14}, // '#'
      {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // '$'
      {0x23, 0x13, 0x08, 0x64, 0x62}, // '%'
      {0x36, 0x49, 0x56, 0x20, 0x50}, // '&'
      {0x00, 0x05, 0x03, 0x00, 0x00}, // '\''
      {0x00, 0x1C, 0x22, 0x41, 0x00}, // '('
      {0x00, 0x41, 0x22, 0x1C, 0x00}, // ')'
      {0x14, 0x08, 0x3E, 0x08, 0x14}, // '*'
      {0x08, 0x08, 0x3E, 0x08, 0x08}, // '+'
      {0x00, 0x50, 0x30, 0x00, 0x00}, // ','
      {0x08, 0x08, 0x08, 0x08, 0x08}, // '-'
      {0x00, 0x60, 0x60, 0x00, 0x00}, // '.'
      {0x20, 0x10, 0x08, 0x04, 0x02}, // '/'
      {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0'
      {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
      {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
      {0x21, 0x41, 0x45, 0x4B, 0x31}, // '3'
      {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
      {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
      {0x3C, 0x4A, 0x49, 0x49, 0x30}, // '6'
      {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
      {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
      {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9'
      {0x00, 0x36, 0x36, 0x00, 0x00}, // ':'
      {0x00, 0x56, 0x36, 0x00, 0x00}, // ';'
      {0x08, 0x14, 0x22, 0x41, 0x00}, // '<'
      {0x14, 0x14, 0x14, 0x14, 0x14}, // '='
      {0x00, 0x41, 0x22, 0x14, 0x08}, // '>'
      {0x02, 0x01, 0x51, 0x09, 0x06}, // '?'
      {0x32, 0x49, 0x79, 0x41, 0x3E}, // '@'
      {0x7C, 0x12, 0x11, 0x12, 0x7C}, // 'A'
      {0x7F, 0x49, 0x49, 0x49, 0x36}, // 'B'
      {0x3E, 0x41, 0x41, 0x41, 0x22}, // 'C'
      {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 'D'
      {0x7F, 0x49, 0x49, 0x49, 0x41}, // 'E'
      {0x7F, 0x09, 0x09, 0x09, 0x01}, // 'F'
      {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 'G'
      {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 'H'
      {0x00, 0x41, 0x7F, 0x41, 0x00}, // 'I'
      {0x20, 0x40, 0x41, 0x3F, 0x01}, // 'J'
      {0x7F, 0x08, 0x14, 0x22, 0x41}, // 'K'
      {0x7F, 0x40, 0x40, 0x40, 0x40}, // 'L'
      {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 'M'
      {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 'N'
      {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 'O'
      {0x7F, 0x09, 0x09, 0x09, 0x06}, // 'P'
      {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 'Q'
      {0x7F, 0x09, 0x19, 0x29, 0x46}, // 'R'
      {0x46, 0x49, 0x49, 0x49, 0x31}, // 'S'
      {0x01, 0x01, 0x7F, 0x01, 0x01}, // 'T'
      {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 'U'
      {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 'V'
      {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 'W'
      {0x63, 0x14, 0x08, 0x14, 0x63}, // 'X'
      {0x07, 0x08, 0x70, 0x08, 0x07}, // 'Y'
      {0x61, 0x51, 0x49, 0x45, 0x43}, // 'Z'
  };

  const int idx = static_cast<int>(c) - 32;
  if (idx >= 0 && idx < 96) {
    return font[idx];
  }
  return font[0];
}

void draw_rect(std::uint32_t* fb, int fb_w, int fb_h, int rx, int ry, int rw, int rh, std::uint32_t color) {
  for (int y = ry; y < ry + rh; ++y) {
    if (y < 0 || y >= fb_h) continue;
    for (int x = rx; x < rx + rw; ++x) {
      if (x < 0 || x >= fb_w) continue;
      fb[y * fb_w + x] = color;
    }
  }
}

void draw_text(std::uint32_t* fb, int fb_w, int fb_h, int tx, int ty, const std::string& text, std::uint32_t color) {
  int cur_x = tx;
  for (char c : text) {
    const std::uint8_t* glyph = get_font_glyph(c);
    for (int col = 0; col < 5; ++col) {
      const std::uint8_t line = glyph[col];
      for (int row = 0; row < 7; ++row) {
        if (line & (1 << row)) {
          const int px = cur_x + col;
          const int py = ty + row;
          if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
            fb[py * fb_w + px] = color;
          }
        }
      }
    }
    cur_x += 6;
  }
}

}  // namespace

void draw_hud_overlay(std::uint32_t* fb, int fb_w, int fb_h,
                      const TacticalIntel& intel, bool /*widescreen_mode*/) {
  if (fb == nullptr || fb_w <= 0 || fb_h <= 0) return;

  // Top Bar: only the verified cursor readout. CO/funds/turn stay hidden
  // until their addresses are mined (see data/symbols/README.md) - the HUD
  // never invents values.
  if (intel.cursor_valid()) {
    const int top_x = 4;
    const int top_y = 2;
    const int top_w = 96;
    const int top_h = 13;

    draw_rect(fb, fb_w, fb_h, top_x, top_y, top_w, top_h, 0xDD0B121Cu);
    draw_rect(fb, fb_w, fb_h, top_x, top_y, top_w, 1, 0xFF4A90E2u);
    draw_rect(fb, fb_w, fb_h, top_x, top_y + top_h - 1, top_w, 1, 0xFF4A90E2u);

    std::string top_text = "CUR " + std::to_string(intel.cursor_x()) + "," +
                           std::to_string(intel.cursor_y());
    draw_text(fb, fb_w, fb_h, top_x + 4, top_y + 3, top_text, 0xFF50E3C2u);
  }

  // Bottom Panel: unit stats & combat forecast, only when the data is real.
  const UnitIntel& unit = intel.hovered_unit();
  if (!unit.valid) return;

  const int bot_x = 4;
  const int bot_y = 122;
  const int bot_w = 232;
  const int bot_h = 36;

  // Dark panel with gold accent border
  draw_rect(fb, fb_w, fb_h, bot_x, bot_y, bot_w, bot_h, 0xDD0B121Cu);
  draw_rect(fb, fb_w, fb_h, bot_x, bot_y, bot_w, 1, 0xFFF5A623u);
  draw_rect(fb, fb_w, fb_h, bot_x, bot_y + bot_h - 1, bot_w, 1, 0xFFF5A623u);

  // Line 1: Unit Name, HP, Ammo, Fuel
  std::string line1 = unit.name + " [" + unit.army + "] HP:" + std::to_string(unit.hp) + "/10";
  if (unit.max_ammo > 0) {
    line1 += " AMMO:" + std::to_string(unit.ammo);
  }
  line1 += " FUEL:" + std::to_string(unit.fuel);
  draw_text(fb, fb_w, fb_h, bot_x + 4, bot_y + 4, line1, 0xFFFFFFFFu);

  // Line 2: Damage Calculator & Combat Forecast
  const DamageForecast& fc = intel.forecast();
  if (fc.valid) {
    std::string line2 = "DMG VS " + fc.defender_name + ": " + std::to_string(fc.min_damage) + "%-" +
                        std::to_string(fc.max_damage) + "% | CNTR: " + std::to_string(fc.counter_damage) + "%";
    draw_text(fb, fb_w, fb_h, bot_x + 4, bot_y + 15, line2, 0xFFF5A623u);
  }

  // Line 3: Terrain Defense & Movement
  std::string line3 = "TERRAIN: " + unit.terrain_name + " | MOVE: " + std::to_string(unit.move_range) + " (" + unit.move_type + ")";
  draw_text(fb, fb_w, fb_h, bot_x + 4, bot_y + 26, line3, 0xFF7ED321u);
}

void draw_status_overlay(std::uint32_t* fb, int fb_w, int fb_h,
                         std::uint16_t keys, std::uint64_t frame_count,
                         bool recording, bool playing) {
  if (fb == nullptr || fb_w <= 0 || fb_h <= 0) return;

  struct ButtonBox {
    char label;
    std::uint16_t mask;
  };
  static const ButtonBox kButtons[] = {
    {'^', aw::kKeyUp}, {'v', aw::kKeyDown}, {'<', aw::kKeyLeft}, {'>', aw::kKeyRight},
    {'A', aw::kKeyA}, {'B', aw::kKeyB}, {'L', aw::kKeyL}, {'R', aw::kKeyR},
    {'S', aw::kKeyStart}, {'s', aw::kKeySelect},
  };
  constexpr int kBoxCount = 10;

  // Layout from the right edge: [REC/PLAY marker] [frame counter] [boxes]
  const int box_w = 11;
  const int boxes_w = kBoxCount * box_w;
  const std::string counter = "F" + std::to_string(frame_count);
  const int counter_w = static_cast<int>(counter.size()) * 6 + 4;
  const std::string marker = recording ? "REC" : (playing ? "PLAY" : "");
  const int marker_w = marker.empty() ? 0 : static_cast<int>(marker.size()) * 6 + 8;

  const int strip_w = marker_w + counter_w + boxes_w + 6;
  const int strip_h = 13;
  const int strip_x = fb_w - strip_w - 3;
  const int strip_y = 2;
  if (strip_x < 0) return;  // Degenerate sizes: draw nothing.

  draw_rect(fb, fb_w, fb_h, strip_x, strip_y, strip_w, strip_h, 0xDD0B121Cu);
  draw_rect(fb, fb_w, fb_h, strip_x, strip_y, strip_w, 1, 0xFF4A90E2u);
  draw_rect(fb, fb_w, fb_h, strip_x, strip_y + strip_h - 1, strip_w, 1, 0xFF4A90E2u);

  int x = strip_x + 3;
  if (!marker.empty()) {
    draw_text(fb, fb_w, fb_h, x, strip_y + 3, marker,
              recording ? 0xFFFF5555u : 0xFF50E3C2u);
    x += marker_w;
  }
  draw_text(fb, fb_w, fb_h, x, strip_y + 3, counter, 0xFFA0AAB8u);
  x += counter_w;

  for (int i = 0; i < kBoxCount; ++i) {
    const bool held = (keys & kButtons[i].mask) != 0;
    const int bx = x + i * box_w;
    draw_rect(fb, fb_w, fb_h, bx, strip_y + 2, box_w - 1, 9,
              held ? 0xFF1F6FEBu : 0xFF222A35u);
    char label[2] = {kButtons[i].label, '\0'};
    draw_text(fb, fb_w, fb_h, bx + 3, strip_y + 3, label,
              held ? 0xFFFFFFFFu : 0xFF707A86u);
  }
}

}  // namespace aw
