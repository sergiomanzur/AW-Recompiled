#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

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

void parses_advance_wars_rev1_header() {
  const auto rom = aw::load_rom_file(kRomPath);
  const auto header = aw::parse_header(rom.bytes);

  require_equal(header.title, std::string("ADVANCEWARS"), "title");
  require_equal(header.game_code, std::string("AWRE"), "game code");
  require_equal(header.maker_code, std::string("01"), "maker code");
  require_equal(header.version, std::uint8_t{1}, "version");
  require_equal(header.fixed_value, std::uint8_t{0x96}, "fixed value");
}

void computes_expected_advance_wars_rev1_sha1() {
  const auto rom = aw::load_rom_file(kRomPath);

  require_equal(aw::sha1_hex(rom.bytes),
                std::string("15053499D5B3F49128A941D7F2D84876F5424D0C"),
                "sha1");
  require_equal(aw::is_expected_advance_wars_rev1(rom), true, "expected ROM");
}

}  // namespace

int main() {
  try {
    parses_advance_wars_rev1_header();
    computes_expected_advance_wars_rev1_sha1();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }

  return 0;
}
