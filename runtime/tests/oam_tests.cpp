#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/probe/oam.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

// Builds a 1 KB OAM buffer and writes one entry's four attribute halfwords.
struct OamBuffer {
  std::array<std::uint8_t, 1024> bytes{};

  void set(std::size_t index, std::uint16_t attr0, std::uint16_t attr1, std::uint16_t attr2) {
    const std::size_t base = index * 8;
    bytes[base + 0] = static_cast<std::uint8_t>(attr0 & 0xFF);
    bytes[base + 1] = static_cast<std::uint8_t>(attr0 >> 8);
    bytes[base + 2] = static_cast<std::uint8_t>(attr1 & 0xFF);
    bytes[base + 3] = static_cast<std::uint8_t>(attr1 >> 8);
    bytes[base + 4] = static_cast<std::uint8_t>(attr2 & 0xFF);
    bytes[base + 5] = static_cast<std::uint8_t>(attr2 >> 8);
  }
};

void tests_decodes_position_tile_and_palette() {
  OamBuffer oam;
  // y = 80, x = 120, tile = 0x123, palette bank 5, priority 2.
  oam.set(3, /*attr0=*/80, /*attr1=*/120,
          /*attr2=*/static_cast<std::uint16_t>(0x123 | (2 << 10) | (5 << 12)));

  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 3);
  require_equal(e.y, 80, "y");
  require_equal(e.x, 120, "x");
  require_equal(e.tile, 0x123, "tile");
  require_equal(e.priority, 2, "priority");
  require_equal(e.palette, 5, "palette");
  require_equal(e.visible, true, "visible");
  require_equal(e.on_screen(), true, "on screen");
}

void tests_x_is_nine_bit_signed() {
  OamBuffer oam;
  // attr1 X field is 9 bits; 0x1F0 (496) means -16.
  oam.set(0, /*attr0=*/40, /*attr1=*/0x1F0, /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 0);
  require_equal(e.x, -16, "negative x");
}

void tests_x_nine_bit_boundaries() {
  // attr1's X field is 9 bits of two's complement: raw 0..255 stay
  // positive, raw 256..511 fold back to -256..-1. Pin the exact fold
  // boundary and both extremes so an off-by-one threshold (e.g. a fold
  // starting at raw >= 257 instead of >= 256) cannot survive.
  OamBuffer oam;
  oam.set(0, /*attr0=*/40, /*attr1=*/255, /*attr2=*/0);
  oam.set(1, /*attr0=*/40, /*attr1=*/256, /*attr2=*/0);
  oam.set(2, /*attr0=*/40, /*attr1=*/511, /*attr2=*/0);

  require_equal(aw::decode_oam_entry(oam.bytes.data(), 0).x, 255, "raw 255 stays positive");
  require_equal(aw::decode_oam_entry(oam.bytes.data(), 1).x, -256, "raw 256 folds to -256");
  require_equal(aw::decode_oam_entry(oam.bytes.data(), 2).x, -1, "raw 511 folds to -1");
}

void tests_disabled_objects_are_not_visible() {
  OamBuffer oam;
  // attr0 bits 8-9 == 0b10 is the "disabled" object mode.
  oam.set(1, /*attr0=*/static_cast<std::uint16_t>(60 | (2 << 8)), /*attr1=*/50, /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 1);
  require_equal(e.visible, false, "disabled is invisible");
  require_equal(e.on_screen(), false, "disabled is off screen");
}

void tests_object_mode_bit_combinations() {
  // attr0 bits 8-9 encode the OBJ mode: bit8 is "Transformed"
  // (rotation/scaling), bit9 is "Disable" -- but bit9 only means disable
  // when bit8 is 0. When both bits are set (0b11) the object is an
  // affine "double size" sprite, not a disabled one, and it still
  // renders. Checking a single bit instead of the full two-bit field
  // would wrongly hide those double-size affine sprites, so pin all four
  // combinations explicitly.
  OamBuffer oam;
  oam.set(0, /*attr0=*/static_cast<std::uint16_t>(60 | (0 << 8)), /*attr1=*/50, /*attr2=*/0);
  oam.set(1, /*attr0=*/static_cast<std::uint16_t>(60 | (1 << 8)), /*attr1=*/50, /*attr2=*/0);
  oam.set(2, /*attr0=*/static_cast<std::uint16_t>(60 | (2 << 8)), /*attr1=*/50, /*attr2=*/0);
  oam.set(3, /*attr0=*/static_cast<std::uint16_t>(60 | (3 << 8)), /*attr1=*/50, /*attr2=*/0);

  require_equal(aw::decode_oam_entry(oam.bytes.data(), 0).visible, true, "mode 0b00 (normal) visible");
  require_equal(aw::decode_oam_entry(oam.bytes.data(), 1).visible, true, "mode 0b01 (affine) visible");
  require_equal(aw::decode_oam_entry(oam.bytes.data(), 2).visible, false, "mode 0b10 (disabled) invisible");
  require_equal(aw::decode_oam_entry(oam.bytes.data(), 3).visible, true, "mode 0b11 (affine double-size) visible");
}

void tests_offscreen_y_is_not_on_screen() {
  OamBuffer oam;
  // Y = 200 is the usual "parked below the screen" idiom.
  oam.set(2, /*attr0=*/200, /*attr1=*/50, /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 2);
  require_equal(e.visible, true, "not disabled");
  require_equal(e.on_screen(), false, "parked below screen");
}

void tests_on_screen_horizontal_bounds() {
  // on_screen() requires x > -64 and x < 240. A 64-wide sprite parked
  // exactly at x = -64 has zero visible columns, so -64 itself must read
  // as off screen; -63 is the first on-screen column. Mirror the check
  // at the right edge: 239 on screen, 240 off screen.
  OamBuffer oam;
  oam.set(0, /*attr0=*/50, /*attr1=*/448 /* raw two's-complement for x = -64 */, /*attr2=*/0);
  oam.set(1, /*attr0=*/50, /*attr1=*/449 /* raw two's-complement for x = -63 */, /*attr2=*/0);
  oam.set(2, /*attr0=*/50, /*attr1=*/239, /*attr2=*/0);
  oam.set(3, /*attr0=*/50, /*attr1=*/240, /*attr2=*/0);

  require_equal(aw::decode_oam_entry(oam.bytes.data(), 0).on_screen(), false, "x = -64 is off screen");
  require_equal(aw::decode_oam_entry(oam.bytes.data(), 1).on_screen(), true, "x = -63 is on screen");
  require_equal(aw::decode_oam_entry(oam.bytes.data(), 2).on_screen(), true, "x = 239 is on screen");
  require_equal(aw::decode_oam_entry(oam.bytes.data(), 3).on_screen(), false, "x = 240 is off screen");
}

void tests_tile_field_uses_full_ten_bits() {
  // attr2's tile field (character name) is 10 bits (mask 0x3FF). A mask
  // of 0x1FF (9 bits) would silently drop bit 9 and still pass every
  // other test in this file, since the only tile value exercised
  // elsewhere (0x123) fits in 9 bits.
  OamBuffer oam;
  oam.set(0, /*attr0=*/40, /*attr1=*/50, /*attr2=*/0x200);
  oam.set(1, /*attr0=*/40, /*attr1=*/50, /*attr2=*/0x3FF);

  require_equal(aw::decode_oam_entry(oam.bytes.data(), 0).tile, 0x200, "tile bit 9 alone round-trips");
  require_equal(aw::decode_oam_entry(oam.bytes.data(), 1).tile, 0x3FF, "tile all ten bits round-trip");
}

void tests_object_size_square_16x16() {
  // shape=0 (square), size=1 -> 16x16. attr0 bits 14-15 hold shape, attr1
  // bits 14-15 hold size; neither overlaps the y/x/mode fields those tests
  // above already exercise.
  OamBuffer oam;
  oam.set(0, /*attr0=*/static_cast<std::uint16_t>(50 | (0u << 14)),
          /*attr1=*/static_cast<std::uint16_t>(60 | (1u << 14)), /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 0);
  require_equal(e.width, 16, "square size1 width");
  require_equal(e.height, 16, "square size1 height");
}

void tests_object_size_horizontal_16x8() {
  // shape=1 (horizontal), size=0 -> 16x8. Width and height differ, which is
  // exactly what a width/height swap in the decode table would get backwards
  // (see mutation M8).
  OamBuffer oam;
  oam.set(0, /*attr0=*/static_cast<std::uint16_t>(50 | (1u << 14)),
          /*attr1=*/static_cast<std::uint16_t>(60 | (0u << 14)), /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 0);
  require_equal(e.width, 16, "horizontal size0 width");
  require_equal(e.height, 8, "horizontal size0 height");
}

void tests_object_size_vertical_8x32() {
  // shape=2 (vertical), size=1 -> 8x32.
  OamBuffer oam;
  oam.set(0, /*attr0=*/static_cast<std::uint16_t>(50 | (2u << 14)),
          /*attr1=*/static_cast<std::uint16_t>(60 | (1u << 14)), /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 0);
  require_equal(e.width, 8, "vertical size1 width");
  require_equal(e.height, 32, "vertical size1 height");
}

void tests_object_shape_prohibited_falls_back_to_8x8() {
  // shape=3 is prohibited by GBA hardware. Pin the fallback to 8x8 even with
  // non-zero size bits, so an OAM entry that (legitimately or not) encodes
  // shape 3 never produces a bogus or out-of-table size.
  OamBuffer oam;
  oam.set(0, /*attr0=*/static_cast<std::uint16_t>(50 | (3u << 14)),
          /*attr1=*/static_cast<std::uint16_t>(60 | (2u << 14)), /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 0);
  require_equal(e.width, 8, "prohibited shape falls back to 8 width");
  require_equal(e.height, 8, "prohibited shape falls back to 8 height");
}

void tests_out_of_range_index_is_invisible() {
  OamBuffer oam;
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), aw::kOamEntryCount);
  require_equal(e.visible, false, "out of range is invisible");
}

void tests_null_buffer_is_invisible() {
  const aw::OamEntry e = aw::decode_oam_entry(nullptr, 0);
  require_equal(e.visible, false, "null buffer is invisible");
}

}  // namespace

int main() {
  try {
    tests_decodes_position_tile_and_palette();
    tests_x_is_nine_bit_signed();
    tests_x_nine_bit_boundaries();
    tests_disabled_objects_are_not_visible();
    tests_object_mode_bit_combinations();
    tests_offscreen_y_is_not_on_screen();
    tests_on_screen_horizontal_bounds();
    tests_tile_field_uses_full_ten_bits();
    tests_object_size_square_16x16();
    tests_object_size_horizontal_16x8();
    tests_object_size_vertical_8x32();
    tests_object_shape_prohibited_falls_back_to_8x8();
    tests_out_of_range_index_is_invisible();
    tests_null_buffer_is_invisible();
    std::cout << "oam_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
