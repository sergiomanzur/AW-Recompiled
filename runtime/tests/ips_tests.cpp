#include "aw/ips.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void require(bool condition, const std::string& label) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label.c_str());
  }
}

std::vector<std::uint8_t> bytes(std::initializer_list<std::uint8_t> list) {
  return std::vector<std::uint8_t>(list);
}

void test_plain_record() {
  // PATCH, record at offset 4 length 3 "XYZ", EOF.
  const auto patch = bytes({'P', 'A', 'T', 'C', 'H',
                            0, 0, 4, 0, 3, 'X', 'Y', 'Z',
                            'E', 'O', 'F'});
  std::vector<std::uint8_t> rom(16, 0xAA);
  std::string err;
  require(aw::apply_ips(rom, patch, err), "plain record applies: " + err);
  require(rom[3] == 0xAA && rom[4] == 'X' && rom[5] == 'Y' && rom[6] == 'Z' && rom[7] == 0xAA,
          "only the record range changed");
}

void test_rle_record() {
  const auto patch = bytes({'P', 'A', 'T', 'C', 'H',
                            0, 0, 2, 0, 0, 0, 4, 0x77,
                            'E', 'O', 'F'});
  std::vector<std::uint8_t> rom(8, 0);
  std::string err;
  require(aw::apply_ips(rom, patch, err), "RLE record applies: " + err);
  require(rom[0] == 0 && rom[1] == 0, "before untouched");
  require(rom[2] == 0x77 && rom[3] == 0x77 && rom[4] == 0x77 && rom[5] == 0x77,
          "four fill bytes written");
  require(rom[6] == 0, "after untouched");
}

void test_rejects_bad_input() {
  std::vector<std::uint8_t> rom(16, 0);
  std::string err;

  require(!aw::apply_ips(rom, bytes({'N', 'O', 'P', 'E'}), err) && err.find("PATCH") != std::string::npos,
          "bad magic rejected");

  // Record past the end of the ROM.
  require(!aw::apply_ips(rom, bytes({'P', 'A', 'T', 'C', 'H', 0, 1, 0, 0, 4, 1, 2, 3, 4}),
                         err) &&
              err.find("past the end") != std::string::npos,
          "out-of-range record rejected: " + err);

  // RLE past the end.
  require(!aw::apply_ips(rom, bytes({'P', 'A', 'T', 'C', 'H', 0, 0, 14, 0, 0, 0, 8, 0}),
                         err) &&
              err.find("past the end") != std::string::npos,
          "out-of-range RLE rejected");

  // Truncated payload.
  require(!aw::apply_ips(rom, bytes({'P', 'A', 'T', 'C', 'H', 0, 0, 0, 0, 8, 1, 2}), err),
          "truncated record rejected");

  // Header-only (no records).
  require(!aw::apply_ips(rom, bytes({'P', 'A', 'T', 'C', 'H', 'E', 'O', 'F'}), err),
          "empty patch rejected");
}

void test_missing_eof_tolerated() {
  // Some tools omit EOF; records are still applied.
  const auto patch = bytes({'P', 'A', 'T', 'C', 'H', 0, 0, 0, 0, 1, 0x55});
  std::vector<std::uint8_t> rom(4, 0);
  std::string err;
  require(aw::apply_ips(rom, patch, err), "missing EOF tolerated: " + err);
  require(rom[0] == 0x55, "record applied");
}

}  // namespace

int main() {
  test_plain_record();
  test_rle_record();
  test_rejects_bad_input();
  test_missing_eof_tolerated();

  if (failures == 0) {
    std::printf("ips_tests: all tests passed\n");
    return 0;
  }
  std::fprintf(stderr, "ips_tests: %d failure(s)\n", failures);
  return 1;
}
