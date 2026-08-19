#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/cpu_state.hpp"
#include "aw/hardware.hpp"
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

void tests_keyinput_mapping() {
  aw::Memory memory;
  aw::Hardware hw;

  hw.keys_pressed = aw::kKeyA | aw::kKeyStart;
  hw.update_keys(memory);

  // Default is 0x03FF. Key A (bit 0) and Start (bit 3) pressed -> 0x03FF & ~(1|8) = 0x03F6
  require_equal(aw::read16(memory, 0x04000130), std::uint16_t{0x03F6}, "keyinput active-low");
}

void tests_dma_immediate_transfer() {
  aw::Memory memory;
  aw::Hardware hw;

  // Source IWRAM 0x03000000 -> Dest EWRAM 0x02000000
  aw::write32(memory, 0x03000000, 0x11223344);
  aw::write32(memory, 0x03000004, 0x55667788);

  aw::write32(memory, 0x040000D4, 0x03000000); // DMA3 SAD
  aw::write32(memory, 0x040000D8, 0x02000000); // DMA3 DAD
  // Control: Count 2, 32-bit (bit 26), Start Immediate (bits 28-29=0), Enable (bit 31) -> 0x84000002
  aw::write32(memory, 0x040000DC, 0x84000002);

  hw.process_dma(memory, 0);

  require_equal(aw::read32(memory, 0x02000000), 0x11223344u, "dma3 word 0");
  require_equal(aw::read32(memory, 0x02000004), 0x55667788u, "dma3 word 1");
}

}  // namespace

int main() {
  try {
    tests_keyinput_mapping();
    tests_dma_immediate_transfer();
    std::cout << "hardware_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
