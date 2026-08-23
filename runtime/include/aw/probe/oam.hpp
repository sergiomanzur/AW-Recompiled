#pragma once

#include <cstddef>
#include <cstdint>

namespace aw {

constexpr std::size_t kOamEntryCount = 128;
constexpr std::size_t kOamBytes = kOamEntryCount * 8;

// One decoded OBJ attribute entry. GBA OAM stores sprite position in *screen*
// coordinates, which is why tracking a sprite gives the selection indicator's
// position in the same space the mouse reports.
struct OamEntry {
  int x = 0;             // Screen X, 9-bit signed (-256..255)
  int y = 0;             // Screen Y, 0..255 as stored
  int tile = 0;          // Character name, attr2 bits 0-9
  int palette = 0;       // Palette bank, attr2 bits 12-15
  int priority = 0;      // attr2 bits 10-11
  bool visible = false;  // Not in the disabled object mode

  bool on_screen() const {
    return visible && y < 160 && x > -64 && x < 240;
  }
};

// Decodes entry `index` from a 1 KB OAM block. Returns a default-constructed
// (invisible) entry for a null buffer or an out-of-range index.
OamEntry decode_oam_entry(const std::uint8_t* oam, std::size_t index);

}  // namespace aw
