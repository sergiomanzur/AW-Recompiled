#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace aw {

struct ArmBranch {
  std::uint8_t condition = 0;
  bool link = false;
  std::uint32_t target = 0;
};

std::uint32_t read_le32(std::span<const std::uint8_t> bytes, std::size_t offset);
ArmBranch decode_arm_branch(std::uint32_t instruction, std::uint32_t pc_address);

}  // namespace aw
