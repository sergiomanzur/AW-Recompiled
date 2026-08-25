#include "aw/hardware.hpp"
#include "aw/map_sensor.hpp"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

void require(bool condition, const std::string& label) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label.c_str());
  }
}

aw::CursorTile tile(int x, int y) {
  aw::CursorTile t;
  t.found = true;
  t.x = x;
  t.y = y;
  return t;
}

void feed_idle(aw::MapSensor& sensor, int frames) {
  for (int i = 0; i < frames; ++i) {
    sensor.on_frame(0, tile(5, 5));
  }
}

void test_cursor_following_dpad_confirms_map() {
  aw::MapSensor sensor;
  require(!sensor.in_map(), "starts outside map mode");

  // Press Right (edge), cursor moves right a few frames later.
  sensor.on_frame(aw::kKeyRight, tile(5, 5));
  sensor.on_frame(0, tile(5, 5));
  sensor.on_frame(0, tile(6, 5));
  require(sensor.in_map(), "cursor answering D-pad confirms map mode");
}

void test_frozen_cursor_drops_map() {
  aw::MapSensor sensor;
  sensor.on_frame(aw::kKeyRight, tile(5, 5));
  sensor.on_frame(0, tile(6, 5));
  require(sensor.in_map(), "confirmed");

  // Menu opens: D-pad presses no longer move the cursor. After the holdout
  // the sensor must drop map mode.
  for (int i = 0; i < aw::MapSensor::kHoldoutFrames + 2; ++i) {
    sensor.on_frame((i % 8 == 0) ? aw::kKeyDown : 0, tile(6, 5));
  }
  require(!sensor.in_map(), "frozen cursor drops map mode");

  // Back on the map: a D-pad press that moves the cursor re-confirms.
  sensor.on_frame(aw::kKeyUp, tile(6, 5));
  sensor.on_frame(0, tile(6, 4));
  require(sensor.in_map(), "re-confirmed after menu closes");
}

void test_wrong_direction_does_not_confirm() {
  aw::MapSensor sensor;
  // Press Right but the cursor moves up (a menu, not the map cursor).
  sensor.on_frame(aw::kKeyRight, tile(5, 5));
  for (int i = 0; i < aw::MapSensor::kResponseFrames + 2; ++i) {
    sensor.on_frame(0, tile(5, 4));
  }
  require(!sensor.in_map(), "unrelated movement is not map evidence");
}

void test_held_key_only_counts_once() {
  aw::MapSensor sensor;
  // Holding Right across many frames: only the initial edge arms the check,
  // and one matching move confirms once - repeated moves while held do not
  // keep the flag alive without further presses.
  sensor.on_frame(aw::kKeyRight, tile(5, 5));
  sensor.on_frame(aw::kKeyRight, tile(6, 5));
  require(sensor.in_map(), "confirmed on the edge press");

  // Key still held, cursor keeps sliding, then the game opens a menu and
  // everything freezes. Holdout counts down regardless of the held key.
  for (int i = 0; i < aw::MapSensor::kHoldoutFrames + 2; ++i) {
    sensor.on_frame(aw::kKeyRight, tile(6, 5));
  }
  require(!sensor.in_map(), "held key alone does not keep map mode");
}

void test_missing_cursor_never_confirms() {
  aw::MapSensor sensor;
  aw::CursorTile missing;
  for (int i = 0; i < 30; ++i) {
    sensor.on_frame((i % 10 == 0) ? aw::kKeyLeft : 0, missing);
  }
  require(!sensor.in_map(), "no cursor data, no map mode");
}

void test_reset() {
  aw::MapSensor sensor;
  sensor.on_frame(aw::kKeyRight, tile(5, 5));
  sensor.on_frame(0, tile(6, 5));
  sensor.reset();
  require(!sensor.in_map(), "reset clears map mode");
}

}  // namespace

int main() {
  test_cursor_following_dpad_confirms_map();
  test_frozen_cursor_drops_map();
  test_wrong_direction_does_not_confirm();
  test_held_key_only_counts_once();
  test_missing_cursor_never_confirms();
  test_reset();

  if (failures == 0) {
    std::printf("map_sensor_tests: all tests passed\n");
    return 0;
  }
  std::fprintf(stderr, "map_sensor_tests: %d failure(s)\n", failures);
  return 1;
}
