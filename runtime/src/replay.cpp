#include "aw/replay.hpp"

#include <cstring>

namespace aw {

namespace {

constexpr char kMagic[8] = {'A', 'W', 'R', 'E', 'P', 'L', 'A', 'Y'};
constexpr std::uint32_t kVersion = 1;
constexpr std::size_t kHeaderSize = 64;

void write_u16(std::ofstream& out, std::uint16_t v) {
  const char bytes[2] = {static_cast<char>(v & 0xFF), static_cast<char>(v >> 8)};
  out.write(bytes, 2);
}

void write_u32(std::ofstream& out, std::uint32_t v) {
  const char bytes[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                         static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
  out.write(bytes, 4);
}

std::uint16_t read_u16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t read_u32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

bool ReplayRecorder::start(const std::string& path, const std::string& rom_sha1) {
  stop();
  path_ = path;
  frames_ = 0;
  out_.open(path, std::ios::binary | std::ios::trunc);
  if (!out_.is_open()) return false;

  out_.write(kMagic, 8);
  write_u32(out_, kVersion);
  char sha[44] = {};
  std::memcpy(sha, rom_sha1.c_str(), rom_sha1.size() < 41 ? rom_sha1.size() : 40);
  out_.write(sha, 44);
  write_u32(out_, 0);  // Frame count patched by stop().
  write_u32(out_, 0);  // Reserved.
  return true;
}

void ReplayRecorder::record(std::uint16_t keys) {
  if (!out_.is_open()) return;
  write_u16(out_, keys);
  ++frames_;
}

bool ReplayRecorder::stop() {
  if (!out_.is_open()) return true;
  out_.flush();
  out_.seekp(56);
  write_u32(out_, frames_);
  out_.close();
  return true;
}

bool ReplayPlayer::load(const std::string& path, std::string& err) {
  loaded_ = false;
  keys_.clear();
  index_ = 0;
  info_ = Info{};

  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    err = "cannot open replay file";
    return false;
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
  if (bytes.size() < kHeaderSize) {
    err = "file too small to be a replay";
    return false;
  }
  if (std::memcmp(bytes.data(), kMagic, 8) != 0) {
    err = "bad magic (not an AW replay)";
    return false;
  }
  if (read_u32(bytes.data() + 8) != kVersion) {
    err = "unsupported replay version";
    return false;
  }

  char sha[45] = {};
  std::memcpy(sha, bytes.data() + 12, 44);
  sha[44] = '\0';
  info_.rom_sha1 = sha;
  info_.frame_count = read_u32(bytes.data() + 56);

  const std::size_t body = bytes.size() - kHeaderSize;
  if (body < static_cast<std::size_t>(info_.frame_count) * 2) {
    err = "replay truncated: header promises more frames than the file holds";
    return false;
  }

  keys_.resize(info_.frame_count);
  for (std::uint32_t i = 0; i < info_.frame_count; ++i) {
    keys_[i] = read_u16(bytes.data() + kHeaderSize + i * 2);
  }
  loaded_ = true;
  return true;
}

bool ReplayPlayer::rom_matches(const std::string& expected_sha1) const {
  return loaded_ && info_.rom_sha1 == expected_sha1;
}

bool ReplayPlayer::next(std::uint16_t& keys) {
  if (!loaded_ || index_ >= info_.frame_count) return false;
  keys = keys_[index_];
  ++index_;
  return true;
}

void ReplayPlayer::rewind_to_start() {
  index_ = 0;
}

}  // namespace aw
