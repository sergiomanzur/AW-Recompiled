#include "aw/render/hd_text.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void require(bool condition, const std::string& label) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label.c_str());
  }
}

// 0xAABBGGRR framebuffer colours (post window-copy channel order).
constexpr std::uint32_t kBgBlue = 0xFFA03010u;
constexpr std::uint32_t kInkWhite = 0xFFF0F0F0u;

// An 8x8 block: solid background with an "I-beam" glyph of ink pixels.
void fill_block(std::uint32_t* block, std::uint32_t bg, std::uint32_t ink) {
  for (int i = 0; i < 64; ++i) block[i] = bg;
  block[1] = ink; block[2] = ink; block[3] = ink; block[4] = ink; block[5] = ink;  // top bar
  block[9] = ink; block[17] = ink; block[25] = ink; block[33] = ink; block[41] = ink;  // stem
  block[49] = ink; block[50] = ink; block[51] = ink; block[52] = ink; block[53] = ink;  // bottom
}

void test_hash_is_palette_independent() {
  std::uint32_t a[64];
  std::uint32_t b[64];
  fill_block(a, kBgBlue, kInkWhite);
  // Same glyph shape, completely different palette.
  fill_block(b, 0xFF102030u, 0xFF20E060u);
  require(aw::HdTextPack::block_hash(a) == aw::HdTextPack::block_hash(b),
          "same glyph shape hashes identically across palettes");

  // A different glyph must differ.
  b[60] = kInkWhite;  // Extra ink pixel.
  require(aw::HdTextPack::block_hash(a) != aw::HdTextPack::block_hash(b),
          "different glyph hashes differently");
}

void test_blank_blocks_hash_equally() {
  std::uint32_t a[64];
  std::uint32_t b[64];
  for (int i = 0; i < 64; ++i) a[i] = kBgBlue;
  for (int i = 0; i < 64; ++i) b[i] = 0xFF00FF00u;
  require(aw::HdTextPack::block_hash(a) == aw::HdTextPack::block_hash(b),
          "all-background blocks are palette-independent");
}

void write_bmp16(const std::string& path, const unsigned char* rgb_top_down) {
  // 16x16 24-bit bottom-up BMP.
  const int row = 16 * 3;
  std::vector<unsigned char> bmp(54 + row * 16, 0);
  const int size = 54 + row * 16;
  bmp[0] = 'B'; bmp[1] = 'M';
  bmp[2] = size & 0xFF; bmp[3] = (size >> 8) & 0xFF; bmp[4] = (size >> 16) & 0xFF;
  bmp[10] = 54;
  bmp[14] = 40;
  bmp[18] = 16; bmp[19] = 0;
  bmp[22] = 16; bmp[23] = 0;
  bmp[26] = 1; bmp[28] = 24;
  bmp[34] = row * 16 & 0xFF; bmp[35] = (row * 16 >> 8) & 0xFF;
  for (int y = 0; y < 16; ++y) {
    std::memcpy(&bmp[54 + (15 - y) * row], &rgb_top_down[y * row], row);
  }
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bmp.data()), bmp.size());
}

void test_pack_load_and_composite() {
  std::filesystem::create_directories("hd_test_tiles");

  // Compute the hash of our glyph block.
  std::uint32_t block[64];
  fill_block(block, kBgBlue, kInkWhite);
  const std::uint64_t hash = aw::HdTextPack::block_hash(block);

  // Replacement: a 16x16 all-ink bitmap (every pixel opaque).
  std::vector<unsigned char> rgb(16 * 16 * 3, 0);  // black = ink
  write_bmp16("hd_test_tiles/glyph.bmp", rgb.data());

  {
    std::ofstream ini("hd_test_tiles/tiles.ini", std::ios::binary);
    ini << "[Pack]\nentry1 = " << hash << ",glyph.bmp\n";
  }

  aw::HdTextPack pack;
  require(pack.load("hd_test_tiles/tiles.ini"), "pack loads");
  require(pack.size() == 1, "one entry");
  require(pack.find(hash) != nullptr, "hash resolves");

  // Full 16x8 framebuffer: left 8x8 block is our glyph (painted with the
  // framebuffer's 16-pixel stride so apply_hd_text extracts exactly the
  // block we hashed), right block is blank.
  std::uint32_t glyph[64];
  fill_block(glyph, kBgBlue, kInkWhite);
  std::vector<std::uint32_t> src(16 * 8, kBgBlue);
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      src[static_cast<std::size_t>(y) * 16 + x] = glyph[y * 8 + x];
    }
  }
  std::vector<std::uint32_t> dst(32 * 16, 0xFF3366AAu);
  const int replaced = aw::apply_hd_text(src.data(), 16, 8, dst.data(), 32, pack);
  require(replaced == 1, "exactly the glyph block replaced");

  // The glyph's 2x area is now the sampled ink colour (white).
  require(dst[0] == 0xFFF0F0F0u, "replacement pixels tinted with ink colour");
  // The blank block kept the scaler output (right block's top-left, x=16 y=0).
  require(dst[0 * 32 + 16] == 0xFF3366AAu, "unmatched block untouched");
}

void test_missing_pack_is_dormant() {
  aw::HdTextPack pack;
  require(!pack.load("hd_test_tiles/does_not_exist.ini"), "missing pack reports false");
  std::uint32_t src[64] = {};
  std::uint32_t dst[256] = {};
  require(aw::apply_hd_text(src, 8, 8, dst, 16, pack) == 0, "dormant pack replaces nothing");
}

}  // namespace

int main() {
  test_hash_is_palette_independent();
  test_blank_blocks_hash_equally();
  test_pack_load_and_composite();
  test_missing_pack_is_dormant();

  if (failures == 0) {
    std::printf("hd_text_tests: all tests passed\n");
    return 0;
  }
  std::fprintf(stderr, "hd_text_tests: %d failure(s)\n", failures);
  return 1;
}
