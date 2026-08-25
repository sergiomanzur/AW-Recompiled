#pragma once

#include "aw/rewind.hpp"

#include <vector>

namespace aw {

// A push/pop stack of in-RAM savestates powering semantic "undo last order".
// The caller pushes at the moment the player confirms an order on the map
// (an A-button edge while the map cursor is live) and pops to restore the
// game to just before that press. Unlike RewindBuffer this is LIFO with no
// interval logic: pushes are events, not a schedule.
class OrderStack {
public:
  static constexpr int kDefaultCapacity = 10;
  static constexpr int kMaxConsecutiveFailures = 3;

  explicit OrderStack(int capacity = kDefaultCapacity);

  void set_io(const RewindIo& io) { io_ = io; }

  // Drop all snapshots (ROM switch, savestate load).
  void reset();

  // Capture the current state as the newest undo point. Returns false when
  // capture fails. The oldest snapshot is dropped when the stack is full.
  bool push();

  // Restore the newest snapshot and remove it. Returns false when empty or
  // restore fails (restore failure also disables the stack).
  bool pop();

  bool empty() const { return count_ == 0; }
  int size() const { return count_; }
  int capacity() const { return capacity_; }
  bool disabled() const { return disabled_; }

private:
  RewindIo io_;
  std::vector<void*> slots_;
  int count_ = 0;  // slots_[0 .. count_-1] live, index 0 is the oldest
  int capacity_;
  int consecutive_failures_ = 0;
  bool disabled_ = false;
};

}  // namespace aw
