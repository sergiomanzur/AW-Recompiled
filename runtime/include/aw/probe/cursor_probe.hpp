#pragma once

#include "aw/probe/backend.hpp"

#include <cstdint>

namespace aw {

// Where a ROM revision keeps the map cursor's tile coordinates. Mined
// offline by aw-cursor-miner (see runtime/tools/cursor_miner.cpp) from a
// real gameplay savestate, then recorded in data/symbols/<sha1>.ini.
struct CursorAddresses {
  std::uint32_t x_addr = 0;  // Absolute GBA address, 0 = unknown
  std::uint32_t y_addr = 0;

  bool valid() const { return x_addr != 0 && y_addr != 0; }
};

struct CursorTile {
  bool found = false;
  int x = 0;
  int y = 0;
};

// Reads the cursor's tile coordinates directly out of game RAM. This
// replaces guessing which OAM sprite is the cursor (OamTracker) with an
// exact read, so steering becomes tile arithmetic instead of a pixel-space
// control loop.
//
// Returns found=false -- never garbage -- when the addresses are unset, or
// when an address does not resolve into the IWRAM/EWRAM block the backend
// currently exposes (block unavailable, or the offset is beyond the block's
// real size).
CursorTile read_cursor_tile(ProbeBackend& backend, const CursorAddresses& addrs);

}  // namespace aw
