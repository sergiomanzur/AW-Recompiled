#include "aw/hardware.hpp"
#include "aw/cpu_interpreter.hpp"

namespace aw {

namespace {

inline void set_io_if(Memory& memory, std::uint16_t val) {
  memory.io[0x0202] = static_cast<std::uint8_t>(val & 0xFF);
  memory.io[0x0203] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
}

}  // namespace

void Hardware::update_keys(Memory& memory) {
  // KEYINPUT register at 0x04000130 is ACTIVE LOW (0 = pressed, 1 = unpressed)
  const std::uint16_t keyinput = (~keys_pressed) & 0x03FF;
  write16(memory, 0x04000130, keyinput);
}

void Hardware::set_scanline(Memory& memory, int scanline) {
  write16(memory, 0x04000006, static_cast<std::uint16_t>(scanline));

  std::uint16_t dispstat = read16(memory, 0x04000004);
  dispstat &= ~0x0007;

  if (scanline >= 160) {
    dispstat |= 0x0001; // VBlank flag
  }

  const std::uint16_t vcount_target = (dispstat >> 8) & 0xFF;
  if (scanline == vcount_target) {
    dispstat |= 0x0004; // VCounter match
  }

  write16(memory, 0x04000004, dispstat);

  // HBlank IRQ check (if enabled in DISPSTAT bit 4)
  if (dispstat & (1 << 4)) {
    std::uint16_t if_reg = read16(memory, 0x04000202);
    if_reg |= (1 << 1); // Bit 1 = HBlank IRQ
    set_io_if(memory, if_reg);
  }

  // VCounter IRQ check (if matched and enabled in DISPSTAT bit 5)
  if ((scanline == vcount_target) && (dispstat & (1 << 5))) {
    std::uint16_t if_reg = read16(memory, 0x04000202);
    if_reg |= (1 << 2); // Bit 2 = VCounter IRQ
    write16(memory, 0x04000202, if_reg);
  }
}

void Hardware::process_dma(Memory& memory, int dma_type) {
  static const std::uint32_t dma_reg_base[4] = {0x040000B0, 0x040000BC, 0x040000C8, 0x040000D4};

  for (int ch = 0; ch < 4; ++ch) {
    const std::uint32_t cnt_addr = dma_reg_base[ch] + 8;
    const std::uint32_t cnt = read32(memory, cnt_addr);

    if (!(cnt & (1u << 31))) continue; // Enabled?

    const int start_timing = (cnt >> 28) & 3;
    if (start_timing != dma_type) continue;

    std::uint32_t src = read32(memory, dma_reg_base[ch]);
    std::uint32_t dst = read32(memory, dma_reg_base[ch] + 4);
    std::uint32_t count = cnt & 0xFFFF;
    if (count == 0) count = (ch == 3) ? 0x10000 : 0x4000;

    const int dad_ctl = (cnt >> 21) & 3;
    const int sad_ctl = (cnt >> 23) & 3;
    const bool is_32 = (cnt & (1u << 26)) != 0;
    const std::uint32_t step = is_32 ? 4 : 2;

    for (std::uint32_t i = 0; i < count; ++i) {
      if (is_32) {
        write32(memory, dst, read32(memory, src));
      } else {
        write16(memory, dst, read16(memory, src));
      }

      if (dad_ctl == 0 || dad_ctl == 3) dst += step;
      else if (dad_ctl == 1) dst -= step;

      if (sad_ctl == 0) src += step;
      else if (sad_ctl == 1) src -= step;
    }

    // Disable DMA if not repeat
    const bool repeat = (cnt & (1u << 25)) != 0;
    if (!repeat || start_timing == 0) {
      write32(memory, cnt_addr, cnt & ~(1u << 31));
    }
  }
}

void Hardware::trigger_vblank_irq(CpuState& state) {
  const std::uint16_t dispstat = read16(state.memory, 0x04000004);

  // Direct hardware write to memory.io to set VBlank IF bit 0
  const std::uint16_t if_reg = read16(state.memory, 0x04000202) | 1;
  set_io_if(state.memory, if_reg);

  check_and_dispatch_irq(state);
}

void Hardware::update_timers(CpuState& state, std::uint32_t cycles) {
  static const int prescaler_shifts[4] = {0, 6, 8, 10};

  for (int i = 0; i < 4; ++i) {
    const std::uint32_t cnt_h_addr = 0x04000102 + i * 4;
    const std::uint16_t cnt_h = read16(state.memory, cnt_h_addr);

    // Enabled? (Bit 7)
    if (!(cnt_h & (1 << 7))) continue;

    // Cascade mode? (Bit 2, valid for timer 1-3)
    const bool is_cascade = (i > 0) && ((cnt_h & (1 << 2)) != 0);
    if (is_cascade) continue;

    const int shift = prescaler_shifts[cnt_h & 3];
    timer_ticks[i] += cycles;

    const std::uint32_t ticks_to_add = timer_ticks[i] >> shift;
    if (ticks_to_add == 0) continue;

    timer_ticks[i] &= ((1u << shift) - 1);

    std::uint32_t val = read16(state.memory, 0x04000100 + i * 4);
    val += ticks_to_add;

    if (val > 0xFFFF) {
      // Timer overflow!
      while (val > 0xFFFF) {
        const std::uint16_t reload = read16(state.memory, 0x04000100 + i * 4);
        val = reload + (val - 0x10000);

        // If Timer IRQ enabled (Bit 6 of CNT_H)
        if (cnt_h & (1 << 6)) {
          std::uint16_t if_reg = read16(state.memory, 0x04000202);
          if_reg |= (1 << (3 + i)); // Timer 0=bit 3, Timer 1=bit 4, Timer 2=bit 5, Timer 3=bit 6
          set_io_if(state.memory, if_reg);
        }

        // Trigger cascade on next timer if enabled
        for (int next = i + 1; next < 4; ++next) {
          const std::uint16_t next_h = read16(state.memory, 0x04000102 + next * 4);
          if ((next_h & (1 << 7)) && (next_h & (1 << 2))) {
            std::uint32_t next_val = read16(state.memory, 0x04000100 + next * 4) + 1;
            if (next_val > 0xFFFF) {
              const std::uint16_t next_reload = read16(state.memory, 0x04000100 + next * 4);
              write16(state.memory, 0x04000100 + next * 4, next_reload);
              if (next_h & (1 << 6)) {
                std::uint16_t if_reg = read16(state.memory, 0x04000202);
                if_reg |= (1 << (3 + next));
                set_io_if(state.memory, if_reg);
              }
            } else {
              write16(state.memory, 0x04000100 + next * 4, static_cast<std::uint16_t>(next_val));
              break;
            }
          } else {
            break;
          }
        }
      }
      write16(state.memory, 0x04000100 + i * 4, static_cast<std::uint16_t>(val));
    } else {
      write16(state.memory, 0x04000100 + i * 4, static_cast<std::uint16_t>(val));
    }
  }

  check_and_dispatch_irq(state);
}

void Hardware::check_and_dispatch_irq(CpuState& state) {
  const std::uint16_t ie = read16(state.memory, 0x04000200);
  const std::uint16_t if_reg = read16(state.memory, 0x04000202);
  const std::uint16_t ime = read16(state.memory, 0x04000208);
  const std::uint32_t user_irq_vector = read32(state.memory, 0x03007FFC);

  const std::uint16_t pending = ie & if_reg;

  // Wake CPU from halt if waiting for any pending matching interrupt
  if (state.halted && (state.halt_irq_mask & pending)) {
    state.halted = false;
  }

  // Dispatch IRQ if IME=1, user IRQ vector is set, matching IE/IF bits exist, and IRQs not disabled in CPSR
  if (ime && user_irq_vector != 0 && pending && !(state.cpsr & (1u << 7))) {
    // Ensure CPSR T-bit (bit 5) matches state.thumb before saving SPSR
    if (state.thumb) {
      state.cpsr |= (1u << 5);
    } else {
      state.cpsr &= ~(1u << 5);
    }
    state.spsr_irq = state.cpsr;

    const std::uint32_t user_sp = state.regs[13];
    const std::uint32_t user_lr = state.regs[14];

    // LR_irq must be return_address + 4 so that BIOS "SUBS PC, LR, #4" returns to return_address!
    state.regs[14] = state.regs[15] + 4;
    state.regs[13] = 0x03007FA0; // IRQ stack

    state.r13_irq = user_sp;
    state.r14_irq = user_lr;

    state.cpsr |= (1u << 7);   // Disable IRQs in CPSR
    state.cpsr &= ~(1u << 5);  // Switch to ARM mode for IRQ vector
    state.thumb = false;

    state.regs[15] = 0x00000018; // ARM IRQ vector
  }
}

}  // namespace aw
