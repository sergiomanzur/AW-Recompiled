#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace aw {

// HD text/UI replacement: a texture-pack for the GBA framebuffer.
//
// The GBA font is 8x8 tiles, so the framebuffer is scanned in 8x8 blocks.
// Each block is reduced to an "ink mask" - which pixels differ from the
// block's dominant background colour - and that mask is hashed. The mask is
// what identifies a glyph, so the same letter hashes identically on a
// dialogue box, a menu and the map HUD regardless of palette.
//
// A pack maps mask hashes to 16x16 24-bit BMP replacements. At render time
// matched blocks are overwritten with their replacement in the 2x buffer,
// giving crisp text at any window size. Unmatched blocks keep the base
// scaler output (nearest/Scale2x), so a partial pack degrades gracefully.
class HdTextPack {
public:
  struct Entry {
    std::uint8_t ink[16][16];  // 0 = transparent, 255 = opaque ink
  };

  // Loads `data/hd/tiles.ini` plus the BMPs it references. Entries use
  // fixed slots so the stock ConfigFile can parse them:
  //   [Pack]
  //   entry1 = <hash>,tiles/a.bmp
  // An empty or missing file loads as an empty pack (engine dormant).
  bool load(const std::string& ini_path);
  bool loaded() const { return loaded_; }
  int size() const { return static_cast<int>(entries_.size()); }
  const Entry* find(std::uint64_t hash) const;

  // Hash of one 8x8 block of the source framebuffer (0xAABBGGRR after the
  // window copy, matching ppu.framebuffer). Public for the capture tool and
  // tests.
  static std::uint64_t block_hash(const std::uint32_t* block);

private:
  bool loaded_ = false;
  std::unordered_map<std::uint64_t, Entry> entries_;
};

// Composites pack replacements into a 480x320 (2x) buffer. `src` is the
// 240x160 framebuffer; `dst` the 2x buffer the base scaler already filled.
// Returns the number of blocks replaced.
int apply_hd_text(const std::uint32_t* src, int src_w, int src_h,
                  std::uint32_t* dst, int dst_w, const HdTextPack& pack);

}  // namespace aw
