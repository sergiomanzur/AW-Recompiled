#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aw {

class ConfigFile;

// RAM-write cheat codes: address + width + value, applied every frame
// while enabled so the game's own writes cannot fight them off. Codes are
// config-driven ([Cheats] section) so a mined address becomes a cheat
// without a rebuild:
//
//   [Cheats]
//   enabled = 1
//   code1 = 50345636:1:7     ; address:width(1|2|4):value, decimal
//   code2 = ...
//
// Address/value also accept 0x-prefixed hex. Bad lines are skipped with a
// one-time warning, never fatal.
struct CheatCode {
  std::uint32_t address = 0;
  int width = 1;  // bytes: 1, 2 or 4
  std::uint32_t value = 0;
};

class CheatEngine {
public:
  void load(const ConfigFile& config);

  bool enabled() const { return enabled_; }
  void set_enabled(bool enabled) { enabled_ = enabled; }
  void toggle() { enabled_ = !enabled_; }

  const std::vector<CheatCode>& codes() const { return codes_; }
  int active_count() const { return enabled_ ? static_cast<int>(codes_.size()) : 0; }

  // Parses "address:width:value"; returns false on malformed input.
  static bool parse_code(const std::string& spec, CheatCode& out);

private:
  bool enabled_ = false;
  std::vector<CheatCode> codes_;
  bool warned_bad_code_ = false;
};

}  // namespace aw
