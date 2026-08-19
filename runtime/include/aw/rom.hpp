#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace aw {

struct RomHeader {
  std::string title;
  std::string game_code;
  std::string maker_code;
  std::uint8_t fixed_value = 0;
  std::uint8_t unit_code = 0;
  std::uint8_t device_type = 0;
  std::uint8_t version = 0;
  std::uint8_t complement = 0;
};

struct RomImage {
  std::filesystem::path path;
  std::vector<std::uint8_t> bytes;
};

RomImage load_rom_file(const std::filesystem::path& path);
RomHeader parse_header(std::span<const std::uint8_t> bytes);
std::string sha1_hex(std::span<const std::uint8_t> bytes);
bool is_expected_advance_wars_rev1(const RomImage& rom);

}  // namespace aw
