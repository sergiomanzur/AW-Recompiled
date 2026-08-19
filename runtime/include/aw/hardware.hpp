#pragma once

#include "aw/cpu_state.hpp"
#include "aw/memory.hpp"
#include <cstdint>

namespace aw {

constexpr std::uint16_t kKeyA      = 1u << 0;
constexpr std::uint16_t kKeyB      = 1u << 1;
constexpr std::uint16_t kKeySelect = 1u << 2;
constexpr std::uint16_t kKeyStart  = 1u << 3;
constexpr std::uint16_t kKeyRight  = 1u << 4;
constexpr std::uint16_t kKeyLeft   = 1u << 5;
constexpr std::uint16_t kKeyUp     = 1u << 6;
constexpr std::uint16_t kKeyDown   = 1u << 7;
constexpr std::uint16_t kKeyR      = 1u << 8;
constexpr std::uint16_t kKeyL      = 1u << 9;

struct Hardware {
  std::uint16_t keys_pressed = 0;
  std::array<std::uint32_t, 4> timer_ticks{};

  void process_dma(Memory& memory, int dma_type);
  void update_keys(Memory& memory);
  void trigger_vblank_irq(CpuState& state);
  void set_scanline(Memory& memory, int scanline);
  void update_timers(CpuState& state, std::uint32_t cycles);
  void check_and_dispatch_irq(CpuState& state);
};

}  // namespace aw
