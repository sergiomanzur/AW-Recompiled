#pragma once

#include "aw/probe/cursor_probe.hpp"

#include <cstdint>

namespace aw {

// Detects "the player is commanding the map" from evidence everyone can
// trust: the mined cursor coordinates (see data/symbols) moving in response
// to D-pad presses. No unverified address reads, no screen scraping.
//
// Feed it once per emulated frame: the keys that were sent to the core that
// frame and the cursor tile read after it ran. A D-pad press followed by a
// matching cursor move within a short window confirms map mode; a stretch of
// frames with no confirmation (cursor frozen by a menu, a dialogue, an AI
// turn) drops it again. Hysteresis keeps the flag stable while the player
// thinks between presses.
class MapSensor {
public:
  // Confirmations needed before map mode is first reported.
  static constexpr int kConfirmThreshold = 1;
  // Frames without a new confirmation before map mode is dropped. Long
  // enough that thinking between moves keeps the flag set, short enough that
  // a menu or dialogue drops it promptly (~2.5 s at 60 fps).
  static constexpr int kHoldoutFrames = 150;
  // Frames after a D-pad press in which a cursor move still counts as a
  // response (the game consumes input with a little latency).
  static constexpr int kResponseFrames = 12;

  // `keys` are the GBA keys sent to the core this frame; `cursor` is the
  // cursor tile read after the frame ran (found=false when unavailable).
  void on_frame(std::uint16_t keys, const CursorTile& cursor);

  bool in_map() const { return confirmations_ >= kConfirmThreshold && holdout_ > 0; }
  int confirmations() const { return confirmations_; }
  void reset();

private:
  std::uint16_t prev_keys_ = 0;
  bool have_prev_cursor_ = false;
  CursorTile prev_cursor_{};
  int pending_dir_x_ = 0;  // Expected delta when a D-pad press is pending
  int pending_dir_y_ = 0;
  int pending_frames_ = 0;
  int confirmations_ = 0;
  int holdout_ = 0;
};

}  // namespace aw
