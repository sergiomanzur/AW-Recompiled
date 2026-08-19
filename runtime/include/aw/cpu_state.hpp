#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "aw/memory.hpp"

namespace aw {

struct CpuState {
  std::array<std::uint32_t, 16> regs{};
  std::uint32_t cpsr = 0;
  bool thumb = false;
  bool trace_enabled = false;
  std::vector<std::string> trace_lines;
  std::uint32_t stop_target = 0;
  Memory memory;

  // IRQ support
  bool halted = false;
  std::uint16_t halt_irq_mask = 0;  // Which IRQ bits to wait for (VBlankIntrWait sets bit 0)

  // Banked registers for IRQ mode
  std::uint32_t spsr_irq = 0;
  std::uint32_t r13_irq = 0x03007FA0;  // GBA default IRQ stack
  std::uint32_t r14_irq = 0;

  // Banked registers for SVC mode
  std::uint32_t spsr_svc = 0;
  std::uint32_t r13_svc = 0x03007FE0;  // GBA default SVC stack
  std::uint32_t r14_svc = 0;
};

void trace(CpuState& state, std::string line);
void stop_at(CpuState& state, std::uint32_t target);

}  // namespace aw
