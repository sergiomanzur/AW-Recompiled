#include "aw/arm_decode.hpp"

#include <stdexcept>

namespace aw {

std::uint32_t read_le32(std::span<const std::uint8_t> bytes, std::size_t offset) {
  if (offset + 4 > bytes.size()) {
    throw std::runtime_error("little-endian read exceeds buffer size");
  }

  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

ArmBranch decode_arm_branch(std::uint32_t instruction, std::uint32_t pc_address) {
  if ((instruction & 0x0E000000u) != 0x0A000000u) {
    throw std::runtime_error("instruction is not an ARM B/BL immediate branch");
  }

  const auto imm24 = instruction & 0x00FFFFFFu;
  auto offset = imm24 << 2;
  if ((imm24 & 0x00800000u) != 0) {
    offset |= 0xFC000000u;
  }

  ArmBranch branch;
  branch.condition = static_cast<std::uint8_t>((instruction >> 28) & 0xF);
  branch.link = (instruction & 0x01000000u) != 0;
  branch.target = pc_address + 8 + offset;
  return branch;
}

}  // namespace aw
