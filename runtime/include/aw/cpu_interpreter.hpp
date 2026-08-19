#pragma once

#include "aw/cpu_state.hpp"
#include <cstdint>

namespace aw {

// Executes a single instruction at state.regs[15] (ARM or Thumb depending on state.thumb).
// Returns cycles consumed.
std::uint32_t step_interpreter(CpuState& state);

// Executes BIOS SWI function (swi_num) using state registers and state.memory.
void execute_swi(CpuState& state, std::uint8_t swi_num);

// Decompresses LZ77-compressed data from source address to dest address in memory.
void lz77_decompress(Memory& memory, std::uint32_t src_addr, std::uint32_t dst_addr, bool is_vram);

// Decompresses Run-Length compressed data from source address to dest address in memory.
void rl_decompress(Memory& memory, std::uint32_t src_addr, std::uint32_t dst_addr, bool is_vram);

}  // namespace aw
