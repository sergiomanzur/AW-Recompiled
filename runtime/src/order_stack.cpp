#include "aw/order_stack.hpp"

#include <cstdio>

namespace aw {

OrderStack::OrderStack(int capacity)
    : capacity_(capacity < 1 ? 1 : capacity) {
  slots_.resize(static_cast<std::size_t>(capacity_), nullptr);
}

void OrderStack::reset() {
  if (io_.valid()) {
    for (int i = 0; i < count_; ++i) {
      io_.release(io_.user, slots_[static_cast<std::size_t>(i)]);
      slots_[static_cast<std::size_t>(i)] = nullptr;
    }
  }
  count_ = 0;
  consecutive_failures_ = 0;
  disabled_ = false;
}

bool OrderStack::push() {
  if (disabled_ || io_.capture == nullptr) {
    return false;
  }

  void* snapshot = io_.capture(io_.user);
  if (snapshot == nullptr) {
    ++consecutive_failures_;
    if (consecutive_failures_ >= kMaxConsecutiveFailures) {
      disabled_ = true;
      std::fprintf(stderr,
                   "Undo: snapshot capture failed %d times; undo disabled for this session\n",
                   consecutive_failures_);
    }
    return false;
  }
  consecutive_failures_ = 0;

  if (count_ == capacity_) {
    io_.release(io_.user, slots_[0]);
    for (int i = 1; i < count_; ++i) {
      slots_[static_cast<std::size_t>(i - 1)] = slots_[static_cast<std::size_t>(i)];
    }
    slots_[static_cast<std::size_t>(count_ - 1)] = nullptr;
    --count_;
  }
  slots_[static_cast<std::size_t>(count_)] = snapshot;
  ++count_;
  return true;
}

bool OrderStack::pop() {
  if (disabled_ || count_ == 0 || io_.restore == nullptr) {
    return false;
  }

  void* snapshot = slots_[static_cast<std::size_t>(count_ - 1)];
  const bool restored = io_.restore(io_.user, snapshot);
  io_.release(io_.user, snapshot);
  slots_[static_cast<std::size_t>(count_ - 1)] = nullptr;
  --count_;

  if (!restored) {
    disabled_ = true;
    std::fprintf(stderr, "Undo: snapshot restore failed; undo disabled for this session\n");
  }
  return restored;
}

}  // namespace aw
