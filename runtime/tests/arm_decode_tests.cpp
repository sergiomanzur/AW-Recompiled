#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "aw/arm_decode.hpp"
#include "aw/rom.hpp"
#include "aw/test_config.hpp"

namespace {

constexpr const char* kRomPath = AW_TEST_ROM_PATH;

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

void decodes_reset_vector_to_rom_entry() {
  const auto rom = aw::load_rom_file(kRomPath);
  const auto instruction = aw::read_le32(rom.bytes, 0);
  const auto branch = aw::decode_arm_branch(instruction, 0x08000000);

  require_equal(branch.condition, std::uint8_t{0xE}, "condition");
  require_equal(branch.link, false, "link flag");
  require_equal(branch.target, 0x080000C0u, "branch target");
}

}  // namespace

int main() {
  try {
    decodes_reset_vector_to_rom_entry();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }

  return 0;
}
