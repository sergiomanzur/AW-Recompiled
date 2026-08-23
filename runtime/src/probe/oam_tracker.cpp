#include "aw/probe/oam_tracker.hpp"

#include "aw/hardware.hpp"

#include <cstdlib>

namespace aw {

void OamTracker::reset() {
  signature_ = {};
  locked_ = false;
  has_prev_ = false;
  missing_frames_ = 0;
  prev_ = {};
  candidates_ = {};
}

void OamTracker::set_signature(const IndicatorSignature& sig) {
  signature_ = sig;
  locked_ = !sig.wildcard();
  missing_frames_ = 0;
}

OamTracker::Candidate* OamTracker::candidate_for(const IndicatorSignature& sig) {
  Candidate* free_slot = nullptr;
  for (auto& c : candidates_) {
    if (c.used && c.sig.tile == sig.tile && c.sig.palette == sig.palette) {
      return &c;
    }
    if (!c.used && free_slot == nullptr) {
      free_slot = &c;
    }
  }
  if (free_slot != nullptr) {
    free_slot->used = true;
    free_slot->sig = sig;
    free_slot->score = 0;
    return free_slot;
  }
  return nullptr;
}

Indicator OamTracker::find_by_signature(const std::uint8_t* oam) const {
  Indicator result;
  for (std::size_t i = 0; i < kOamEntryCount; ++i) {
    const OamEntry e = decode_oam_entry(oam, i);
    if (!e.on_screen()) continue;
    if (!signature_.matches(e)) continue;
    result.found = true;
    result.screen_x = e.x;
    result.screen_y = e.y;
    result.oam_index = i;
    return result;  // Lowest index wins.
  }
  return result;
}

void OamTracker::correlate(const std::uint8_t* oam, std::uint16_t emitted_dpad) {
  const bool right = (emitted_dpad & kKeyRight) != 0;
  const bool left = (emitted_dpad & kKeyLeft) != 0;
  const bool down = (emitted_dpad & kKeyDown) != 0;
  const bool up = (emitted_dpad & kKeyUp) != 0;
  if (!right && !left && !down && !up) return;
  if (!has_prev_) return;

  for (std::size_t i = 0; i < kOamEntryCount; ++i) {
    const OamEntry cur = decode_oam_entry(oam, i);
    const OamEntry& prev = prev_[i];
    if (!cur.on_screen() || !prev.on_screen()) continue;
    // A sprite that changed identity is a different object reusing the slot.
    if (cur.tile != prev.tile || cur.palette != prev.palette) continue;

    const int dx = cur.x - prev.x;
    const int dy = cur.y - prev.y;
    if (dx == 0 && dy == 0) continue;
    if (std::abs(dx) > kMaxStepPixels || std::abs(dy) > kMaxStepPixels) continue;

    const bool agrees = (right && dx > 0) || (left && dx < 0) ||
                        (down && dy > 0) || (up && dy < 0);
    const bool opposes = (right && dx < 0) || (left && dx > 0) ||
                         (down && dy < 0) || (up && dy > 0);
    if (!agrees && !opposes) continue;

    IndicatorSignature sig{cur.tile, cur.palette};
    Candidate* c = candidate_for(sig);
    if (c == nullptr) continue;
    c->score += agrees ? 1 : -1;

    if (!locked_ && c->score >= kLockScore) {
      signature_ = sig;
      locked_ = true;
      missing_frames_ = 0;
    }
  }
}

Indicator OamTracker::update(const std::uint8_t* oam, std::uint16_t emitted_dpad) {
  if (oam == nullptr) {
    has_prev_ = false;
    return {};
  }

  correlate(oam, emitted_dpad);

  Indicator result;
  if (locked_) {
    result = find_by_signature(oam);
    if (result.found) {
      missing_frames_ = 0;
    } else if (++missing_frames_ >= kUnlockFrames) {
      // The signature stopped matching: probably a context change. Drop the
      // lock and let correlation find the new indicator.
      locked_ = false;
      signature_ = {};
      candidates_ = {};
      missing_frames_ = 0;
    }
  }

  for (std::size_t i = 0; i < kOamEntryCount; ++i) {
    prev_[i] = decode_oam_entry(oam, i);
  }
  has_prev_ = true;
  return result;
}

}  // namespace aw
