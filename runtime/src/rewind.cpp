#include "aw/rewind.hpp"

#include <cstdio>

namespace aw {

RewindBuffer::RewindBuffer(int capacity, int snapshot_interval)
    : capacity_(capacity < 1 ? 1 : capacity),
      snapshot_interval_(snapshot_interval < 1 ? 1 : snapshot_interval) {
  slots_.resize(static_cast<std::size_t>(capacity_), nullptr);
}

void RewindBuffer::reset() {
  if (io_.valid()) {
    for (int i = 0; i < count_; ++i) {
      const int index = (head_ - 1 - i + capacity_ * 2) % capacity_;
      if (slots_[static_cast<std::size_t>(index)] != nullptr) {
        io_.release(io_.user, slots_[static_cast<std::size_t>(index)]);
        slots_[static_cast<std::size_t>(index)] = nullptr;
      }
    }
  }
  head_ = 0;
  count_ = 0;
  frames_since_snapshot_ = 0;
  consecutive_failures_ = 0;
  disabled_ = false;
  size_logged_ = false;
  bytes_held_ = 0;
}

bool RewindBuffer::capture_snapshot() {
  if (disabled_ || io_.capture == nullptr) {
    return false;
  }

  void* snapshot = io_.capture(io_.user);
  if (snapshot == nullptr) {
    ++consecutive_failures_;
    if (consecutive_failures_ >= kMaxConsecutiveFailures) {
      disabled_ = true;
      std::fprintf(stderr,
                   "Rewind: snapshot capture failed %d times; time travel disabled for this session\n",
                   consecutive_failures_);
    }
    return false;
  }
  consecutive_failures_ = 0;

  // The ring slot we are about to overwrite owns the oldest snapshot.
  const std::size_t slot = static_cast<std::size_t>(head_);
  if (slots_[slot] != nullptr) {
    if (io_.size != nullptr) {
      bytes_held_ -= io_.size(io_.user, slots_[slot]);
    }
    io_.release(io_.user, slots_[slot]);
  } else {
    ++count_;
  }
  slots_[slot] = snapshot;
  head_ = (head_ + 1) % capacity_;

  if (io_.size != nullptr) {
    bytes_held_ += io_.size(io_.user, snapshot);
    if (!size_logged_) {
      size_logged_ = true;
      std::printf("Rewind: history armed (%d snapshots x %.1f KB max, interval %d frames)\n",
                  capacity_,
                  static_cast<double>(io_.size(io_.user, snapshot)) / 1024.0,
                  snapshot_interval_);
    }
  }
  return true;
}

void RewindBuffer::on_frame() {
  if (disabled_) {
    return;
  }
  ++frames_since_snapshot_;
  if (frames_since_snapshot_ >= snapshot_interval_) {
    frames_since_snapshot_ = 0;
    capture_snapshot();
  }
}

bool RewindBuffer::rewind_step() {
  if (disabled_ || count_ == 0 || io_.restore == nullptr) {
    return false;
  }

  head_ = (head_ - 1 + capacity_) % capacity_;
  void* snapshot = slots_[static_cast<std::size_t>(head_)];
  if (snapshot == nullptr) {
    return false;
  }

  const bool restored = io_.restore(io_.user, snapshot);
  if (io_.size != nullptr) {
    bytes_held_ -= io_.size(io_.user, snapshot);
  }
  io_.release(io_.user, snapshot);
  slots_[static_cast<std::size_t>(head_)] = nullptr;
  --count_;

  // After restoring, the next capture should happen promptly so the history
  // rebuilds from the rewound-to moment.
  frames_since_snapshot_ = snapshot_interval_ - 1;

  if (!restored) {
    disabled_ = true;
    std::fprintf(stderr, "Rewind: snapshot restore failed; time travel disabled for this session\n");
  }
  return restored;
}

}  // namespace aw
