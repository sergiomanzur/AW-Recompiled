#include "aw/probe/cursor_probe.hpp"

namespace aw {

namespace {

constexpr std::uint32_t kEwramBase = 0x02000000;
constexpr std::uint32_t kIwramBase = 0x03000000;
constexpr std::uint32_t kIwramEnd = 0x04000000;  // One past the last IWRAM address

// Resolves `addr` to a byte in whichever block the backend exposes for that
// address range, bounds-checking the offset against the block's real size.
// A null block (backend has none mapped) or an out-of-range offset both
// count as "not found" -- neither fabricates a value.
bool read_byte(ProbeBackend& backend, std::uint32_t addr, std::uint8_t& out) {
  const std::uint8_t* block = nullptr;
  std::size_t size = 0;
  std::uint32_t offset = 0;

  if (addr >= kIwramBase && addr < kIwramEnd) {
    block = backend.iwram(size);
    offset = addr - kIwramBase;
  } else if (addr >= kEwramBase && addr < kIwramBase) {
    block = backend.ewram(size);
    offset = addr - kEwramBase;
  } else {
    return false;  // Neither mapped block covers this address.
  }

  if (block == nullptr) return false;
  if (offset >= size) return false;

  out = block[offset];
  return true;
}

}  // namespace

CursorTile read_cursor_tile(ProbeBackend& backend, const CursorAddresses& addrs) {
  CursorTile tile;
  if (!addrs.valid()) return tile;

  std::uint8_t x = 0;
  std::uint8_t y = 0;
  if (!read_byte(backend, addrs.x_addr, x)) return tile;
  if (!read_byte(backend, addrs.y_addr, y)) return tile;

  tile.found = true;
  tile.x = x;
  tile.y = y;
  return tile;
}

}  // namespace aw
