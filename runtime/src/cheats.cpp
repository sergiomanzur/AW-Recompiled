#include "aw/cheats.hpp"

#include "aw/config_file.hpp"

#include <cstdio>
#include <cstdlib>

namespace aw {

namespace {

// Decimal by default; 0x prefix switches to hex (config files quote
// addresses in both).
std::uint32_t parse_number(const std::string& text, bool& ok) {
  ok = false;
  if (text.empty()) return 0;
  const char* c = text.c_str();
  while (*c == ' ') ++c;
  int base = 10;
  if (c[0] == '0' && (c[1] == 'x' || c[1] == 'X')) {
    base = 16;
    c += 2;
  }
  if (*c == '\0') return 0;
  char* end = nullptr;
  const unsigned long v = std::strtoul(c, &end, base);
  if (end == nullptr || *end != '\0') return 0;
  ok = true;
  return static_cast<std::uint32_t>(v);
}

}  // namespace

bool CheatEngine::parse_code(const std::string& spec, CheatCode& out) {
  const std::size_t first = spec.find(':');
  if (first == std::string::npos) return false;
  const std::size_t second = spec.find(':', first + 1);
  if (second == std::string::npos) return false;

  bool ok = false;
  out.address = parse_number(spec.substr(0, first), ok);
  if (!ok) return false;
  out.width = static_cast<int>(parse_number(spec.substr(first + 1, second - first - 1), ok));
  if (!ok || (out.width != 1 && out.width != 2 && out.width != 4)) return false;
  out.value = parse_number(spec.substr(second + 1), ok);
  return ok;
}

void CheatEngine::load(const ConfigFile& config) {
  enabled_ = config.get_int("Cheats", "enabled", 0) != 0;
  codes_.clear();
  warned_bad_code_ = false;

  for (int slot = 1; slot <= 64; ++slot) {
    const std::string spec = config.get_string("Cheats", "code" + std::to_string(slot), "");
    if (spec.empty()) continue;
    CheatCode code;
    if (parse_code(spec, code)) {
      codes_.push_back(code);
    } else if (!warned_bad_code_) {
      warned_bad_code_ = true;
      std::fprintf(stderr, "Cheats: skipping malformed code '%s' (want address:width:value)\n",
                   spec.c_str());
    }
  }
}

}  // namespace aw
