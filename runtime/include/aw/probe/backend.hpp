#pragma once

#include <cstddef>
#include <cstdint>

namespace aw {

// Read-only access to the running game's memory. mGBA implements this today;
// the static recompiler can implement it later without the probe, tracker or
// navigation layers noticing.
//
// Everything here is read-only by design: the navigation loop never writes
// game state, so the game's own cursor logic, camera and sound cannot desync.
class ProbeBackend {
public:
  virtual ~ProbeBackend() = default;

  // False when the backend cannot supply memory access at all. Callers must
  // disable themselves rather than fabricating data.
  virtual bool available() = 0;

  // 1 KB of OBJ attribute memory, or nullptr.
  virtual const std::uint8_t* oam() = 0;

  // 256 KB of external work RAM, or nullptr. `size_out` receives the real size.
  virtual const std::uint8_t* ewram(std::size_t& size_out) = 0;

  // 32 KB of internal work RAM, or nullptr. `size_out` receives the real
  // size. Defaults to unavailable (nullptr, size 0) so existing backends
  // that predate this accessor need not be updated; MgbaProbeBackend
  // overrides it. Offline tooling (e.g. the cursor-coordinate miner) uses
  // this alongside ewram() since some games keep hot state like a cursor
  // position in IWRAM rather than EWRAM.
  virtual const std::uint8_t* iwram(std::size_t& size_out) { size_out = 0; return nullptr; }

  // A 16-bit memory-mapped IO register by absolute address, e.g. 0x04000010
  // for REG_BG0HOFS. Returns 0 when unavailable.
  virtual std::uint16_t read_io16(std::uint32_t addr) = 0;

  // Direct memory write access for RTS cursor teleportation and game steering
  virtual void write8(std::uint32_t addr, std::uint8_t val) {}
  virtual void write16(std::uint32_t addr, std::uint16_t val) {}
};

// GBA BG scroll registers. These are hardware, so camera offset needs no
// reverse engineering.
constexpr std::uint32_t kRegBg0Hofs = 0x04000010;
constexpr std::uint32_t kRegBg0Vofs = 0x04000012;

// Byte offset from kRegBg0Hofs to layer `bg`'s horizontal scroll register.
constexpr std::uint32_t bg_hofs_reg(int bg) {
  return kRegBg0Hofs + static_cast<std::uint32_t>(bg) * 4u;
}
constexpr std::uint32_t bg_vofs_reg(int bg) {
  return kRegBg0Vofs + static_cast<std::uint32_t>(bg) * 4u;
}

}  // namespace aw
