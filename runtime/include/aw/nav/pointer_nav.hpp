#pragma once

#include "aw/input/input_frame.hpp"

#include <cstdint>

namespace aw {

struct NavConfig {
  // Frames of unanswered pressing before an axis is declared blocked.
  int blocked_frames = 8;
  // Error, in screen pixels, that counts as "arrived". Half a 16 px tile.
  int snap_radius = 8;
  // Frames to release between steps. The game needs a gap to register a
  // second discrete move.
  int release_frames = 1;
  // While armed and steerable but with no indicator locked yet, PointerNav
  // emits a single one-frame exploratory D-pad pulse every this many frames
  // (and nothing on the frames between) so a mouse-only player still
  // generates the motion OamTracker's correlation needs for a first lock.
  // Unused once an indicator is found.
  int probe_interval_frames = 10;
};

// Everything the controller needs for one frame. All positions are GBA screen
// pixels; scroll is the BG scroll offset, used only to tell whether the game
// responded (a camera scroll is a response even when the sprite holds still).
struct NavInput {
  bool armed_pointer = false;  // Pointer is present, in the viewport, and has moved
  int target_x = 0;
  int target_y = 0;

  bool indicator_found = false;
  int indicator_x = 0;
  int indicator_y = 0;

  int scroll_x = 0;
  int scroll_y = 0;

  bool steerable = true;           // False in cutscenes: clicks only
  std::uint16_t device_dpad = 0;   // D-pad from a physical device this frame
  bool primary_edge = false;       // Left button / touch press edge
  bool secondary_edge = false;     // Right button press edge
};

struct NavOutput {
  std::uint16_t keys = 0;
};

// Closed-loop pointer steering.
//
// The predecessor was open-loop: it assumed each emitted D-pad pulse moved the
// cursor, so dropped inputs during animations desynced its model permanently.
// This controller only believes the indicator's observed position, so there is
// no model to desync.
class PointerNav {
public:
  PointerNav() = default;
  explicit PointerNav(NavConfig cfg) : cfg_(cfg) {}

  NavOutput step(const NavInput& in);
  void reset();

  // True while the pointer owns the D-pad.
  bool steering() const { return armed_; }

  // Frames a blocked axis waits before retrying on its own, even if the
  // wanted direction never changes. Blocked exists to stop input spam while
  // genuinely stuck, not to give up permanently: animations and screen
  // transitions routinely outlast blocked_frames, so the axis must resume
  // once the game becomes responsive again. ~0.5 s at 60 fps.
  static constexpr int kBlockedCooldownFrames = 30;

private:
  enum class Phase : std::uint8_t { Idle, Pressing, Releasing, Blocked };

  struct Axis {
    Phase phase = Phase::Idle;
    std::uint16_t dir = 0;   // Key mask currently being pressed
    int press_frames = 0;
    int release_frames = 0;
    int blocked_elapsed = 0;  // Frames spent in Phase::Blocked; drives the retry backoff
    int world_at_press = 0;  // Indicator + scroll when the press began
  };

  std::uint16_t drive_axis(Axis& axis, int error, int world,
                           std::uint16_t positive, std::uint16_t negative);

  // Emits a low-rate exploratory D-pad pulse while no indicator is locked.
  // Never presses continuously -- a held direction would run the game's own
  // cursor across the map with nothing steering it back.
  std::uint16_t probe(const NavInput& in);

  // Centre of the GBA screen (240x160), used to pick a probe direction from
  // where the pointer sits relative to it.
  static constexpr int kScreenCenterX = 120;
  static constexpr int kScreenCenterY = 80;

  NavConfig cfg_{};
  Axis x_{};
  Axis y_{};
  bool armed_ = false;
  int probe_elapsed_ = 0;  // Frames since the last exploratory pulse
};

}  // namespace aw
