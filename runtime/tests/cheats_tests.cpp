#include "aw/cheats.hpp"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

void require(bool condition, const std::string& label) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label.c_str());
  }
}

void test_parse() {
  aw::CheatCode code;
  require(aw::CheatEngine::parse_code("50345636:1:7", code), "decimal parses");
  require(code.address == 50345636 && code.width == 1 && code.value == 7, "decimal values");

  require(aw::CheatEngine::parse_code("0x030036A4:2:0x1234", code), "hex parses");
  require(code.address == 0x030036A4 && code.width == 2 && code.value == 0x1234, "hex values");

  require(aw::CheatEngine::parse_code("1024:4:4000000000", code), "wide value parses");
  require(code.width == 4 && code.value == 4000000000u, "wide value kept");

  require(!aw::CheatEngine::parse_code("nope", code), "garbage rejected");
  require(!aw::CheatEngine::parse_code("1:2", code), "missing value rejected");
  require(!aw::CheatEngine::parse_code("1:3:4", code), "bad width rejected");
  require(!aw::CheatEngine::parse_code("1:2:zzz", code), "bad value rejected");
}

}  // namespace

int main() {
  test_parse();
  if (failures == 0) {
    std::printf("cheats_tests: all tests passed\n");
    return 0;
  }
  std::fprintf(stderr, "cheats_tests: %d failure(s)\n", failures);
  return 1;
}
