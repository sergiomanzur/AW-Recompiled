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

void tests_lock_anchors_to_the_earning_sprite_not_a_decoy_with_the_same_signature() {
  aw::OamTracker tracker;  // No signature set; correlation only.

  OamBuffer oam;
  // The decoy sits at a LOWER OAM index than the mover and shares the exact
  // same tile+palette, but never moves. Before anchoring to an OAM index,
  // find_by_signature() reported "lowest index with a matching signature" --
  // this decoy -- forever, even though it never earned the lock.
  oam.place(1, /*x=*/150, /*y=*/40, /*tile=*/0x2A0, /*palette=*/5);  // decoy
  int x = 20;
  oam.place(6, x, 100, /*tile=*/0x2A0, /*palette=*/5);  // mover, same signature

  tracker.update(oam.bytes.data(), 0);

  for (int i = 0; i < 3; ++i) {
    x += 16;
    oam.place(6, x, 100, 0x2A0, 5);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }
  require_equal(tracker.locked(), true, "locked onto the shared signature");

  const aw::Indicator ind = tracker.update(oam.bytes.data(), 0);
  require_equal(ind.found, true, "reports a position");
  require_equal(ind.oam_index, std::size_t{6},
                "reports the sprite that earned the lock, not the lower-index decoy");
  require_equal(ind.screen_x, x, "reports the mover's position, not the decoy's");
}

void tests_lock_breaks_when_the_locked_sprite_reverses() {
  aw::OamTracker tracker;

  OamBuffer oam;
  int x = 32;
  oam.place(5, x, 64, /*tile=*/0x111, /*palette=*/1);
  tracker.update(oam.bytes.data(), 0);

  for (int i = 0; i < 3; ++i) {
    x += 16;
    oam.place(5, x, 64, 0x111, 1);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }
  require_equal(tracker.locked(), true, "locked after consistent agreement");

  // Still holding Right, but the sprite now reverses every frame. A lock
  // that never re-evaluates would keep reporting this sprite forever; the
  // score must fall and the lock must break instead.
  for (int i = 0; i < 3; ++i) {
    x -= 16;
    oam.place(5, x, 64, 0x111, 1);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }

  require_equal(tracker.locked(), false, "lock breaks once the sprite stops agreeing");
}

void tests_correlation_ignores_sprites_moving_the_wrong_way() {
  aw::OamTracker tracker;

  OamBuffer oam;
  // The wrong-direction sprite sits at a LOWER OAM index than the correct
  // one. correlate() scans indices in ascending order and locks onto the
  // first candidate to cross the score threshold within a frame, so if the
  // sign check were ever broken (both sprites credited as "agreeing"), the
  // lower index would win the tie and this test would catch the wrong lock.
  // With a correct sign check the wrong-direction sprite's score only ever
  // goes negative, so it can never win regardless of index order.
  int bad = 180;
  int good = 32;
  oam.place(1, bad, 90, /*tile=*/0x400, /*palette=*/2);
  oam.place(2, good, 64, /*tile=*/0x300, /*palette=*/1);
  tracker.update(oam.bytes.data(), 0);

  // Press Right repeatedly. Entry 1 (lower index) moves *left*; entry 2
  // (higher index) moves right.
  for (int i = 0; i < 3; ++i) {
    bad -= 16;
    good += 16;
    oam.place(1, bad, 90, 0x400, 2);
    oam.place(2, good, 64, 0x300, 1);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }

  require_equal(tracker.locked(), true, "locked");
  require_equal(tracker.signature().tile, 0x300, "locked onto the agreeing sprite");
}

void tests_correlation_ignores_ambiguous_diagonal_chords() {
  aw::OamTracker tracker;

  OamBuffer oam;
  int x = 32;
  int y = 96;
  oam.place(3, x, y, /*tile=*/0x123, /*palette=*/4);
  tracker.update(oam.bytes.data(), 0);

  const std::uint16_t chord = aw::kKeyRight | aw::kKeyDown;
  // Every frame the sprite moves right (agrees with Right) and up (opposes
  // Down): a diagonal half-agreement that must not be scored either way, or
  // it would eventually lock onto a sprite that only ever half-obeyed.
  for (int i = 0; i < 6; ++i) {
    x += 16;
    y -= 16;
    oam.place(3, x, y, 0x123, 4);
    tracker.update(oam.bytes.data(), chord);
  }

  require_equal(tracker.locked(), false, "ambiguous diagonal agreement never locks");
}

void tests_max_step_pixels_boundary() {
  {
    aw::OamTracker tracker;
    OamBuffer oam;
    int x = 0;
    oam.place(4, x, 50, /*tile=*/0x0A0, /*palette=*/2);
    tracker.update(oam.bytes.data(), 0);

    // A step of exactly kMaxStepPixels (32) is still a cursor move.
    for (int i = 0; i < 3; ++i) {
      x += 32;
      oam.place(4, x, 50, 0x0A0, 2);
      tracker.update(oam.bytes.data(), aw::kKeyRight);
    }
    require_equal(tracker.locked(), true, "a 32px step is accepted");
    require_equal(tracker.signature().tile, 0x0A0, "locked onto the 32px stepper");
  }
  {
    aw::OamTracker tracker;
    OamBuffer oam;
    int x = 0;
    oam.place(4, x, 50, /*tile=*/0x0B0, /*palette=*/2);
    tracker.update(oam.bytes.data(), 0);

    // A step of 33px (just past kMaxStepPixels) is a scene jump, not a
    // cursor move, and must never contribute to the score no matter how
    // many frames it repeats.
    for (int i = 0; i < 6; ++i) {
      x += 33;
      oam.place(4, x, 50, 0x0B0, 2);
      tracker.update(oam.bytes.data(), aw::kKeyRight);
    }
    require_equal(tracker.locked(), false, "a 33px step never locks");
  }
}

void tests_animation_creep_never_locks_even_over_many_frames() {
  aw::OamTracker tracker;  // No signature set; correlation only.

  OamBuffer oam;
  int x = 32;
  oam.place(5, x, 64, /*tile=*/0x111, /*palette=*/1);
  tracker.update(oam.bytes.data(), 0);

  // 2 px/frame in the commanded direction is animation creep (scrolling
  // text, a unit's idle animation), not a cursor step. Even many frames of
  // consistent "agreement" at this magnitude must never accumulate enough
  // score to lock -- unlike a stuck-then-recovering axis, this is not a
  // matter of waiting longer.
  for (int i = 0; i < 100; ++i) {
    x += 2;
    oam.place(5, x, 64, 0x111, 1);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }

  require_equal(tracker.locked(), false, "sub-tile creep never locks, however long it persists");
}

void tests_tile_sized_step_still_locks() {
  // Regression guard: the existing tile-sized-step lock path must be
  // unaffected by the new lower bound.
  aw::OamTracker tracker;
  OamBuffer oam;
  int x = 32;
  oam.place(5, x, 64, /*tile=*/0x0E0, /*palette=*/1);
  tracker.update(oam.bytes.data(), 0);

  for (int i = 0; i < 3; ++i) {
    x += 16;
    oam.place(5, x, 64, 0x0E0, 1);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }

  require_equal(tracker.locked(), true, "a 16px tile step still locks");
  require_equal(tracker.signature().tile, 0x0E0, "locked onto the 16px stepper");
}

void tests_min_step_pixels_boundary() {
  {
    aw::OamTracker tracker;
    OamBuffer oam;
    int x = 0;
    oam.place(4, x, 50, /*tile=*/0x0C0, /*palette=*/2);
    tracker.update(oam.bytes.data(), 0);

    // A step of exactly kMinStepPixels (4) still counts as evidence.
    for (int i = 0; i < 3; ++i) {
      x += 4;
      oam.place(4, x, 50, 0x0C0, 2);
      tracker.update(oam.bytes.data(), aw::kKeyRight);
    }
    require_equal(tracker.locked(), true, "a 4px step is accepted");
    require_equal(tracker.signature().tile, 0x0C0, "locked onto the 4px stepper");
  }
  {
    aw::OamTracker tracker;
    OamBuffer oam;
    int x = 0;
    oam.place(4, x, 50, /*tile=*/0x0D0, /*palette=*/2);
    tracker.update(oam.bytes.data(), 0);

    // A step of 3px (just under kMinStepPixels) must never contribute to
    // the score, no matter how many frames it repeats.
    for (int i = 0; i < 6; ++i) {
      x += 3;
      oam.place(4, x, 50, 0x0D0, 2);
      tracker.update(oam.bytes.data(), aw::kKeyRight);
    }
    require_equal(tracker.locked(), false, "a 3px step never locks");
  }
}

void tests_stale_signature_drops_then_recorrelates() {
  aw::OamTracker tracker;
  tracker.set_signature({/*tile=*/0x555, /*palette=*/1});

  OamBuffer oam;  // The signature never appears anywhere on screen.
  int x = 40;
  // Tile IDs are a 10-bit OAM field (max 0x3FF); stay under that so the
  // value read back after the place()/decode_oam_entry() round trip is
  // exactly the constant used here.
  oam.place(9, x, 70, /*tile=*/0x1D0, /*palette=*/6);

  const aw::Indicator primed = tracker.update(oam.bytes.data(), 0);
  require_equal(primed.found, false, "stale signature never matches");
  require_equal(tracker.locked(), true, "starts locked from the explicit signature");

  // The signature never matches anything on screen; after kUnlockFrames (60)
  // frames of absence the tracker must give up on it rather than reporting
  // it, or nothing, forever.
  for (int i = 0; i < 60; ++i) {
    const aw::Indicator ind = tracker.update(oam.bytes.data(), 0);
    require_equal(ind.found, false, "stale signature never matches");
  }
  require_equal(tracker.locked(), false, "gives up on a stale signature");

  // Drive a different sprite with the D-pad; correlation should pick it up
  // exactly as it would if no signature had ever been set.
  for (int i = 0; i < 3; ++i) {
    x += 16;
    oam.place(9, x, 70, 0x1D0, 6);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }

  require_equal(tracker.locked(), true, "re-locks by correlation after the drop");
  require_equal(tracker.signature().tile, 0x1D0, "locked onto the new sprite");
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

void tests_verified_lock_stays_locked_while_moving_as_commanded() {
  aw::OamTracker tracker;
  OamBuffer oam;
  int x = 0;
  oam.place(5, x, 64, /*tile=*/0x151, /*palette=*/2);
  tracker.update(oam.bytes.data(), 0);

  // Earn the lock exactly as tests_correlation_locks_on_after_consistent_agreement.
  for (int i = 0; i < 3; ++i) {
    x += 16;
    oam.place(5, x, 64, 0x151, 2);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }
  require_equal(tracker.locked(), true, "locked after the initial agreement");

  // Keep obeying every single press, for well more than the 12-point
  // verification budget could tolerate if it were (wrongly) still
  // accumulating instead of resetting on every agreeing frame: a sprite that
  // genuinely is the cursor must never lose its lock. (An 8 px step keeps
  // this comfortably on the 240 px-wide screen for 20 frames; on_screen()
  // requires x < 240.)
  for (int i = 0; i < 20; ++i) {
    x += 8;
    oam.place(5, x, 64, 0x151, 2);
    const aw::Indicator ind = tracker.update(oam.bytes.data(), aw::kKeyRight);
    require_equal(tracker.locked(), true, "stays locked while genuinely obeying every press");
    require_equal(ind.found, true, "still reporting a position");
  }
}

void tests_verified_lock_drops_on_consistent_wrong_direction_within_budget() {
  aw::OamTracker tracker;
  OamBuffer oam;

  // Entry 5 earns the lock normally.
  int x = 32;
  oam.place(5, x, 64, /*tile=*/0x153, /*palette=*/2);
  tracker.update(oam.bytes.data(), 0);
  for (int i = 0; i < 3; ++i) {
    x += 16;
    oam.place(5, x, 64, 0x153, 2);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }
  require_equal(tracker.locked(), true, "locked after the initial agreement");
  require_equal(tracker.signature().tile, 0x153, "locked onto entry 5");

  // From here entry 5 (the anchor) moves the WRONG way on every commanded
  // frame. Entry 6 is a decoy sharing the exact same tile+palette that moves
  // CORRECTLY every frame at the same time. correlate() scores candidates by
  // signature, not by OAM index, so entry 6's +1 cancels entry 5's -1 in the
  // very same shared candidate every frame; the net score never reaches
  // kUnlockScore. Any drop that happens below can only be verify_lock()'s
  // own budget at work, not the pre-existing score-decay unlock path -- this
  // isolates the new mechanism from the old one.
  int decoy_x = 150;
  for (int i = 0; i < 5; ++i) {
    x -= 16;        // entry 5 (the anchor): wrong way -- Right commanded, moves left
    decoy_x += 16;   // entry 6 (the decoy): agrees
    oam.place(5, x, 64, 0x153, 2);
    oam.place(6, decoy_x, 100, 0x153, 2);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
    require_equal(tracker.locked(), true, "under the wrong-direction budget (5 * weight 2 = 10 < 12)");
  }

  // The 6th wrong-direction frame brings the weighted total to 5*2 + 2 = 12,
  // exhausting the budget, even though the shared candidate's score-based
  // path (still healthy, thanks to the decoy) never would have unlocked it.
  x -= 16;
  decoy_x += 16;
  oam.place(5, x, 64, 0x153, 2);
  oam.place(6, decoy_x, 100, 0x153, 2);
  tracker.update(oam.bytes.data(), aw::kKeyRight);
  require_equal(tracker.locked(), false, "wrong-direction budget exhausted; lock dropped");
}

void tests_verified_lock_is_more_lenient_about_a_still_sprite_than_a_wrong_way_one() {
  aw::OamTracker tracker;
  OamBuffer oam;
  int x = 32;
  oam.place(5, x, 64, /*tile=*/0x152, /*palette=*/2);
  tracker.update(oam.bytes.data(), 0);
  for (int i = 0; i < 3; ++i) {
    x += 16;
    oam.place(5, x, 64, 0x152, 2);
    tracker.update(oam.bytes.data(), aw::kKeyRight);
  }
  require_equal(tracker.locked(), true, "locked after the initial agreement");

  // The game is busy (e.g. an animation) and legitimately ignores input: the
  // locked sprite sits perfectly still even though Right keeps being
  // commanded. dx == dy == 0 makes correlate() skip this candidate entirely
  // (its own early `continue`), so only verify_lock()'s own budget is at
  // work here -- and stillness must be forgiven for far longer than an
  // outright wrong turn (which drops the lock within 6 frames, see the
  // wrong-direction test above). At weight 1 per frame, 11 still frames
  // (verify_fail_ = 11) must NOT be enough.
  for (int i = 0; i < 11; ++i) {
    oam.place(5, x, 64, 0x152, 2);  // unchanged position
    tracker.update(oam.bytes.data(), aw::kKeyRight);
    require_equal(tracker.locked(), true, "stillness is forgiven far longer than a wrong turn");
  }

  // The 12th still-but-commanded frame reaches the budget (12) and the lock
  // finally drops -- it must not be forgiven forever, either.
  oam.place(5, x, 64, 0x152, 2);
  tracker.update(oam.bytes.data(), aw::kKeyRight);
  require_equal(tracker.locked(), false, "eventually drops if it never resumes moving");
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
    tests_lock_anchors_to_the_earning_sprite_not_a_decoy_with_the_same_signature();
    tests_lock_breaks_when_the_locked_sprite_reverses();
    tests_correlation_ignores_sprites_moving_the_wrong_way();
    tests_correlation_ignores_ambiguous_diagonal_chords();
    tests_max_step_pixels_boundary();
    tests_animation_creep_never_locks_even_over_many_frames();
    tests_tile_sized_step_still_locks();
    tests_min_step_pixels_boundary();
    tests_stale_signature_drops_then_recorrelates();
    tests_indicator_absent_is_reported_not_crashed();
    tests_null_oam_is_safe();
    tests_verified_lock_stays_locked_while_moving_as_commanded();
    tests_verified_lock_drops_on_consistent_wrong_direction_within_budget();
    tests_verified_lock_is_more_lenient_about_a_still_sprite_than_a_wrong_way_one();
    tests_reset_clears_the_lock();
    std::cout << "oam_tracker_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
