#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "aw/probe/cursor_probe.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

// A ProbeBackend backed by plain vectors for both IWRAM and EWRAM, so
// read_cursor_tile is testable with no emulator. Either block can be made to
// report "unavailable" (null pointer) via the *_null_ flags, so the
// null-block path is exercised without needing a second backend type.
class FakeBackend final : public aw::ProbeBackend {
public:
  explicit FakeBackend(std::size_t iwram_size = 0x8000, std::size_t ewram_size = 0x40000)
      : iwram_(iwram_size, 0), ewram_(ewram_size, 0) {}

  void poke_iwram(std::uint32_t addr, std::uint8_t value) { iwram_[addr - 0x03000000] = value; }
  void poke_ewram(std::uint32_t addr, std::uint8_t value) { ewram_[addr - 0x02000000] = value; }

  void set_iwram_null(bool null) { iwram_null_ = null; }

  bool available() override { return true; }
  const std::uint8_t* oam() override { return nullptr; }

  const std::uint8_t* ewram(std::size_t& size_out) override {
    size_out = ewram_.size();
    return ewram_.data();
  }

  const std::uint8_t* iwram(std::size_t& size_out) override {
    if (iwram_null_) {
      size_out = 0;
      return nullptr;
    }
    size_out = iwram_.size();
    return iwram_.data();
  }

  std::uint16_t read_io16(std::uint32_t) override { return 0; }

private:
  std::vector<std::uint8_t> iwram_;
  std::vector<std::uint8_t> ewram_;
  bool iwram_null_ = false;
};

// Mirrors the real mined addresses (task-9f brief / data/symbols ini):
// X = 0x030036A4, Y = 0x030036A6, two bytes apart, both in IWRAM.
constexpr std::uint32_t kRealXAddr = 0x030036A4;
constexpr std::uint32_t kRealYAddr = 0x030036A6;

void tests_reads_from_iwram() {
  FakeBackend backend;
  backend.poke_iwram(kRealXAddr, 7);
  backend.poke_iwram(kRealYAddr, 0);

  aw::CursorAddresses addrs{kRealXAddr, kRealYAddr};
  const aw::CursorTile tile = aw::read_cursor_tile(backend, addrs);

  require_equal(tile.found, true, "iwram read found");
  require_equal(tile.x, 7, "iwram x");
  require_equal(tile.y, 0, "iwram y");
}

void tests_reads_from_ewram() {
  FakeBackend backend;
  const std::uint32_t x_addr = 0x02001000;
  const std::uint32_t y_addr = 0x02001002;
  backend.poke_ewram(x_addr, 3);
  backend.poke_ewram(y_addr, 5);

  aw::CursorAddresses addrs{x_addr, y_addr};
  const aw::CursorTile tile = aw::read_cursor_tile(backend, addrs);

  require_equal(tile.found, true, "ewram read found");
  require_equal(tile.x, 3, "ewram x");
  require_equal(tile.y, 5, "ewram y");
}

void tests_unset_addresses_not_found() {
  FakeBackend backend;
  backend.poke_iwram(kRealXAddr, 7);

  aw::CursorAddresses addrs;  // x_addr/y_addr default to 0
  const aw::CursorTile tile = aw::read_cursor_tile(backend, addrs);

  require_equal(tile.found, false, "unset addresses not found");
}

void tests_one_unset_address_not_found() {
  FakeBackend backend;
  backend.poke_iwram(kRealXAddr, 7);

  aw::CursorAddresses addrs{kRealXAddr, 0};  // y_addr unset
  const aw::CursorTile tile = aw::read_cursor_tile(backend, addrs);

  require_equal(tile.found, false, "one address unset -> not found");
}

void tests_address_past_block_size_not_found() {
  FakeBackend backend(0x8000, 0x40000);  // 32 KB IWRAM

  // Squarely inside the IWRAM *address range* (0x03000000-0x03FFFFFF) but
  // past the real 32 KB block the backend actually has.
  const std::uint32_t x_addr = 0x03000000 + 0x9000;
  const std::uint32_t y_addr = 0x03000000 + 0x9002;

  aw::CursorAddresses addrs{x_addr, y_addr};
  const aw::CursorTile tile = aw::read_cursor_tile(backend, addrs);

  require_equal(tile.found, false, "past block size -> not found");
}

void tests_null_block_not_found() {
  FakeBackend backend;
  backend.set_iwram_null(true);

  aw::CursorAddresses addrs{kRealXAddr, kRealYAddr};
  const aw::CursorTile tile = aw::read_cursor_tile(backend, addrs);

  require_equal(tile.found, false, "null block -> not found");
}

void tests_address_outside_any_mapped_range_not_found() {
  FakeBackend backend;
  // 0x04000000 is IO space, covered by neither ewram() nor iwram().
  aw::CursorAddresses addrs{0x04000010, 0x04000012};
  const aw::CursorTile tile = aw::read_cursor_tile(backend, addrs);

  require_equal(tile.found, false, "unmapped range -> not found");
}

}  // namespace

int main() {
  try {
    tests_reads_from_iwram();
    tests_reads_from_ewram();
    tests_unset_addresses_not_found();
    tests_one_unset_address_not_found();
    tests_address_past_block_size_not_found();
    tests_null_block_not_found();
    tests_address_outside_any_mapped_range_not_found();
    std::cout << "cursor_probe_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
