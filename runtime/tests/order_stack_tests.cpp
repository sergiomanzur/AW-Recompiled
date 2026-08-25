#include "aw/order_stack.hpp"

#include <cstdio>
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

struct FakeStore {
  std::vector<int> restored;
  int next_value = 0;
  int fail_after = -1;
  bool fail_restore = false;
  int captures = 0;
  int live_handles = 0;
};

void* fake_capture(void* user) {
  auto* store = static_cast<FakeStore*>(user);
  if (store->fail_after >= 0 && store->captures >= store->fail_after) return nullptr;
  ++store->captures;
  ++store->live_handles;
  return new int(store->next_value++);
}

bool fake_restore(void* user, void* snapshot) {
  auto* store = static_cast<FakeStore*>(user);
  if (store->fail_restore) return false;
  store->restored.push_back(*static_cast<int*>(snapshot));
  return true;
}

void fake_release(void* user, void* snapshot) {
  --static_cast<FakeStore*>(user)->live_handles;
  delete static_cast<int*>(snapshot);
}

aw::RewindIo make_io(FakeStore& store) {
  aw::RewindIo io;
  io.capture = fake_capture;
  io.restore = fake_restore;
  io.release = fake_release;
  io.user = &store;
  return io;
}

void test_push_pop_lifo() {
  FakeStore store;
  aw::OrderStack stack(4);
  stack.set_io(make_io(store));

  require(stack.push(), "push 1");
  require(stack.push(), "push 2");
  require(stack.push(), "push 3");
  require(stack.size() == 3, "three snapshots");

  require(stack.pop(), "pop newest");
  require(store.restored.back() == 2, "restores newest first");
  require(stack.pop(), "pop middle");
  require(store.restored.back() == 1, "restores in LIFO order");
  require(stack.size() == 1, "one left");
}

void test_capacity_drops_oldest() {
  FakeStore store;
  aw::OrderStack stack(2);
  stack.set_io(make_io(store));

  stack.push();
  stack.push();
  stack.push();  // Drops the first snapshot.
  require(stack.size() == 2, "size clamps at capacity");
  require(store.live_handles == 2, "oldest released");

  stack.pop();
  require(store.restored.back() == 2, "newest of the three survives");
  stack.pop();
  require(store.restored.back() == 1, "second-newest survives");
  require(!stack.pop(), "empty pop fails");
}

void test_reset_releases_all() {
  FakeStore store;
  aw::OrderStack stack(4);
  stack.set_io(make_io(store));

  stack.push();
  stack.push();
  stack.reset();
  require(stack.empty(), "reset empties");
  require(store.live_handles == 0, "reset frees handles");
  require(stack.push(), "usable after reset");
}

void test_failures_disable() {
  FakeStore store;
  store.fail_after = 1;  // First capture succeeds, the rest fail.
  aw::OrderStack stack(4);
  stack.set_io(make_io(store));

  require(stack.push(), "first push works");
  require(!stack.push(), "second push fails");
  require(!stack.push(), "third push fails");
  require(!stack.push(), "fourth push fails and disables");
  require(stack.disabled(), "disabled after consecutive failures");
  require(!stack.pop(), "disabled stack will not restore");
}

void test_restore_failure_disables() {
  FakeStore store;
  aw::OrderStack stack(4);
  stack.set_io(make_io(store));

  stack.push();
  stack.push();
  store.fail_restore = true;
  require(!stack.pop(), "restore failure reported");
  require(stack.disabled(), "restore failure disables");
  require(store.live_handles == 1, "failed snapshot still released");
}

}  // namespace

int main() {
  test_push_pop_lifo();
  test_capacity_drops_oldest();
  test_reset_releases_all();
  test_failures_disable();
  test_restore_failure_disables();

  if (failures == 0) {
    std::printf("order_stack_tests: all tests passed\n");
    return 0;
  }
  std::fprintf(stderr, "order_stack_tests: %d failure(s)\n", failures);
  return 1;
}
