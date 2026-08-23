#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/hardware.hpp"
#include "aw/probe/oam_tracker.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

struct OamBuffer {
  std::array<std::uint8_t, 1024> bytes{};

  OamBuffer() {
    // Park every entry off-screen by default.
    for (std::size_t i = 0; i < aw::kOamEntryCount; ++i) {
      place(i, 0, 200, 0, 0);
    }
  }

  void place(std::size_t index, int x, int y, int tile, int palette) {
    const std::uint16_t attr0 = static_cast<std::uint16_t>(y & 0xFF);
    const std::uint16_t attr1 = static_cast<std::uint16_t>(x & 0x1FF);
    const std::uint16_t attr2 =
        static_cast<std::uint16_t>((tile & 0x3FF) | ((palette & 0xF) << 12));
    const std::size_t base = index * 8;
    bytes[base + 0] = static_cast<std::uint8_t>(attr0 & 0xFF);
    bytes[base + 1] = static_cast<std::uint8_t>(attr0 >> 8);
    bytes[base + 2] = static_cast<std::uint8_t>(attr1 & 0xFF);
    bytes[base + 3] = static_cast<std::uint8_t>(attr1 >> 8);
    bytes[base + 4] = static_cast<std::uint8_t>(attr2 & 0xFF);
    bytes[base + 5] = static_cast<std::uint8_t>(attr2 >> 8);
  }
};

void tests_signature_lock_reports_position_immediately() {
  aw::OamTracker tracker;
  tracker.set_signature({/*tile=*/0x040, /*palette=*/3});

  OamBuffer oam;
  oam.place(7, /*x=*/96, /*y=*/64, /*tile=*/0x040, /*palette=*/3);

  const aw::Indicator ind = tracker.update(oam.bytes.data(), 0);
  require_equal(ind.found, true, "found by signature");
  require_equal(ind.screen_x, 96, "signature x");
  require_equal(ind.screen_y, 64, "signature y");
  require_equal(ind.oam_index, std::size_t{7}, "signature index");
}

void tests_signature_wildcards_match_any_palette() {
  aw::OamTracker tracker;
  tracker.set_signature({/*tile=*/0x040, /*palette=*/-1});

  OamBuffer oam;
  oam.place(2, 48, 32, 0x040, /*palette=*/9);

  const aw::Indicator ind = tracker.update(oam.bytes.data(), 0);
  require_equal(ind.found, true, "wildcard palette matches");
  require_equal(ind.oam_index, std::size_t{2}, "wildcard index");
}

void tests_correlation_locks_on_after_consistent_agreement() {
  aw::OamTracker tracker;  // No signature set.

  OamBuffer oam;
  // Entry 5 is the indicator; entry 6 is a distractor that never moves.
  int x = 32;
  oam.place(5, x, 64, /*tile=*/0x111, /*palette=*/1);
  oam.place(6, 200, 100, /*tile=*/0x222, /*palette=*/2);

  // Prime the tracker with the first frame (no previous frame to compare).
  tracker.update(oam.bytes.data(), 0);
  require_equal(tracker.locked(), false, "not locked before any motion");

  // Three frames of Right, with entry 5 moving right by 16 each time.
  for (int i = 0; i < 3; ++i) {
    x += 16;
    oam.place(5, x, 64, 0x111, 1);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }

  require_equal(tracker.locked(), true, "locked after consistent agreement");
  require_equal(tracker.signature().tile, 0x111, "locked onto the mover");

  const aw::Indicator ind = tracker.update(oam.bytes.data(), 0);
  require_equal(ind.found, true, "reports position once locked");
  require_equal(ind.screen_x, x, "locked x");
}

void tests_correlation_ignores_sprites_moving_the_wrong_way() {
  aw::OamTracker tracker;

  OamBuffer oam;
  int good = 32;
  int bad = 180;
  oam.place(1, good, 64, /*tile=*/0x300, /*palette=*/1);
  oam.place(2, bad, 90, /*tile=*/0x400, /*palette=*/2);
  tracker.update(oam.bytes.data(), 0);

  // Press Right repeatedly. Entry 1 moves right; entry 2 moves *left*.
  for (int i = 0; i < 3; ++i) {
    good += 16;
    bad -= 16;
    oam.place(1, good, 64, 0x300, 1);
    oam.place(2, bad, 90, 0x400, 2);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }

  require_equal(tracker.locked(), true, "locked");
  require_equal(tracker.signature().tile, 0x300, "locked onto the agreeing sprite");
}

void tests_indicator_absent_is_reported_not_crashed() {
  aw::OamTracker tracker;
  tracker.set_signature({/*tile=*/0x555, /*palette=*/1});

  OamBuffer oam;  // Nothing on screen.
  const aw::Indicator ind = tracker.update(oam.bytes.data(), 0);
  require_equal(ind.found, false, "absent indicator reported as not found");
}

void tests_null_oam_is_safe() {
  aw::OamTracker tracker;
  const aw::Indicator ind = tracker.update(nullptr, aw::kKeyRight);
  require_equal(ind.found, false, "null oam is not found");
}

void tests_reset_clears_the_lock() {
  aw::OamTracker tracker;
  tracker.set_signature({0x040, 3});
  OamBuffer oam;
  oam.place(0, 10, 10, 0x040, 3);
  tracker.update(oam.bytes.data(), 0);

  tracker.reset();
  require_equal(tracker.locked(), false, "reset unlocks");
}

}  // namespace

int main() {
  try {
    tests_signature_lock_reports_position_immediately();
    tests_signature_wildcards_match_any_palette();
    tests_correlation_locks_on_after_consistent_agreement();
    tests_correlation_ignores_sprites_moving_the_wrong_way();
    tests_indicator_absent_is_reported_not_crashed();
    tests_null_oam_is_safe();
    tests_reset_clears_the_lock();
    std::cout << "oam_tracker_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
