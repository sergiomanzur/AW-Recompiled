#include "aw/render/hd_text.hpp"

#include "aw/config_file.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace aw {

namespace {

// FNV-1a over the 8 mask bytes: stable across sessions and platforms.
std::uint64_t fnv1a(const std::uint8_t* data, std::size_t n) {
  std::uint64_t h = 1469598103934665603ull;
  for (std::size_t i = 0; i < n; ++i) {
    h ^= data[i];
    h *= 1099511628211ull;
  }
  return h;
}

// Reads a 24-bit top-down BMP of exactly 16x16 into an ink mask: dark
// pixels (below half luminance) are ink, the rest transparent. Returns
// false on any shape mismatch.
bool read_ink_bmp(const std::string& path, HdTextPack::Entry& out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  std::uint8_t header[54];
  if (std::fread(header, 1, 54, f) != 54) {
    std::fclose(f);
    return false;
  }
  const int width = header[18] | (header[19] << 8);
  const int height = header[22] | (header[23] << 8);
  const int bpp = header[28] | (header[29] << 8);
  const std::uint32_t offset = header[10] | (header[11] << 8) | (header[12] << 16) |
                               (static_cast<std::uint32_t>(header[13]) << 24);
  if (width != 16 || height != 16 || bpp != 24) {
    std::fclose(f);
    return false;
  }
  const int row = 16 * 3 + ((16 * 3) % 4 == 0 ? 0 : 4 - (16 * 3) % 4);
  std::uint8_t pixels[16 * row];
  if (std::fread(pixels, 1, sizeof(pixels), f) != sizeof(pixels)) {
    std::fclose(f);
    return false;
  }
  std::fclose(f);

  for (int y = 0; y < 16; ++y) {
    // BMPs are bottom-up unless the height is negative; the capture tool
    // and this reader agree on bottom-up.
    const int src_y = 15 - y;
    for (int x = 0; x < 16; ++x) {
      const std::uint8_t b = pixels[src_y * row + x * 3 + 0];
      const std::uint8_t g = pixels[src_y * row + x * 3 + 1];
      const std::uint8_t r = pixels[src_y * row + x * 3 + 2];
      const int lum = (r * 30 + g * 59 + b * 11) / 100;
      out.ink[y][x] = lum < 128 ? 255 : 0;
    }
  }
  return true;
}

}  // namespace

std::uint64_t HdTextPack::block_hash(const std::uint32_t* block) {
  // The dominant colour of a text block is its background; everything
  // sufficiently far from it is ink. Cheap: 64 pixels, no sorting.
  std::uint32_t counts[64] = {};
  std::uint32_t colors[64] = {};
  int distinct = 0;
  for (int i = 0; i < 64; ++i) {
    int found = -1;
    for (int j = 0; j < distinct; ++j) {
      if (colors[j] == block[i]) {
        found = j;
        break;
      }
    }
    if (found < 0 && distinct < 64) {
      colors[distinct] = block[i];
      counts[distinct] = 1;
      ++distinct;
    } else if (found >= 0) {
      ++counts[found];
    }
  }
  int bg_index = 0;
  for (int i = 1; i < distinct; ++i) {
    if (counts[i] > counts[bg_index]) bg_index = i;
  }
  const std::uint32_t bg = colors[bg_index];

  std::uint8_t mask[8] = {};
  for (int i = 0; i < 64; ++i) {
    // Per-channel distance from the background; text ink differs clearly.
    const std::uint32_t c = block[i];
    const int dr = static_cast<int>((c >> 16) & 0xFF) - static_cast<int>((bg >> 16) & 0xFF);
    const int dg = static_cast<int>((c >> 8) & 0xFF) - static_cast<int>((bg >> 8) & 0xFF);
    const int db = static_cast<int>(c & 0xFF) - static_cast<int>(bg & 0xFF);
    if (dr * dr + dg * dg + db * db > 3 * 48 * 48) {
      mask[i / 8] |= static_cast<std::uint8_t>(1u << (i % 8));
    }
  }
  return fnv1a(mask, 8);
}

bool HdTextPack::load(const std::string& ini_path) {
  loaded_ = false;
  entries_.clear();

  ConfigFile config;
  if (!config.load(ini_path)) {
    return false;  // Missing pack file: engine stays dormant, not an error.
  }

  const std::filesystem::path base = std::filesystem::path(ini_path).parent_path();
  int good = 0;
  for (int slot = 1; slot <= 512; ++slot) {
    const std::string value = config.get_string("Pack", "entry" + std::to_string(slot), "");
    if (value.empty()) continue;
    const std::size_t comma = value.find(',');
    if (comma == std::string::npos) continue;

    char* end = nullptr;
    const unsigned long long hash = std::strtoull(value.substr(0, comma).c_str(), &end, 10);
    if (end == nullptr || *end != '\0') continue;

    Entry entry{};
    if (!read_ink_bmp((base / value.substr(comma + 1)).string(), entry)) {
      std::fprintf(stderr, "HD text: skipping unreadable tile %s\n", value.substr(comma + 1).c_str());
      continue;
    }
    entries_[hash] = entry;
    ++good;
  }

  loaded_ = true;
  std::printf("HD text pack: %d tile(s) loaded\n", good);
  return true;
}

const HdTextPack::Entry* HdTextPack::find(std::uint64_t hash) const {
  const auto it = entries_.find(hash);
  return it == entries_.end() ? nullptr : &it->second;
}

int apply_hd_text(const std::uint32_t* src, int src_w, int src_h,
                  std::uint32_t* dst, int dst_w, const HdTextPack& pack) {
  if (src == nullptr || dst == nullptr || !pack.loaded()) return 0;

  int replaced = 0;
  for (int by = 0; by + 8 <= src_h; by += 8) {
    for (int bx = 0; bx + 8 <= src_w; bx += 8) {
      std::uint32_t block[64];
      for (int y = 0; y < 8; ++y) {
        std::memcpy(&block[y * 8], &src[(by + y) * src_w + bx], 8 * sizeof(std::uint32_t));
      }
      const HdTextPack::Entry* entry = pack.find(HdTextPack::block_hash(block));
      if (entry == nullptr) continue;

      // Ink colour: the block pixel furthest from its background; the
      // replacement keeps the game's palette while contributing the shape.
      std::uint32_t ink = 0xFF000000u;
      int best_dist = -1;
      {
        // Recompute the dominant colour cheaply: reuse the first pixel as a
        // fallback baseline; ink candidates are pixels that differ.
        std::uint32_t counts[16] = {};
        std::uint32_t colors[16] = {};
        int distinct = 0;
        for (int i = 0; i < 64 && distinct < 16; ++i) {
          bool found = false;
          for (int j = 0; j < distinct; ++j) {
            if (colors[j] == block[i]) {
              ++counts[j];
              found = true;
              break;
            }
          }
          if (!found) {
            colors[distinct] = block[i];
            counts[distinct] = 1;
            ++distinct;
          }
        }
        int bg_i = 0;
        for (int i = 1; i < distinct; ++i) {
          if (counts[i] > counts[bg_i]) bg_i = i;
        }
        const std::uint32_t bg = colors[bg_i];
        for (int i = 0; i < distinct; ++i) {
          if (i == bg_i) continue;
          const int dr = static_cast<int>((colors[i] >> 16) & 0xFF) - static_cast<int>((bg >> 16) & 0xFF);
          const int dg = static_cast<int>((colors[i] >> 8) & 0xFF) - static_cast<int>((bg >> 8) & 0xFF);
          const int db = static_cast<int>(colors[i] & 0xFF) - static_cast<int>(bg & 0xFF);
          const int dist = dr * dr + dg * dg + db * db;
          if (dist > best_dist) {
            best_dist = dist;
            ink = colors[i];
          }
        }
      }

      // Blit the 16x16 replacement, tinted with the sampled ink colour.
      for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
          const int px = (bx * 2) + x;
          const int py = (by * 2) + y;
          if (px >= dst_w) continue;
          const std::uint8_t alpha = entry->ink[y][x];
          if (alpha == 0) continue;  // Keep the upscaled background beneath.
          dst[py * dst_w + px] = ink | 0xFF000000u;
        }
      }
      ++replaced;
    }
  }
  return replaced;
}

}  // namespace aw
