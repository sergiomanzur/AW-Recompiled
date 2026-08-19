#pragma once

#include <array>
#include <cstdint>

namespace aw {

struct Memory {
  Memory();

  std::array<std::uint8_t, 16 * 1024> bios{};
  std::array<std::uint8_t, 256 * 1024> ewram{};
  std::array<std::uint8_t, 32 * 1024> iwram{};
  std::array<std::uint8_t, 1024> io{};
  std::array<std::uint8_t, 1024> pram{};
  std::array<std::uint8_t, 96 * 1024> vram{};
  std::array<std::uint8_t, 1024> oam{};
  std::array<std::uint8_t, 64 * 1024> sram{};
  mutable bool flash_id_mode = false;
  const std::uint8_t* rom_data = nullptr;
  std::size_t rom_size = 0;
};

std::uint32_t read32(const Memory& memory, std::uint32_t address);
std::uint16_t read16(const Memory& memory, std::uint32_t address);
std::uint8_t read8(const Memory& memory, std::uint32_t address);
void write8(Memory& memory, std::uint32_t address, std::uint8_t value);
void write16(Memory& memory, std::uint32_t address, std::uint16_t value);
void write32(Memory& memory, std::uint32_t address, std::uint32_t value);

}  // namespace aw
