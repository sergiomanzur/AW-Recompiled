#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/hardware.hpp"
#include "aw/nav/pointer_nav.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

// A stand-in for the game: the indicator moves 16 px in the commanded
// direction, but only after the key has been held for `latency` frames, and
// only if `frozen` is false.
struct FakeGame {
  int x = 0;
  int y = 0;
  int scroll_x = 0;
  int scroll_y = 0;
  int latency = 1;
  bool frozen = false;
  int held = 0;
  std::uint16_t last_dir = 0;

  void apply(std::uint16_t keys) {
    const std::uint16_t dir = keys & aw::kDpadMask;
    if (dir == 0 || dir != last_dir) {
      held = 0;
      last_dir = dir;
      if (dir == 0) return;
    }
    ++held;
    if (frozen || held < latency) return;
    held = 0;
    if (dir & aw::kKeyRight) x += 16;
    if (dir & aw::kKeyLeft) x -= 16;
    if (dir & aw::kKeyDown) y += 16;
    if (dir & aw::kKeyUp) y -= 16;
  }
};

aw::NavInput make_input(const FakeGame& game, int target_x, int target_y) {
  aw::NavInput in;
  in.armed_pointer = true;
  in.steerable = true;
  in.target_x = target_x;
  in.target_y = target_y;
  in.indicator_found = true;
  in.indicator_x = game.x;
  in.indicator_y = game.y;
  in.scroll_x = game.scroll_x;
  in.scroll_y = game.scroll_y;
  return in;
}

void tests_converges_on_the_target() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 16;
  game.y = 16;

  const int target_x = 128;
  const int target_y = 96;

  int frames = 0;
  for (; frames < 300; ++frames) {
    const aw::NavOutput out = nav.step(make_input(game, target_x, target_y));
    game.apply(out.keys);
    if (std::abs(game.x - target_x) <= 8 && std::abs(game.y - target_y) <= 8) break;
  }

  require_equal(frames < 300, true, "converged within the frame budget");
  require_equal(std::abs(game.x - target_x) <= 8, true, "x within snap radius");
  require_equal(std::abs(game.y - target_y) <= 8, true, "y within snap radius");
}

void tests_deadband_emits_nothing() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 100;
  game.y = 100;

  // Target 4 px away on both axes: inside the 8 px snap radius.
  const aw::NavOutput out = nav.step(make_input(game, 104, 96));
  require_equal(out.keys & aw::kDpadMask, std::uint16_t{0}, "no dpad inside deadband");
}

void tests_releases_between_steps() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 0;
  game.y = 0;

  // A real release phase inserts more than just the single frame on which
  // motion happens to be detected: the axis must sit through a full
  // `Releasing` visit, silent, before it presses again. If the release phase
  // were skipped entirely (motion detected -> straight back to Idle), the
  // next call would re-press immediately, collapsing the gap between two
  // presses to one frame. So look at the longest run of silent frames that
  // separates two presses, not merely whether at least one exists — a
  // single-frame check can't tell "released for a frame" apart from "always
  // returns 0 on the frame it notices motion", which happens either way.
  int silent_run = 0;
  int max_gap_between_presses = 0;
  bool have_prev_press = false;

  for (int i = 0; i < 40; ++i) {
    const aw::NavOutput out = nav.step(make_input(game, 200, 0));
    const bool pressed = (out.keys & aw::kDpadMask) != 0;
    if (pressed) {
      if (have_prev_press && silent_run > max_gap_between_presses) {
        max_gap_between_presses = silent_run;
      }
      have_prev_press = true;
      silent_run = 0;
    } else {
      ++silent_run;
    }
    game.apply(out.keys);
  }

  require_equal(max_gap_between_presses >= 2, true,
                "at least two silent frames separate presses after motion");
}

void tests_blocked_axis_stops_emitting() {
  aw::NavConfig cfg;
  cfg.blocked_frames = 4;
  aw::PointerNav nav(cfg);

  FakeGame game;
  game.frozen = true;  // The game never responds.
  game.x = 0;

  // Press for longer than blocked_frames.
  for (int i = 0; i < cfg.blocked_frames + 2; ++i) {
    const aw::NavOutput out = nav.step(make_input(game, 200, 0));
    game.apply(out.keys);
  }

  // Once blocked, it must stay quiet rather than spamming the core.
  for (int i = 0; i < 10; ++i) {
    const aw::NavOutput out = nav.step(make_input(game, 200, 0));
    require_equal(out.keys & aw::kDpadMask, std::uint16_t{0}, "blocked axis stays quiet");
  }
}

void tests_blocked_axis_recovers_when_target_reverses() {
  aw::NavConfig cfg;
  cfg.blocked_frames = 4;
  aw::PointerNav nav(cfg);

  FakeGame game;
  game.frozen = true;
  game.x = 100;

  for (int i = 0; i < cfg.blocked_frames + 2; ++i) {
    nav.step(make_input(game, 200, 100));  // Steering right, blocked.
  }

  // Target moves to the other side: the axis must try again.
  const aw::NavOutput out = nav.step(make_input(game, 0, 100));
  require_equal((out.keys & aw::kKeyLeft) != 0, true, "retries after direction change");
}

void tests_blocked_axis_recovers_after_cooldown() {
  aw::NavConfig cfg;
  cfg.blocked_frames = 4;
  aw::PointerNav nav(cfg);

  FakeGame game;
  game.frozen = true;  // The game never responds.
  game.x = 0;

  const int target_x = 200;

  // Drive until the axis gives up and is blocked.
  for (int i = 0; i < cfg.blocked_frames + 2; ++i) {
    nav.step(make_input(game, target_x, 0));
  }

  // The game becomes responsive again, but the target never moved, so there
  // is no direction flip to exploit. Blocked exists to stop input spam, not
  // to give up forever: animations and screen transitions routinely outlast
  // blocked_frames, so the axis must retry on its own within the cooldown
  // window.
  game.frozen = false;
  int frame_of_retry = -1;
  for (int i = 0; i < aw::PointerNav::kBlockedCooldownFrames + 2; ++i) {
    const aw::NavOutput out = nav.step(make_input(game, target_x, 0));
    game.apply(out.keys);
    if (out.keys & aw::kKeyRight) {
      frame_of_retry = i;
      break;
    }
  }

  require_equal(frame_of_retry >= 0, true,
                "blocked axis retries within the cooldown window once the target is reachable again");

  // Having retried, it must keep going and actually reach the target rather
  // than pressing once and re-blocking.
  int frames = 0;
  for (; frames < 300; ++frames) {
    const aw::NavOutput out = nav.step(make_input(game, target_x, 0));
    game.apply(out.keys);
    if (std::abs(game.x - target_x) <= cfg.snap_radius) break;
  }
  require_equal(frames < 300, true, "converges after the blocked axis recovers");
}

void tests_scroll_counts_as_motion() {
  aw::NavConfig cfg;
  cfg.blocked_frames = 4;
  aw::PointerNav nav(cfg);

  FakeGame game;
  game.x = 100;
  game.y = 100;

  // The indicator never moves on screen, but the camera scrolls: the game IS
  // responding, so the axis must never be declared blocked. Count presses over
  // a long run rather than sampling one frame — the controller is legitimately
  // silent during its release frames.
  int presses = 0;
  for (int i = 0; i < 30; ++i) {
    const aw::NavOutput out = nav.step(make_input(game, 220, 100));
    if (out.keys & aw::kKeyRight) {
      ++presses;
      game.scroll_x += 16;
    }
  }

  // A blocked axis would have fallen silent after about blocked_frames presses.
  require_equal(presses > cfg.blocked_frames, true, "scrolling is not treated as blocked");
}

void tests_physical_dpad_disarms_steering() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 0;

  aw::NavInput in = make_input(game, 200, 0);
  in.device_dpad = aw::kKeyUp;  // The player touched the keyboard or pad.

  const aw::NavOutput out = nav.step(in);
  require_equal(out.keys & aw::kDpadMask, std::uint16_t{0}, "device dpad wins");
  require_equal(nav.steering(), false, "steering disarmed");
}

void tests_steering_clears_when_pointer_leaves() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 100;
  game.y = 100;

  // Arm it first.
  nav.step(make_input(game, 200, 100));
  require_equal(nav.steering(), true, "armed after a normal frame");

  // The pointer leaves the viewport (or the device goes idle): steering()
  // must stop claiming ownership of the D-pad.
  aw::NavInput in = make_input(game, 200, 100);
  in.armed_pointer = false;
  nav.step(in);

  require_equal(nav.steering(), false, "steering clears once the pointer is no longer armed");
}

void tests_steering_clears_when_indicator_lost() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 100;
  game.y = 100;

  nav.step(make_input(game, 200, 100));
  require_equal(nav.steering(), true, "armed after a normal frame");

  // The probe loses the indicator (e.g. mid screen-transition): steering()
  // must stop claiming ownership of the D-pad.
  aw::NavInput in = make_input(game, 200, 100);
  in.indicator_found = false;
  nav.step(in);

  require_equal(nav.steering(), false, "steering clears once the indicator is lost");
}

void tests_unarmed_pointer_emits_nothing() {
  aw::PointerNav nav;
  FakeGame game;

  aw::NavInput in = make_input(game, 200, 0);
  in.armed_pointer = false;

  const aw::NavOutput out = nav.step(in);
  require_equal(out.keys, std::uint16_t{0}, "unarmed pointer is silent");
}

void tests_missing_indicator_emits_nothing() {
  aw::PointerNav nav;
  FakeGame game;

  aw::NavInput in = make_input(game, 200, 0);
  in.indicator_found = false;

  const aw::NavOutput out = nav.step(in);
  require_equal(out.keys & aw::kDpadMask, std::uint16_t{0}, "no indicator, no steering");
}

void tests_unsteerable_context_emits_no_dpad_but_still_clicks() {
  aw::PointerNav nav;
  FakeGame game;

  aw::NavInput in = make_input(game, 200, 0);
  in.steerable = false;
  in.primary_edge = true;

  const aw::NavOutput out = nav.step(in);
  require_equal(out.keys & aw::kDpadMask, std::uint16_t{0}, "cutscene does not steer");
  require_equal((out.keys & aw::kKeyA) != 0, true, "cutscene still clicks");
}

void tests_click_edges_map_to_a_and_b() {
  aw::PointerNav nav;
  FakeGame game;
  game.x = 100;
  game.y = 100;

  aw::NavInput in = make_input(game, 100, 100);
  in.primary_edge = true;
  require_equal((nav.step(in).keys & aw::kKeyA) != 0, true, "left click is A");

  in.primary_edge = false;
  in.secondary_edge = true;
  require_equal((nav.step(in).keys & aw::kKeyB) != 0, true, "right click is B");
}

void tests_clicks_work_even_when_unarmed() {
  aw::PointerNav nav;
  FakeGame game;

  aw::NavInput in = make_input(game, 100, 100);
  in.armed_pointer = false;
  in.primary_edge = true;

  require_equal((nav.step(in).keys & aw::kKeyA) != 0, true, "click without motion");
}

void tests_reset_clears_state() {
  aw::NavConfig cfg;
  cfg.blocked_frames = 2;
  aw::PointerNav nav(cfg);

  FakeGame game;
  game.frozen = true;
  for (int i = 0; i < 5; ++i) nav.step(make_input(game, 200, 0));

  nav.reset();

  const aw::NavOutput out = nav.step(make_input(game, 200, 0));
  require_equal((out.keys & aw::kKeyRight) != 0, true, "reset clears the blocked axis");
}

}  // namespace

int main() {
  try {
    tests_converges_on_the_target();
    tests_deadband_emits_nothing();
    tests_releases_between_steps();
    tests_blocked_axis_stops_emitting();
    tests_blocked_axis_recovers_when_target_reverses();
    tests_blocked_axis_recovers_after_cooldown();
    tests_scroll_counts_as_motion();
    tests_physical_dpad_disarms_steering();
    tests_steering_clears_when_pointer_leaves();
    tests_steering_clears_when_indicator_lost();
    tests_unarmed_pointer_emits_nothing();
    tests_missing_indicator_emits_nothing();
    tests_unsteerable_context_emits_no_dpad_but_still_clicks();
    tests_click_edges_map_to_a_and_b();
    tests_clicks_work_even_when_unarmed();
    tests_reset_clears_state();
    std::cout << "pointer_nav_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
