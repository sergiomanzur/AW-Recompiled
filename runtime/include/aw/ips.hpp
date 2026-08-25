#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aw {

// Applies a classic IPS patch to a ROM image in memory. IPS is the format
// community randomizers ship: "PATCH", then records of (3-byte big-endian
// offset, 2-byte big-endian length, payload; length 0 means run-length
// encode: 2-byte count + 1 fill byte), terminated by "EOF".
//
// This is the randomizer hook: any AW randomizer patch becomes
// "File > Apply IPS Patch..." and a reboot - no external tooling, no
// patched-rom copies to manage (the patched image exists only as a temp
// file while playing).
//
// Returns true on success. `err` explains a malformed patch; a record that
// writes past the ROM's end is an error rather than a silent truncation,
// because a mismatched patch usually means wrong ROM revision.
bool apply_ips(std::vector<std::uint8_t>& rom, const std::vector<std::uint8_t>& patch,
               std::string& err);

}  // namespace aw
