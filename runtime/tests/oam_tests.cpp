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

void tests_disabled_objects_are_not_visible() {
  OamBuffer oam;
  // attr0 bits 8-9 == 0b10 is the "disabled" object mode.
  oam.set(1, /*attr0=*/static_cast<std::uint16_t>(60 | (2 << 8)), /*attr1=*/50, /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 1);
  require_equal(e.visible, false, "disabled is invisible");
  require_equal(e.on_screen(), false, "disabled is off screen");
}

void tests_offscreen_y_is_not_on_screen() {
  OamBuffer oam;
  // Y = 200 is the usual "parked below the screen" idiom.
  oam.set(2, /*attr0=*/200, /*attr1=*/50, /*attr2=*/0);
  const aw::OamEntry e = aw::decode_oam_entry(oam.bytes.data(), 2);
  require_equal(e.visible, true, "not disabled");
  require_equal(e.on_screen(), false, "parked below screen");
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
    tests_disabled_objects_are_not_visible();
    tests_offscreen_y_is_not_on_screen();
    tests_out_of_range_index_is_invisible();
    tests_null_buffer_is_invisible();
    std::cout << "oam_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
