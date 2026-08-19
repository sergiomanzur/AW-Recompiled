#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/memory.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

void round_trips_iwram_word() {
  aw::Memory memory;

  aw::write32(memory, 0x03007BFC, 0x080000E4);

  require_equal(aw::read32(memory, 0x03007BFC), 0x080000E4u, "iwram word");
}

void accesses_vram_pram_oam_sram() {
  aw::Memory memory;

  aw::write32(memory, 0x05000000, 0x12345678);
  require_equal(aw::read32(memory, 0x05000000), 0x12345678u, "pram word");

  aw::write16(memory, 0x06001000, 0xABCD);
  require_equal(aw::read16(memory, 0x06001000), std::uint16_t{0xABCD}, "vram halfword");

  aw::write16(memory, 0x07000004, 0x55AA);
  require_equal(aw::read16(memory, 0x07000004), std::uint16_t{0x55AA}, "oam halfword");

  aw::write8(memory, 0x0E000010, 0x42);
  require_equal(aw::read8(memory, 0x0E000010), std::uint8_t{0x42}, "sram byte");
}

void round_trips_io_halfword() {
  aw::Memory memory;

  aw::write16(memory, 0x04000200, 0x0040);

  require_equal(aw::read16(memory, 0x04000200), std::uint16_t{0x0040}, "io halfword");
}

void round_trips_iwram_byte() {
  aw::Memory memory;

  aw::write8(memory, 0x03006561, 0x10);

  require_equal(aw::read8(memory, 0x03006561), std::uint8_t{0x10}, "iwram byte");
}

void round_trips_io_word() {
  aw::Memory memory;

  aw::write32(memory, 0x040000D4, 0x03007BEC);

  require_equal(aw::read32(memory, 0x040000D4), 0x03007BECu, "io word");
}

void keyinput_defaults_to_unpressed() {
  aw::Memory memory;

  require_equal(aw::read16(memory, 0x04000130), std::uint16_t{0x03FF}, "default keyinput");
}

}  // namespace

int main() {
  try {
    round_trips_iwram_word();
    accesses_vram_pram_oam_sram();
    round_trips_io_halfword();
    round_trips_iwram_byte();
    round_trips_io_word();
    keyinput_defaults_to_unpressed();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }

  return 0;
}
