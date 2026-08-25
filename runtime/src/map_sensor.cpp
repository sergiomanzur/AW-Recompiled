#include "aw/map_sensor.hpp"

#include "aw/hardware.hpp"
#include "aw/input/input_frame.hpp"

namespace aw {

void MapSensor::reset() {
  prev_keys_ = 0;
  have_prev_cursor_ = false;
  prev_cursor_ = {};
  pending_dir_x_ = 0;
  pending_dir_y_ = 0;
  pending_frames_ = 0;
  confirmations_ = 0;
  holdout_ = 0;
}

void MapSensor::on_frame(std::uint16_t keys, const CursorTile& cursor) {
  // A newly pressed D-pad direction arms a response window: the cursor is
  // expected to move along that direction within kResponseFrames. The press
  // must be an edge - holding a direction into a map edge should not keep
  // confirming map mode.
  const std::uint16_t dpad = keys & kDpadMask;
  const std::uint16_t newly = dpad & ~prev_keys_;
  if (newly != 0) {
    // Direction pressed this frame, in press priority order.
    if (newly & kKeyRight) { pending_dir_x_ = +1; pending_dir_y_ = 0; }
    else if (newly & kKeyLeft) { pending_dir_x_ = -1; pending_dir_y_ = 0; }
    else if (newly & kKeyDown) { pending_dir_x_ = 0; pending_dir_y_ = +1; }
    else if (newly & kKeyUp) { pending_dir_x_ = 0; pending_dir_y_ = -1; }
    pending_frames_ = kResponseFrames;
  }

  // A matching cursor move inside the window is a confirmation.
  if (pending_frames_ > 0) {
    --pending_frames_;
    if (cursor.found && have_prev_cursor_ && prev_cursor_.found) {
      const int dx = cursor.x - prev_cursor_.x;
      const int dy = cursor.y - prev_cursor_.y;
      const bool matches = (pending_dir_x_ > 0 && dx > 0) ||
                           (pending_dir_x_ < 0 && dx < 0) ||
                           (pending_dir_y_ > 0 && dy > 0) ||
                           (pending_dir_y_ < 0 && dy < 0);
      if (matches) {
        ++confirmations_;
        holdout_ = kHoldoutFrames;
        pending_frames_ = 0;  // Consumed; one confirmation per press.
      }
    }
  }

  if (holdout_ > 0 && confirmations_ >= kConfirmThreshold) {
    --holdout_;
    if (holdout_ == 0) {
      confirmations_ = 0;  // Map mode dropped; require fresh evidence to return.
    }
  }

  prev_keys_ = keys;
  if (cursor.found) {
    prev_cursor_ = cursor;
    have_prev_cursor_ = true;
  }
}

}  // namespace aw
