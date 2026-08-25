#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace aw {

// Deterministic replay: the exact keys_pressed stream fed to a core that was
// reset to frame 0, plus the ROM's SHA-1 so mismatches are caught before a
// desync instead of after. The file contains no ROM-derived data, so replays
// are safe to share.
//
// Layout (all integers little-endian):
//   0   8  "AWREPLAY"
//   8   4  version (= 1)
//   12 44  rom_sha1 (41 chars + 3 zero pad)
//   56  4  frame_count
//   60  4  reserved (= 0)
//   64 ..  frame_count u16 key masks
class ReplayRecorder {
public:
  // Starts a new recording: truncates `path`, writes the header. Recording
  // must begin from a freshly reset core for the replay to be deterministic.
  bool start(const std::string& path, const std::string& rom_sha1);
  void record(std::uint16_t keys);
  bool stop();  // Flushes and rewrites the frame count; safe to call twice.
  bool active() const { return out_.is_open(); }
  std::uint32_t frames() const { return frames_; }
  const std::string& path() const { return path_; }

private:
  std::ofstream out_;
  std::string path_;
  std::uint32_t frames_ = 0;
};

class ReplayPlayer {
public:
  struct Info {
    std::string rom_sha1;
    std::uint32_t frame_count = 0;
  };

  // Loads and validates a replay. `err` explains magic/version/shape
  // problems; a SHA-1 mismatch against `expected_sha1` is NOT a load error -
  // callers decide via rom_matches() whether to play it.
  bool load(const std::string& path, std::string& err);
  bool rom_matches(const std::string& expected_sha1) const;

  // Returns false at the end of the replay (and leaves `keys` untouched).
  bool next(std::uint16_t& keys);
  void rewind_to_start();

  bool loaded() const { return loaded_; }
  bool finished() const { return index_ >= info_.frame_count; }
  std::uint32_t frame_index() const { return index_; }
  const Info& info() const { return info_; }

private:
  bool loaded_ = false;
  Info info_{};
  std::vector<std::uint16_t> keys_;
  std::uint32_t index_ = 0;
};

}  // namespace aw
