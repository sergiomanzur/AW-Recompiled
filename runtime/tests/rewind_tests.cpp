#include "aw/rewind.hpp"

#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

namespace {

int failures = 0;

void require(bool condition, const std::string& label) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label.c_str());
  }
}

// Fake snapshot store: each capture allocates a boxed int; restore records
// the value so tests can assert which snapshot was loaded.
struct FakeStore {
  std::vector<int> restored;
  int next_value = 0;
  int fail_after = -1;  // capture N (0-based) fails when >= fail_after
  bool fail_restore = false;
  int captures = 0;
  int live_handles = 0;
};

void* fake_capture(void* user) {
  auto* store = static_cast<FakeStore*>(user);
  if (store->fail_after >= 0 && store->captures >= store->fail_after) {
    return nullptr;
  }
  ++store->captures;
  ++store->live_handles;
  return new int(store->next_value++);
}

bool fake_restore(void* user, void* snapshot) {
  auto* store = static_cast<FakeStore*>(user);
  if (store->fail_restore) {
    return false;
  }
  store->restored.push_back(*static_cast<int*>(snapshot));
  return true;
}

void fake_release(void* user, void* snapshot) {
  auto* store = static_cast<FakeStore*>(user);
  --store->live_handles;
  delete static_cast<int*>(snapshot);
}

std::uint64_t fake_size(void* /*user*/, void* snapshot) {
  return sizeof(int);
}

aw::RewindIo make_io(FakeStore& store) {
  aw::RewindIo io;
  io.capture = fake_capture;
  io.restore = fake_restore;
  io.release = fake_release;
  io.size = fake_size;
  io.user = &store;
  return io;
}

void test_captures_on_interval() {
  FakeStore store;
  aw::RewindBuffer rewind(8, 3);
  rewind.set_io(make_io(store));

  for (int frame = 0; frame < 3; ++frame) {
    rewind.on_frame();
  }
  require(rewind.size() == 1, "first snapshot after 3 frames");
  for (int frame = 0; frame < 3; ++frame) {
    rewind.on_frame();
  }
  require(rewind.size() == 2, "second snapshot after 6 frames");
  require(store.live_handles == 2, "both handles live");
}

void test_rewind_steps_restore_newest_first() {
  FakeStore store;
  aw::RewindBuffer rewind(8, 1);
  rewind.set_io(make_io(store));

  for (int frame = 0; frame < 4; ++frame) {
    rewind.on_frame();
  }
  require(rewind.size() == 4, "four snapshots captured");

  require(rewind.rewind_step(), "step 1 restores");
  require(rewind.rewind_step(), "step 2 restores");
  require(store.restored.size() == std::size_t(2), "two restores recorded");
  require(store.restored[0] == 3, "newest snapshot restored first");
  require(store.restored[1] == 2, "second-newest restored second");
  require(rewind.size() == 2, "history shrunk by two");
  require(store.live_handles == 2, "released handles freed");
}

void test_rewind_empty_returns_false() {
  FakeStore store;
  aw::RewindBuffer rewind(8, 1);
  rewind.set_io(make_io(store));

  require(!rewind.rewind_step(), "empty history does not restore");
  require(store.restored.empty(), "nothing recorded");
}

void test_ring_overwrites_oldest() {
  FakeStore store;
  aw::RewindBuffer rewind(3, 1);
  rewind.set_io(make_io(store));

  for (int frame = 0; frame < 6; ++frame) {
    rewind.on_frame();
  }
  require(rewind.size() == 3, "capacity clamps at 3");
  require(store.live_handles == 3, "only live handles kept");

  require(rewind.rewind_step(), "restore works at capacity");
  require(store.restored.back() == 5, "newest of the wrapped ring restored");
}

void test_reset_releases_everything() {
  FakeStore store;
  aw::RewindBuffer rewind(8, 1);
  rewind.set_io(make_io(store));

  for (int frame = 0; frame < 5; ++frame) {
    rewind.on_frame();
  }
  rewind.reset();
  require(rewind.empty(), "reset empties the ring");
  require(store.live_handles == 0, "reset frees all handles");
  require(!rewind.rewind_step(), "no restore after reset");

  // The buffer stays usable after a reset.
  rewind.on_frame();
  require(rewind.size() == 1, "captures resume after reset");
}

void test_capture_failures_disable() {
  FakeStore store;
  store.fail_after = 0;
  aw::RewindBuffer rewind(8, 1);
  rewind.set_io(make_io(store));

  for (int frame = 0; frame < 4; ++frame) {
    rewind.on_frame();
  }
  require(rewind.disabled(), "disabled after consecutive failures");
  require(rewind.empty(), "nothing captured while failing");
  require(!rewind.rewind_step(), "disabled buffer will not restore");
}

void test_restore_failure_disables() {
  FakeStore store;
  aw::RewindBuffer rewind(8, 1);
  rewind.set_io(make_io(store));

  for (int frame = 0; frame < 2; ++frame) {
    rewind.on_frame();
  }
  store.fail_restore = true;
  require(!rewind.rewind_step(), "restore failure reported");
  require(rewind.disabled(), "restore failure disables the buffer");
  require(store.live_handles == 1, "failed snapshot still released");
}

void test_bytes_held_tracks_snapshots() {
  FakeStore store;
  aw::RewindBuffer rewind(4, 1);
  rewind.set_io(make_io(store));

  for (int frame = 0; frame < 6; ++frame) {
    rewind.on_frame();
  }
  require(rewind.total_bytes_held() == std::uint64_t(4) * sizeof(int),
          "byte accounting matches live snapshots");
  rewind.rewind_step();
  require(rewind.total_bytes_held() == std::uint64_t(3) * sizeof(int),
          "bytes released on rewind");
}

void test_window_evicts_aged_snapshots() {
  FakeStore store;
  // One snapshot per frame, evict anything older than 5 frames.
  aw::RewindBuffer rewind(32, 1, 5);
  rewind.set_io(make_io(store));

  for (int frame = 0; frame < 10; ++frame) {
    rewind.on_frame();
  }
  // Captures at frames 1..10; at frame 10 only ages <= 5 survive (frames 5..10).
  require(rewind.size() == 6, "window clamps ring at 6 snapshots");
  require(store.live_handles == 6, "aged-out handles released");

  // Restores walk back only as far as the window edge.
  for (int expected = 9; expected >= 4; --expected) {
    require(rewind.rewind_step(), "step inside the window restores");
    require(store.restored.back() == expected, "newest-in-window restored in order");
  }
  require(!rewind.rewind_step(), "cannot rewind past the window");
  require(rewind.empty(), "window-bounded history exhausted");
}

void test_window_never_exceeds_max_reach() {
  FakeStore store;
  // Interval 3, window 6: reach is at most 6 frames regardless of capacity.
  aw::RewindBuffer rewind(32, 3, 6);
  rewind.set_io(make_io(store));

  for (int frame = 0; frame < 30; ++frame) {
    rewind.on_frame();
  }
  // Live captures sit at frames 24, 27, 30 (ages 6, 3, 0).
  require(rewind.size() == 3, "only in-window snapshots kept");

  int steps = 0;
  while (rewind.rewind_step()) {
    ++steps;
  }
  require(steps == 3, "all restores stay inside the window");
  require(store.restored.size() == std::size_t(3), "one restore per live snapshot");
}

void test_window_edge_survives_exactly_at_limit() {
  FakeStore store;
  aw::RewindBuffer rewind(32, 1, 5);
  rewind.set_io(make_io(store));

  for (int frame = 0; frame < 6; ++frame) {
    rewind.on_frame();
  }
  // At frame 6 the frame-1 snapshot is exactly 5 frames old: still reachable.
  require(rewind.size() == 6, "boundary-age snapshot survives");
  rewind.on_frame();
  require(rewind.size() == 6, "snapshot ages out one frame past the window");
}

}  // namespace

int main() {
  test_captures_on_interval();
  test_rewind_steps_restore_newest_first();
  test_rewind_empty_returns_false();
  test_ring_overwrites_oldest();
  test_reset_releases_everything();
  test_capture_failures_disable();
  test_restore_failure_disables();
  test_bytes_held_tracks_snapshots();
  test_window_evicts_aged_snapshots();
  test_window_never_exceeds_max_reach();
  test_window_edge_survives_exactly_at_limit();

  if (failures == 0) {
    std::printf("rewind_tests: all tests passed\n");
    return 0;
  }
  std::fprintf(stderr, "rewind_tests: %d failure(s)\n", failures);
  return 1;
}
