#pragma once

#include <cstdint>
#include <vector>

namespace aw {

// How the rewind ring talks to whatever owns savestates (the mGBA adapter in
// the runtime; fakes in tests). Snapshots are opaque handles.
struct RewindIo {
  // Capture the current state. Returns nullptr on failure.
  void* (*capture)(void* user) = nullptr;
  // Restore a previously captured snapshot. Returns false on failure.
  bool (*restore)(void* user, void* snapshot) = nullptr;
  // Destroy a snapshot handle.
  void (*release)(void* user, void* snapshot) = nullptr;
  // Optional: report a snapshot's size in bytes (for logging/budgeting).
  std::uint64_t (*size)(void* user, void* snapshot) = nullptr;
  void* user = nullptr;

  bool valid() const { return capture != nullptr && restore != nullptr && release != nullptr; }
};

// Fixed-capacity ring of periodic savestates powering instant time travel.
// Call on_frame() once per emulated frame after running it; snapshots are
// captured every `snapshot_interval` frames. rewind_step() restores the most
// recent snapshot and pops it, so repeated steps walk further back in time.
// Snapshots older than `max_history_frames` are evicted as the game runs, so
// the reachable past never stretches beyond that window.
class RewindBuffer {
public:
  // ~5 seconds of GBA time (300 frames at ~59.7 fps).
  static constexpr int kDefaultMaxHistoryFrames = 300;
  static constexpr int kDefaultCapacity = 240;
  static constexpr int kDefaultInterval = 20;
  // Stop trying (and log once) after this many consecutive capture failures.
  static constexpr int kMaxConsecutiveFailures = 3;

  explicit RewindBuffer(int capacity = kDefaultCapacity,
                        int snapshot_interval = kDefaultInterval,
                        int max_history_frames = kDefaultMaxHistoryFrames);

  void set_io(const RewindIo& io) { io_ = io; }

  // Drop all history (ROM switch). Keeps capacity and interval settings.
  void reset();

  // Advance the frame counter, capturing a snapshot when the interval elapses
  // and evicting any snapshot that aged out of the history window.
  void on_frame();

  // Restore the newest snapshot and remove it from history. Returns false
  // when history is empty, restore fails, or the buffer was disabled.
  bool rewind_step();

  bool empty() const { return count_ == 0; }
  int size() const { return count_; }
  int capacity() const { return capacity_; }
  int snapshot_interval() const { return snapshot_interval_; }
  int max_history_frames() const { return max_history_frames_; }
  std::uint64_t total_bytes_held() const { return bytes_held_; }
  bool disabled() const { return disabled_; }

private:
  bool capture_snapshot();
  void evict_expired();

  RewindIo io_;
  std::vector<void*> slots_;       // ring; slots_[head_] is the next write
  std::vector<std::uint64_t> slot_frames_;  // frame index of each capture
  int head_ = 0;
  int count_ = 0;
  int capacity_;
  int snapshot_interval_;
  int max_history_frames_;
  std::uint64_t frames_elapsed_ = 0;
  int frames_since_snapshot_ = 0;
  int consecutive_failures_ = 0;
  bool disabled_ = false;
  bool size_logged_ = false;
  std::uint64_t bytes_held_ = 0;
};

}  // namespace aw
