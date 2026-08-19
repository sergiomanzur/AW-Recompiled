#include "aw/rom.hpp"

#include <array>
#include <bit>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace aw {
namespace {

constexpr std::size_t kHeaderSize = 0xC0;
constexpr std::size_t kTitleOffset = 0xA0;
constexpr std::size_t kTitleSize = 12;
constexpr std::size_t kGameCodeOffset = 0xAC;
constexpr std::size_t kGameCodeSize = 4;
constexpr std::size_t kMakerCodeOffset = 0xB0;
constexpr std::size_t kMakerCodeSize = 2;
constexpr std::size_t kFixedValueOffset = 0xB2;
constexpr std::size_t kUnitCodeOffset = 0xB3;
constexpr std::size_t kDeviceTypeOffset = 0xB4;
constexpr std::size_t kVersionOffset = 0xBC;
constexpr std::size_t kComplementOffset = 0xBD;
constexpr const char* kExpectedSha1 = "15053499D5B3F49128A941D7F2D84876F5424D0C";

std::string ascii_field(std::span<const std::uint8_t> bytes,
                        std::size_t offset,
                        std::size_t size) {
  std::string value;
  value.reserve(size);
  for (std::size_t i = 0; i < size; ++i) {
    const auto byte = bytes[offset + i];
    if (byte == 0 || byte == ' ') {
      continue;
    }
    value.push_back(static_cast<char>(byte));
  }
  return value;
}

std::uint32_t rotate_left(std::uint32_t value, int bits) {
  return (value << bits) | (value >> (32 - bits));
}

void append_be64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
  }
}

std::uint32_t read_be32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
  return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
         static_cast<std::uint32_t>(bytes[offset + 3]);
}

}  // namespace

RomImage load_rom_file(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open ROM: " + path.string());
  }

  file.seekg(0, std::ios::end);
  const auto size = file.tellg();
  if (size < 0) {
    throw std::runtime_error("failed to determine ROM size: " + path.string());
  }

  RomImage image;
  image.path = path;
  image.bytes.resize(static_cast<std::size_t>(size));

  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char*>(image.bytes.data()),
            static_cast<std::streamsize>(image.bytes.size()));
  if (!file) {
    throw std::runtime_error("failed to read ROM: " + path.string());
  }

  return image;
}

RomHeader parse_header(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < kHeaderSize) {
    throw std::runtime_error("ROM is too small to contain a GBA header");
  }

  RomHeader header;
  header.title = ascii_field(bytes, kTitleOffset, kTitleSize);
  header.game_code = ascii_field(bytes, kGameCodeOffset, kGameCodeSize);
  header.maker_code = ascii_field(bytes, kMakerCodeOffset, kMakerCodeSize);
  header.fixed_value = bytes[kFixedValueOffset];
  header.unit_code = bytes[kUnitCodeOffset];
  header.device_type = bytes[kDeviceTypeOffset];
  header.version = bytes[kVersionOffset];
  header.complement = bytes[kComplementOffset];
  return header;
}

std::string sha1_hex(std::span<const std::uint8_t> bytes) {
  std::vector<std::uint8_t> message(bytes.begin(), bytes.end());
  const auto bit_length = static_cast<std::uint64_t>(message.size()) * 8;
  message.push_back(0x80);
  while ((message.size() % 64) != 56) {
    message.push_back(0);
  }
  append_be64(message, bit_length);

  std::uint32_t h0 = 0x67452301;
  std::uint32_t h1 = 0xEFCDAB89;
  std::uint32_t h2 = 0x98BADCFE;
  std::uint32_t h3 = 0x10325476;
  std::uint32_t h4 = 0xC3D2E1F0;

  for (std::size_t chunk = 0; chunk < message.size(); chunk += 64) {
    std::array<std::uint32_t, 80> words{};
    for (std::size_t i = 0; i < 16; ++i) {
      words[i] = read_be32(message, chunk + i * 4);
    }
    for (std::size_t i = 16; i < words.size(); ++i) {
      words[i] = rotate_left(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
    }

    auto a = h0;
    auto b = h1;
    auto c = h2;
    auto d = h3;
    auto e = h4;

    for (std::size_t i = 0; i < words.size(); ++i) {
      std::uint32_t f = 0;
      std::uint32_t k = 0;
      if (i < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      const auto temp = rotate_left(a, 5) + f + e + k + words[i];
      e = d;
      d = c;
      c = rotate_left(b, 30);
      b = a;
      a = temp;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }

  std::ostringstream out;
  out << std::uppercase << std::hex << std::setfill('0')
      << std::setw(8) << h0
      << std::setw(8) << h1
      << std::setw(8) << h2
      << std::setw(8) << h3
      << std::setw(8) << h4;
  return out.str();
}

bool is_expected_advance_wars_rev1(const RomImage& rom) {
  const auto header = parse_header(rom.bytes);
  return header.title == "ADVANCEWARS" &&
         header.game_code == "AWRE" &&
         header.maker_code == "01" &&
         header.version == 1 &&
         header.fixed_value == 0x96 &&
         sha1_hex(rom.bytes) == kExpectedSha1;
}

}  // namespace aw
