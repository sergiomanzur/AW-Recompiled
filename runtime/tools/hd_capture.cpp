// aw-hd-capture — dumps the distinct 8x8 framebuffer blocks of a running
// game so artists can build an HD text pack (see aw/render/hd_text.hpp).
//
// Boots the ROM headlessly (optionally from a savestate captured in-game
// with F5), advances N frames while optionally mashing A to reach dialogue,
// and writes:
//   data/hd/tiles/<hash>_src.bmp    - the 8x8 source block (for reference)
//   data/hd/tiles/<hash>_16.bmp     - a 16x16 nearest-upscale starter tile
//   data/hd/tiles/tiles.ini         - the pack, ready to hand-edit
//
// The starter tiles are identity upscales: enabling HD text with the fresh
// pack changes nothing visually until tiles are redrawn, which makes it safe
// to iterate - replace a tile, press F4/F8-free boot, see exactly that
// glyph crisp up.
//
// Usage: aw-hd-capture <rom> [state.ss] [--frames N] [--mash]

#include "aw/hardware.hpp"
#include "aw/mgba_adapter.h"
#include "aw/render/hd_text.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using aw::kKeyA;

namespace {

void write_bmp(const std::string& path, const std::uint32_t* pixels, int w, int h) {
  const int row = w * 3 + ((w * 3) % 4 == 0 ? 0 : 4 - (w * 3) % 4);
  const int data = row * h;
  const int size = 54 + data;
  std::vector<std::uint8_t> bmp(size, 0);
  bmp[0] = 'B'; bmp[1] = 'M';
  bmp[2] = size & 0xFF; bmp[3] = (size >> 8) & 0xFF; bmp[4] = (size >> 16) & 0xFF;
  bmp[10] = 54;
  bmp[14] = 40;
  bmp[18] = w & 0xFF; bmp[19] = (w >> 8) & 0xFF;
  bmp[22] = h & 0xFF; bmp[23] = (h >> 8) & 0xFF;
  bmp[26] = 1; bmp[28] = 24;
  bmp[34] = data & 0xFF; bmp[35] = (data >> 8) & 0xFF; bmp[36] = (data >> 16) & 0xFF;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const std::uint32_t px = pixels[static_cast<std::size_t>(y) * w + x];
      std::uint8_t* dst = &bmp[54 + (h - 1 - y) * row + x * 3];
      dst[0] = px & 0xFF;
      dst[1] = (px >> 8) & 0xFF;
      dst[2] = (px >> 16) & 0xFF;
    }
  }
  FILE* f = std::fopen(path.c_str(), "wb");
  if (f != nullptr) {
    std::fwrite(bmp.data(), 1, bmp.size(), f);
    std::fclose(f);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "Usage: aw-hd-capture <rom> [state.ss] [--frames N] [--mash]\n");
    return 1;
  }
  const std::string rom = argv[1];
  std::string state;
  int frames = 600;
  bool mash = false;
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--frames" && i + 1 < argc) frames = std::stoi(argv[++i]);
    else if (arg.rfind("--frames=", 0) == 0) frames = std::stoi(arg.substr(9));
    else if (arg == "--mash") mash = true;
    else if (!arg.empty() && arg[0] != '-') state = arg;
  }

  std::vector<std::uint32_t> video(240 * 160, 0);
  struct mCore* core = aw_mgba_create(rom.c_str(), video.data(), 240);
  if (!core) {
    std::fprintf(stderr, "core creation failed\n");
    return 1;
  }
  if (!state.empty() && !aw_mgba_load_state(core, state.c_str())) {
    std::fprintf(stderr, "load state failed: %s\n", state.c_str());
    return 1;
  }

  // Advance, optionally mashing A to walk dialogue where the font lives.
  for (int i = 0; i < frames; ++i) {
    const std::uint16_t keys = mash ? ((i / 16) % 2 == 0 ? kKeyA : 0) : 0;
    aw_mgba_run_frame(core, keys);
  }

  // The window copy swaps R/B for GDI; replicate so hashes match gameplay.
  std::vector<std::uint32_t> fb(240 * 160);
  for (std::size_t i = 0; i < fb.size(); ++i) {
    const std::uint32_t c = video[i];
    fb[i] = ((c & 0x000000FF) << 16) | (c & 0x00FF00) | ((c & 0xFF0000) >> 16) | 0xFF000000u;
  }

  // Collect distinct block hashes. Skip all-background blocks: only inked
  // blocks (glyph candidates) belong in a pack.
  std::map<std::uint64_t, std::vector<std::uint32_t>> blocks;
  for (int by = 0; by + 8 <= 160; by += 8) {
    for (int bx = 0; bx + 8 <= 240; bx += 8) {
      std::uint32_t block[64];
      for (int y = 0; y < 8; ++y) {
        std::memcpy(&block[y * 8], &fb[(by + y) * 240 + bx], 8 * sizeof(std::uint32_t));
      }
      const std::uint64_t hash = aw::HdTextPack::block_hash(block);
      bool any_ink = false;
      std::uint32_t first = block[0];
      for (int i = 0; i < 64; ++i) {
        if (block[i] != first) any_ink = true;
      }
      if (!any_ink) continue;
      if (blocks.find(hash) == blocks.end()) {
        blocks.emplace(hash, std::vector<std::uint32_t>(block, block + 64));
      }
    }
  }

  const std::filesystem::path dir = "data/hd/tiles";
  std::filesystem::create_directories(dir);
  std::string ini = "[Pack]\n";
  int written = 0;
  for (const auto& [hash, pixels] : blocks) {
    const std::string base = std::to_string(hash);
    write_bmp((dir / (base + "_src.bmp")).string(), pixels.data(), 8, 8);
    // Starter 16x16 tile: nearest 2x of the source block.
    std::vector<std::uint32_t> big(16 * 16);
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 16; ++x) {
        big[static_cast<std::size_t>(y) * 16 + x] = pixels[(y / 2) * 8 + (x / 2)];
      }
    }
    write_bmp((dir / (base + "_16.bmp")).string(), big.data(), 16, 16);
    ini += "entry" + std::to_string(++written) + " = " + base + "," + base + "_16.bmp\n";
  }
  {
    FILE* f = std::fopen((dir / "tiles.ini").string().c_str(), "wb");
    if (f != nullptr) {
      std::fwrite(ini.data(), 1, ini.size(), f);
      std::fclose(f);
    }
  }

  std::printf("hd-capture: %d distinct inked block(s) -> %s\n", written, dir.string().c_str());
  aw_mgba_destroy(core);
  return 0;
}
