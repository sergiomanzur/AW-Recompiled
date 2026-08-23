#pragma once

#include "aw/probe/oam.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace aw {

// Identifies the indicator sprite by its character and palette. -1 is a
// wildcard, so a context can pin the tile and let the palette vary by army.
struct IndicatorSignature {
  int tile = -1;
  int palette = -1;

  bool wildcard() const { return tile < 0 && palette < 0; }
  bool matches(const OamEntry& e) const {
    return (tile < 0 || e.tile == tile) && (palette < 0 || e.palette == palette);
  }
};

struct Indicator {
  bool found = false;
  int screen_x = 0;
  int screen_y = 0;
  std::size_t oam_index = kOamEntryCount;
};

// Tracks the game's selection indicator through OAM.
//
// Two modes. With a signature (from the symbol table) the indicator is found
// by matching tile/palette. Without one, the tracker correlates movement
// against the D-pad it was told we emitted: the sprite that keeps moving the
// way we commanded is the indicator. Correlation means steering works with no
// mined symbols at all, and it re-locks by itself if a stale signature stops
// matching.
class OamTracker {
public:
  void reset();

  // Pins the signature explicitly. Pass a wildcard signature to fall back to
  // correlation.
  void set_signature(const IndicatorSignature& sig);

  // Call once per frame *after* the emulator has run. `emitted_dpad` is the
  // D-pad mask sent to the core for the frame that just executed.
  Indicator update(const std::uint8_t* oam, std::uint16_t emitted_dpad);

  bool locked() const { return locked_; }
  IndicatorSignature signature() const { return signature_; }

private:
  Indicator find_by_signature(const std::uint8_t* oam) const;
  void correlate(const std::uint8_t* oam, std::uint16_t emitted_dpad);

  static constexpr int kLockScore = 3;      // Net agreements needed to lock
  static constexpr int kMaxStepPixels = 32; // Larger jumps are not cursor steps
  static constexpr int kUnlockFrames = 60;  // Frames absent before re-locking
  static constexpr std::size_t kMaxCandidates = 32;

  struct Candidate {
    IndicatorSignature sig;
    int score = 0;
    bool used = false;
  };

  Candidate* candidate_for(const IndicatorSignature& sig);

  IndicatorSignature signature_{};
  bool locked_ = false;
  bool has_prev_ = false;
  int missing_frames_ = 0;
  std::array<OamEntry, kOamEntryCount> prev_{};
  std::array<Candidate, kMaxCandidates> candidates_{};
};

}  // namespace aw
