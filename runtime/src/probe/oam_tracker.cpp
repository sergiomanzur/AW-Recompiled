#include "aw/probe/oam_tracker.hpp"

#include "aw/hardware.hpp"

#include <cstdlib>

namespace aw {

void OamTracker::reset() {
  signature_ = {};
  locked_ = false;
  has_prev_ = false;
  missing_frames_ = 0;
  locked_index_ = kOamEntryCount;
  prev_ = {};
  candidates_ = {};
}

void OamTracker::set_signature(const IndicatorSignature& sig) {
  signature_ = sig;
  locked_ = !sig.wildcard();
  missing_frames_ = 0;
  // The caller supplied a signature, not a specific slot: any anchor left
  // over from a previous correlation lock is meaningless now. find_by_
  // signature() will pick up whichever on-screen entry matches first and
  // anchor to that.
  locked_index_ = kOamEntryCount;
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

Indicator OamTracker::find_by_signature(const std::uint8_t* oam) {
  // Prefer the anchored slot: it's the specific sprite that earned the lock,
  // not merely "some sprite with this tile+palette" (many units in Advance
  // Wars share a graphic, so a stationary decoy with the same signature must
  // never be reported instead of the sprite that actually moved).
  if (locked_index_ < kOamEntryCount) {
    const OamEntry anchored = decode_oam_entry(oam, locked_index_);
    if (anchored.on_screen() && signature_.matches(anchored)) {
      Indicator result;
      result.found = true;
      result.screen_x = anchored.x;
      result.screen_y = anchored.y;
      result.oam_index = locked_index_;
      return result;
    }
  }

  // The anchor is gone (slot reused, sprite reordered in OAM, or no anchor
  // yet). Fall back to a fresh scan and re-anchor to whatever it finds.
  Indicator result;
  for (std::size_t i = 0; i < kOamEntryCount; ++i) {
    const OamEntry e = decode_oam_entry(oam, i);
    if (!e.on_screen()) continue;
    if (!signature_.matches(e)) continue;
    result.found = true;
    result.screen_x = e.x;
    result.screen_y = e.y;
    result.oam_index = i;
    locked_index_ = i;
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
    // A held diagonal chord (e.g. Right+Down) can agree on one axis and
    // oppose on the other simultaneously. That frame is ambiguous for this
    // sprite: don't credit or penalize it.
    if (agrees && opposes) continue;

    IndicatorSignature sig{cur.tile, cur.palette};
    Candidate* c = candidate_for(sig);
    if (c == nullptr) continue;
    c->score += agrees ? 1 : -1;

    if (!locked_ && c->score >= kLockScore) {
      signature_ = sig;
      locked_ = true;
      locked_index_ = i;
      missing_frames_ = 0;
    } else if (locked_ && sig.tile == signature_.tile &&
               sig.palette == signature_.palette && c->score <= kUnlockScore) {
      // The sprite we anchored to has stalled or started moving the wrong
      // way for long enough that its net score gave out. Drop the lock
      // exactly as the kUnlockFrames absence path does, and let correlation
      // start over.
      locked_ = false;
      signature_ = {};
      locked_index_ = kOamEntryCount;
      candidates_ = {};
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
      locked_index_ = kOamEntryCount;
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
