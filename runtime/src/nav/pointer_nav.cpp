#include "aw/nav/pointer_nav.hpp"

#include "aw/hardware.hpp"

#include <cstdlib>

namespace aw {

void PointerNav::reset() {
  x_ = {};
  y_ = {};
  armed_ = false;
}

std::uint16_t PointerNav::drive_axis(Axis& axis, int error, int world,
                                     std::uint16_t positive, std::uint16_t negative) {
  // Arrived: stand down.
  if (std::abs(error) <= cfg_.snap_radius) {
    axis.phase = Phase::Idle;
    axis.dir = 0;
    return 0;
  }

  const std::uint16_t wanted = (error > 0) ? positive : negative;

  // A blocked axis retries only when the direction we want changes.
  if (axis.phase == Phase::Blocked) {
    if (wanted == axis.dir) return 0;
    axis.phase = Phase::Idle;
  }

  // Changing direction mid-press restarts the press.
  if (axis.phase == Phase::Pressing && wanted != axis.dir) {
    axis.phase = Phase::Idle;
  }

  switch (axis.phase) {
    case Phase::Idle:
      axis.phase = Phase::Pressing;
      axis.dir = wanted;
      axis.press_frames = 0;
      axis.world_at_press = world;
      return wanted;

    case Phase::Pressing:
      if (world != axis.world_at_press) {
        // The game responded. Release for a frame so the next press registers
        // as a distinct move.
        axis.phase = Phase::Releasing;
        axis.release_frames = 0;
        return 0;
      }
      if (++axis.press_frames >= cfg_.blocked_frames) {
        axis.phase = Phase::Blocked;
        return 0;
      }
      return axis.dir;

    case Phase::Releasing:
      if (++axis.release_frames >= cfg_.release_frames) {
        axis.phase = Phase::Idle;
      }
      return 0;

    case Phase::Blocked:
      return 0;
  }
  return 0;
}

NavOutput PointerNav::step(const NavInput& in) {
  NavOutput out;

  // Clicks are unconditional: they work in unrecognised contexts, in
  // cutscenes, and before the pointer has armed.
  if (in.primary_edge) out.keys |= kKeyA;
  if (in.secondary_edge) out.keys |= kKeyB;

  // A physical D-pad always wins, and hands control back to the player.
  if ((in.device_dpad & kDpadMask) != 0) {
    armed_ = false;
    x_ = {};
    y_ = {};
    return out;
  }

  if (!in.armed_pointer || !in.steerable || !in.indicator_found) {
    return out;
  }

  armed_ = true;

  // The scroll offset cancels out of the error (both points share it), so it
  // is needed only for the "did the game respond" test below.
  const int error_x = in.target_x - in.indicator_x;
  const int error_y = in.target_y - in.indicator_y;

  out.keys |= drive_axis(x_, error_x, in.indicator_x + in.scroll_x, kKeyRight, kKeyLeft);
  out.keys |= drive_axis(y_, error_y, in.indicator_y + in.scroll_y, kKeyDown, kKeyUp);
  return out;
}

}  // namespace aw
