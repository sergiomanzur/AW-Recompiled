#include "aw/nav/pointer_nav.hpp"

#include "aw/hardware.hpp"

#include <algorithm>
#include <cstdlib>

namespace aw {

void PointerNav::reset() {
  x_ = {};
  y_ = {};
  armed_ = false;
  probe_elapsed_ = 0;
}

int PointerNav::effective_deadband(const Axis& axis) const {
  if (axis.observed_step <= 0) return cfg_.snap_radius;
  return std::max(2, axis.observed_step / 2);
}

std::uint16_t PointerNav::drive_axis(Axis& axis, int error, int world,
                                     std::uint16_t positive, std::uint16_t negative) {
  // Arrived: stand down.
  if (std::abs(error) <= effective_deadband(axis)) {
    axis.phase = Phase::Idle;
    axis.dir = 0;
    axis.blocked_elapsed = 0;
    return 0;
  }

  const std::uint16_t wanted = (error > 0) ? positive : negative;

  // A blocked axis retries immediately if the wanted direction changes.
  // Otherwise it falls through to the switch below, which counts down a
  // cooldown rather than staying silent forever -- the block may have been
  // caused by a transient animation or screen transition that has since
  // ended.
  if (axis.phase == Phase::Blocked && wanted != axis.dir) {
    axis.phase = Phase::Idle;
    axis.blocked_elapsed = 0;
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
        // The game responded. Record how far it actually moved: that
        // magnitude is this screen's real step size, which sets next call's
        // deadband (see effective_deadband()). Release for a frame so the
        // next press registers as a distinct move.
        axis.observed_step = std::abs(world - axis.world_at_press);
        axis.phase = Phase::Releasing;
        axis.release_frames = 0;
        return 0;
      }
      if (++axis.press_frames >= cfg_.blocked_frames) {
        axis.phase = Phase::Blocked;
        axis.blocked_elapsed = 0;
        return 0;
      }
      return axis.dir;

    case Phase::Releasing:
      if (++axis.release_frames >= cfg_.release_frames) {
        axis.phase = Phase::Idle;
      }
      return 0;

    case Phase::Blocked:
      // Backoff, not a dead end: retry on our own once the cooldown expires
      // even if nothing else has changed.
      if (++axis.blocked_elapsed >= kBlockedCooldownFrames) {
        axis.phase = Phase::Idle;
        axis.blocked_elapsed = 0;
      }
      return 0;
  }
  return 0;
}

std::uint16_t PointerNav::probe(const NavInput& in) {
  // Explore at a low duty cycle: one frame pressed, `probe_interval_frames -
  // 1` frames silent. A held direction would run the game's own cursor
  // across the map with nothing steering it back, so this must stay a pulse,
  // never a hold.
  if (++probe_elapsed_ < cfg_.probe_interval_frames) return 0;
  probe_elapsed_ = 0;

  // Bias exploration toward the pointer: pick the axis with the larger
  // offset from screen centre and pulse toward it. Moving the mouse toward a
  // target is itself what starts walking the cursor that way.
  const int dx = in.target_x - kScreenCenterX;
  const int dy = in.target_y - kScreenCenterY;
  if (std::abs(dx) >= std::abs(dy)) {
    return (dx >= 0) ? kKeyRight : kKeyLeft;
  }
  return (dy >= 0) ? kKeyDown : kKeyUp;
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

  if (!in.armed_pointer || !in.steerable) {
    // Not actually steering: drop the D-pad's authority and the axis state
    // machines together, so steering() stops lying and no stale
    // world_at_press baseline survives into whatever comes back into view
    // next.
    reset();
    return out;
  }

  if (!in.indicator_found) {
    // Armed and steerable, but no lock yet. Closed-loop steering is not
    // possible with no known indicator position, so fall back to
    // exploratory pulses instead of going silent -- silence is exactly the
    // bug: it never gives OamTracker's correlation a signal to lock onto, so
    // a mouse-only player would never see the cursor move on any screen.
    armed_ = false;
    x_ = {};
    y_ = {};
    out.keys |= probe(in);
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
