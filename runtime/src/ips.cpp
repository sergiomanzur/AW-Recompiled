#include "aw/ips.hpp"

#include <cstdio>

namespace aw {

bool apply_ips(std::vector<std::uint8_t>& rom, const std::vector<std::uint8_t>& patch,
               std::string& err) {
  std::size_t p = 0;
  const std::size_t n = patch.size();

  auto need = [&](std::size_t bytes) -> bool {
    if (p + bytes > n) {
      err = "patch truncated mid-record";
      return false;
    }
    return true;
  };

  if (!need(5) || std::string(reinterpret_cast<const char*>(patch.data()), 5) != "PATCH") {
    err = "not an IPS patch (missing PATCH header)";
    return false;
  }
  p = 5;

  int records = 0;
  while (p < n) {
    if (n - p >= 3 && patch[p] == 'E' && patch[p + 1] == 'O' && patch[p + 2] == 'F') {
      break;  // Normal termination.
    }
    if (!need(5)) return false;
    const std::uint32_t offset = (static_cast<std::uint32_t>(patch[p]) << 16) |
                                 (static_cast<std::uint32_t>(patch[p + 1]) << 8) |
                                 patch[p + 2];
    const std::uint32_t length = (static_cast<std::uint32_t>(patch[p + 3]) << 8) | patch[p + 4];
    p += 5;

    if (length == 0) {
      // RLE record: 2-byte repeat count + 1 fill byte.
      if (!need(3)) return false;
      const std::uint32_t count = (static_cast<std::uint32_t>(patch[p]) << 8) | patch[p + 1];
      const std::uint8_t fill = patch[p + 2];
      p += 3;
      if (static_cast<std::uint64_t>(offset) + count > rom.size()) {
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer),
                      "RLE record at 0x%06X runs past the end of the ROM (wrong ROM revision?)",
                      offset);
        err = buffer;
        return false;
      }
      std::fill(rom.begin() + offset, rom.begin() + offset + count, fill);
    } else {
      if (!need(length)) return false;
      if (static_cast<std::uint64_t>(offset) + length > rom.size()) {
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer),
                      "record at 0x%06X length %u runs past the end of the ROM (wrong ROM revision?)",
                      offset, length);
        err = buffer;
        return false;
      }
      std::copy(patch.begin() + p, patch.begin() + p + length, rom.begin() + offset);
      p += length;
    }
    ++records;
  }

  if (records == 0) {
    err = "patch contains no records";
    return false;
  }
  return true;
}

}  // namespace aw
