#include "aw/cpu_interpreter.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace aw {
namespace {

constexpr std::uint32_t kFlagN = 1u << 31;
constexpr std::uint32_t kFlagZ = 1u << 30;
constexpr std::uint32_t kFlagC = 1u << 29;
constexpr std::uint32_t kFlagV = 1u << 28;

bool check_condition(std::uint32_t cpsr, std::uint32_t cond) {
  const bool n = (cpsr & kFlagN) != 0;
  const bool z = (cpsr & kFlagZ) != 0;
  const bool c = (cpsr & kFlagC) != 0;
  const bool v = (cpsr & kFlagV) != 0;

  switch (cond) {
    case 0x0: return z;
    case 0x1: return !z;
    case 0x2: return c;
    case 0x3: return !c;
    case 0x4: return n;
    case 0x5: return !n;
    case 0x6: return v;
    case 0x7: return !v;
    case 0x8: return c && !z;
    case 0x9: return !c || z;
    case 0xA: return n == v;
    case 0xB: return n != v;
    case 0xC: return !z && (n == v);
    case 0xD: return z || (n != v);
    case 0xE: return true;
    default: return false;
  }
}

void set_nz(CpuState& state, std::uint32_t val) {
  state.cpsr &= ~(kFlagN | kFlagZ);
  if (val & kFlagN) state.cpsr |= kFlagN;
  if (val == 0) state.cpsr |= kFlagZ;
}

}  // namespace

void lz77_decompress(Memory& memory, std::uint32_t src_addr, std::uint32_t dst_addr, bool is_vram) {
  const std::uint32_t header = read32(memory, src_addr);
  src_addr += 4;
  const std::uint32_t decomp_size = header >> 8;
  std::uint32_t bytes_written = 0;

  if (is_vram) {
    std::uint16_t halfword = 0;
    auto write_vram_byte = [&](std::uint8_t byte) {
      if (bytes_written % 2 == 0) {
        halfword = byte;
      } else {
        halfword |= (static_cast<std::uint16_t>(byte) << 8);
        write16(memory, dst_addr + bytes_written - 1, halfword);
      }
      bytes_written++;
    };

    while (bytes_written < decomp_size) {
      const std::uint8_t flags = read8(memory, src_addr++);
      for (int bit = 7; bit >= 0 && bytes_written < decomp_size; --bit) {
        if ((flags & (1 << bit)) == 0) {
          const std::uint8_t b = read8(memory, src_addr++);
          write_vram_byte(b);
        } else {
          const std::uint8_t b1 = read8(memory, src_addr++);
          const std::uint8_t b2 = read8(memory, src_addr++);
          const std::uint32_t length = ((b1 >> 4) & 0xF) + 3;
          const std::uint32_t disp = (((b1 & 0xF) << 8) | b2) + 1;
          for (std::uint32_t i = 0; i < length && bytes_written < decomp_size; ++i) {
            const std::uint8_t b = read8(memory, dst_addr + bytes_written - disp);
            write_vram_byte(b);
          }
        }
      }
    }
    if (bytes_written % 2 != 0) {
      write16(memory, dst_addr + bytes_written - 1, halfword);
    }
  } else {
    while (bytes_written < decomp_size) {
      const std::uint8_t flags = read8(memory, src_addr++);
      for (int bit = 7; bit >= 0 && bytes_written < decomp_size; --bit) {
        if ((flags & (1 << bit)) == 0) {
          const std::uint8_t b = read8(memory, src_addr++);
          write8(memory, dst_addr + bytes_written, b);
          bytes_written++;
        } else {
          const std::uint8_t b1 = read8(memory, src_addr++);
          const std::uint8_t b2 = read8(memory, src_addr++);
          const std::uint32_t length = ((b1 >> 4) & 0xF) + 3;
          const std::uint32_t disp = (((b1 & 0xF) << 8) | b2) + 1;
          for (std::uint32_t i = 0; i < length && bytes_written < decomp_size; ++i) {
            const std::uint8_t b = read8(memory, dst_addr + bytes_written - disp);
            write8(memory, dst_addr + bytes_written, b);
            bytes_written++;
          }
        }
      }
    }
  }
}

void rl_decompress(Memory& memory, std::uint32_t src_addr, std::uint32_t dst_addr, bool is_vram) {
  const std::uint32_t header = read32(memory, src_addr);
  src_addr += 4;
  const std::uint32_t decomp_size = header >> 8;
  std::uint32_t bytes_written = 0;

  if (is_vram) {
    std::uint16_t halfword = 0;
    auto write_vram_byte = [&](std::uint8_t byte) {
      if (bytes_written % 2 == 0) {
        halfword = byte;
      } else {
        halfword |= (static_cast<std::uint16_t>(byte) << 8);
        write16(memory, dst_addr + bytes_written - 1, halfword);
      }
      bytes_written++;
    };

    while (bytes_written < decomp_size) {
      const std::uint8_t flag = read8(memory, src_addr++);
      const bool compressed = (flag & 0x80) != 0;
      const std::uint32_t count = (flag & 0x7F) + (compressed ? 3 : 1);

      if (compressed) {
        const std::uint8_t b = read8(memory, src_addr++);
        for (std::uint32_t i = 0; i < count && bytes_written < decomp_size; ++i) {
          write_vram_byte(b);
        }
      } else {
        for (std::uint32_t i = 0; i < count && bytes_written < decomp_size; ++i) {
          const std::uint8_t b = read8(memory, src_addr++);
          write_vram_byte(b);
        }
      }
    }
    if (bytes_written % 2 != 0) {
      write16(memory, dst_addr + bytes_written - 1, halfword);
    }
  } else {
    while (bytes_written < decomp_size) {
      const std::uint8_t flag = read8(memory, src_addr++);
      const bool compressed = (flag & 0x80) != 0;
      const std::uint32_t count = (flag & 0x7F) + (compressed ? 3 : 1);

      if (compressed) {
        const std::uint8_t b = read8(memory, src_addr++);
        for (std::uint32_t i = 0; i < count && bytes_written < decomp_size; ++i) {
          write8(memory, dst_addr + bytes_written, b);
          bytes_written++;
        }
      } else {
        for (std::uint32_t i = 0; i < count && bytes_written < decomp_size; ++i) {
          const std::uint8_t b = read8(memory, src_addr++);
          write8(memory, dst_addr + bytes_written, b);
          bytes_written++;
        }
      }
    }
  }
}

void execute_swi(CpuState& state, std::uint8_t swi_num) {
  switch (swi_num) {
    case 0x00:  // SoftReset
    case 0x01:  // RegisterRamReset
      break;
    case 0x02:  // Halt
      state.halted = true;
      state.halt_irq_mask = 0xFFFF;  // Any interrupt wakes
      break;
    case 0x04:  // IntrWait
    {
      // R0: 1 = discard old flags, 0 = check existing
      // R1: interrupt mask to wait for
      if (state.regs[0] != 0) {
        // Discard existing flags
        write32(state.memory, 0x03007FF8, read32(state.memory, 0x03007FF8) & ~state.regs[1]);
      }
      state.halt_irq_mask = static_cast<std::uint16_t>(state.regs[1]);
      state.halted = true;
      break;
    }
    case 0x05:  // VBlankIntrWait
    {
      // Equivalent to IntrWait(1, 1) — wait for VBlank with discard
      write32(state.memory, 0x03007FF8, read32(state.memory, 0x03007FF8) & ~1u);
      state.halt_irq_mask = 1;  // VBlank only
      state.halted = true;
      break;
    }
    case 0x06:  // Div
    case 0x07:  // DivArm
    {
      const std::int32_t num = static_cast<std::int32_t>(state.regs[0]);
      const std::int32_t denom = static_cast<std::int32_t>(state.regs[1]);
      if (denom != 0) {
        const std::int32_t quot = num / denom;
        const std::int32_t rem = num % denom;
        state.regs[0] = static_cast<std::uint32_t>(quot);
        state.regs[1] = static_cast<std::uint32_t>(rem);
        state.regs[2] = static_cast<std::uint32_t>(std::abs(quot));
      }
      break;
    }
    case 0x08:  // Sqrt
    {
      const std::uint32_t val = state.regs[0];
      state.regs[0] = static_cast<std::uint32_t>(std::sqrt(val));
      break;
    }
    case 0x09:  // CpuSet
    {
      std::uint32_t src = state.regs[0];
      std::uint32_t dst = state.regs[1];
      const std::uint32_t control = state.regs[2];
      const std::uint32_t count = control & 0x1FFFFF;
      const bool fixed = (control & (1u << 24)) != 0;
      const bool is_32 = (control & (1u << 26)) != 0;

      if (is_32) {
        for (std::uint32_t i = 0; i < count; ++i) {
          const std::uint32_t val = read32(state.memory, src);
          write32(state.memory, dst, val);
          dst += 4;
          if (!fixed) src += 4;
        }
      } else {
        for (std::uint32_t i = 0; i < count; ++i) {
          const std::uint16_t val = read16(state.memory, src);
          write16(state.memory, dst, val);
          dst += 2;
          if (!fixed) src += 2;
        }
      }
      break;
    }
    case 0x0A:  // CpuFastSet
    {
      std::uint32_t src = state.regs[0];
      std::uint32_t dst = state.regs[1];
      const std::uint32_t control = state.regs[2];
      const std::uint32_t count = control & 0x1FFFFF;
      const bool fixed = (control & (1u << 24)) != 0;

      for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t val = read32(state.memory, src);
        write32(state.memory, dst, val);
        dst += 4;
        if (!fixed) src += 4;
      }
      break;
    }
    case 0x0B:  // BiosChecksum
    {
      state.regs[0] = 0xBAAE4BAC;
      break;
    }
    case 0x0E:  // BgAffineSet
      break;
    case 0x11:  // LZ77UncompWram
      lz77_decompress(state.memory, state.regs[0], state.regs[1], false);
      break;
    case 0x12:  // LZ77UncompVram
      lz77_decompress(state.memory, state.regs[0], state.regs[1], true);
      break;
    case 0x13:  // RLUncompWram
      rl_decompress(state.memory, state.regs[0], state.regs[1], false);
      break;
    case 0x14:  // RLUncompVram
      rl_decompress(state.memory, state.regs[0], state.regs[1], true);
      break;
    default:
      break;
  }
}

namespace {

std::uint32_t step_thumb(CpuState& state) {
  const std::uint32_t pc = state.regs[15] & ~1u;
  const std::uint16_t op = read16(state.memory, pc);
  state.regs[15] = pc + 2;

  // Format 1: Shifted register
  if ((op & 0xE000) == 0x0000 && (op & 0x1800) != 0x1800) {
    const std::uint32_t shift_op = (op >> 11) & 3;
    const std::uint32_t offset = (op >> 6) & 0x1F;
    const std::uint32_t rs = (op >> 3) & 7;
    const std::uint32_t rd = op & 7;
    std::uint32_t val = state.regs[rs];

    if (shift_op == 0) {  // LSL
      if (offset > 0) {
        const bool carry = (val & (1u << (32 - offset))) != 0;
        state.cpsr = (state.cpsr & ~kFlagC) | (carry ? kFlagC : 0);
        val <<= offset;
      }
    } else if (shift_op == 1) {  // LSR
      if (offset > 0) {
        const bool carry = (val & (1u << (offset - 1))) != 0;
        state.cpsr = (state.cpsr & ~kFlagC) | (carry ? kFlagC : 0);
        val >>= offset;
      } else {
        const bool carry = (val & (1u << 31)) != 0;
        state.cpsr = (state.cpsr & ~kFlagC) | (carry ? kFlagC : 0);
        val = 0;
      }
    } else if (shift_op == 2) {  // ASR
      const auto sval = static_cast<std::int32_t>(val);
      if (offset > 0) {
        const bool carry = (val & (1u << (offset - 1))) != 0;
        state.cpsr = (state.cpsr & ~kFlagC) | (carry ? kFlagC : 0);
        val = static_cast<std::uint32_t>(sval >> offset);
      } else {
        const bool carry = (val & (1u << 31)) != 0;
        state.cpsr = (state.cpsr & ~kFlagC) | (carry ? kFlagC : 0);
        val = (sval < 0) ? 0xFFFFFFFF : 0;
      }
    }
    state.regs[rd] = val;
    set_nz(state, val);
    return 1;
  }

  // Format 2: Add/Subtract
  if ((op & 0xF800) == 0x1800) {
    const bool is_imm = (op & (1 << 10)) != 0;
    const bool is_sub = (op & (1 << 9)) != 0;
    const std::uint32_t rn_imm = (op >> 6) & 7;
    const std::uint32_t rs = (op >> 3) & 7;
    const std::uint32_t rd = op & 7;
    const std::uint32_t op1 = state.regs[rs];
    const std::uint32_t op2 = is_imm ? rn_imm : state.regs[rn_imm];

    if (!is_sub) {
      const std::uint64_t res = static_cast<std::uint64_t>(op1) + op2;
      state.regs[rd] = static_cast<std::uint32_t>(res);
      set_nz(state, state.regs[rd]);
      state.cpsr &= ~(kFlagC | kFlagV);
      if (res > 0xFFFFFFFF) state.cpsr |= kFlagC;
      if (~(op1 ^ op2) & (op1 ^ state.regs[rd]) & 0x80000000) state.cpsr |= kFlagV;
    } else {
      const std::uint32_t res = op1 - op2;
      state.regs[rd] = res;
      set_nz(state, res);
      state.cpsr &= ~(kFlagC | kFlagV);
      if (op1 >= op2) state.cpsr |= kFlagC;
      if ((op1 ^ op2) & (op1 ^ res) & 0x80000000) state.cpsr |= kFlagV;
    }
    return 1;
  }

  // Format 3: Move/compare/add/subtract immediate
  if ((op & 0xE000) == 0x2000) {
    const std::uint32_t alu_op = (op >> 11) & 3;
    const std::uint32_t rd = (op >> 8) & 7;
    const std::uint32_t imm = op & 0xFF;
    const std::uint32_t op1 = state.regs[rd];

    if (alu_op == 0) {  // MOV
      state.regs[rd] = imm;
      set_nz(state, imm);
    } else if (alu_op == 1) {  // CMP
      const std::uint32_t res = op1 - imm;
      set_nz(state, res);
      state.cpsr &= ~(kFlagC | kFlagV);
      if (op1 >= imm) state.cpsr |= kFlagC;
      if ((op1 ^ imm) & (op1 ^ res) & 0x80000000) state.cpsr |= kFlagV;
    } else if (alu_op == 2) {  // ADD
      const std::uint64_t res = static_cast<std::uint64_t>(op1) + imm;
      state.regs[rd] = static_cast<std::uint32_t>(res);
      set_nz(state, state.regs[rd]);
      state.cpsr &= ~(kFlagC | kFlagV);
      if (res > 0xFFFFFFFF) state.cpsr |= kFlagC;
      if (~(op1 ^ imm) & (op1 ^ state.regs[rd]) & 0x80000000) state.cpsr |= kFlagV;
    } else if (alu_op == 3) {  // SUB
      const std::uint32_t res = op1 - imm;
      state.regs[rd] = res;
      set_nz(state, res);
      state.cpsr &= ~(kFlagC | kFlagV);
      if (op1 >= imm) state.cpsr |= kFlagC;
      if ((op1 ^ imm) & (op1 ^ res) & 0x80000000) state.cpsr |= kFlagV;
    }
    return 1;
  }

  // Format 4: ALU operations
  if ((op & 0xFC00) == 0x4000) {
    const std::uint32_t alu_op = (op >> 6) & 0xF;
    const std::uint32_t rs = (op >> 3) & 7;
    const std::uint32_t rd = op & 7;
    const std::uint32_t op1 = state.regs[rd];
    const std::uint32_t op2 = state.regs[rs];

    switch (alu_op) {
      case 0x0: state.regs[rd] &= op2; set_nz(state, state.regs[rd]); break; // AND
      case 0x1: state.regs[rd] ^= op2; set_nz(state, state.regs[rd]); break; // EOR
      case 0x2: state.regs[rd] = op1 << (op2 & 0x1F); set_nz(state, state.regs[rd]); break; // LSL
      case 0x3: state.regs[rd] = op1 >> (op2 & 0x1F); set_nz(state, state.regs[rd]); break; // LSR
      case 0x4: state.regs[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(op1) >> (op2 & 0x1F)); set_nz(state, state.regs[rd]); break; // ASR
      case 0x5: { // ADC
        const std::uint32_t carry = (state.cpsr & kFlagC) ? 1 : 0;
        const std::uint64_t res = static_cast<std::uint64_t>(op1) + op2 + carry;
        state.regs[rd] = static_cast<std::uint32_t>(res);
        set_nz(state, state.regs[rd]);
        state.cpsr &= ~(kFlagC | kFlagV);
        if (res > 0xFFFFFFFF) state.cpsr |= kFlagC;
        break;
      }
      case 0x6: { // SBC
        const std::uint32_t carry = (state.cpsr & kFlagC) ? 0 : 1;
        const std::uint64_t res = static_cast<std::uint64_t>(op1) - op2 - carry;
        state.regs[rd] = static_cast<std::uint32_t>(res);
        set_nz(state, state.regs[rd]);
        state.cpsr &= ~(kFlagC | kFlagV);
        if (op1 >= op2 + carry) state.cpsr |= kFlagC;
        break;
      }
      case 0x7: { // ROR
        const std::uint32_t shift = op2 & 0x1F;
        state.regs[rd] = (op1 >> shift) | (op1 << (32 - shift));
        set_nz(state, state.regs[rd]);
        break;
      }
      case 0x8: set_nz(state, op1 & op2); break; // TST
      case 0x9: { // NEG
        const std::uint32_t res = 0 - op2;
        state.regs[rd] = res;
        set_nz(state, res);
        state.cpsr &= ~(kFlagC | kFlagV);
        if (0 >= op2) state.cpsr |= kFlagC;
        break;
      }
      case 0xA: { // CMP
        const std::uint32_t res = op1 - op2;
        set_nz(state, res);
        state.cpsr &= ~(kFlagC | kFlagV);
        if (op1 >= op2) state.cpsr |= kFlagC;
        if ((op1 ^ op2) & (op1 ^ res) & 0x80000000) state.cpsr |= kFlagV;
        break;
      }
      case 0xB: { // CMN
        const std::uint64_t res = static_cast<std::uint64_t>(op1) + op2;
        set_nz(state, static_cast<std::uint32_t>(res));
        state.cpsr &= ~(kFlagC | kFlagV);
        if (res > 0xFFFFFFFF) state.cpsr |= kFlagC;
        break;
      }
      case 0xC: state.regs[rd] |= op2; set_nz(state, state.regs[rd]); break; // ORR
      case 0xD: state.regs[rd] = op1 * op2; set_nz(state, state.regs[rd]); break; // MUL
      case 0xE: state.regs[rd] &= ~op2; set_nz(state, state.regs[rd]); break; // BIC
      case 0xF: state.regs[rd] = ~op2; set_nz(state, state.regs[rd]); break; // MVN
    }
    return 1;
  }

  // Format 5: High register ops / Branch exchange
  if ((op & 0xFC00) == 0x4400) {
    const std::uint32_t alu_op = (op >> 8) & 3;
    const std::uint32_t h1 = (op >> 7) & 1;
    const std::uint32_t h2 = (op >> 6) & 1;
    const std::uint32_t rs = ((op >> 3) & 7) | (h2 << 3);
    const std::uint32_t rd = (op & 7) | (h1 << 3);
    const std::uint32_t val_s = state.regs[rs];

    if (alu_op == 0) {  // ADD
      state.regs[rd] += val_s;
    } else if (alu_op == 1) {  // CMP
      const std::uint32_t res = state.regs[rd] - val_s;
      set_nz(state, res);
      state.cpsr &= ~(kFlagC | kFlagV);
      if (state.regs[rd] >= val_s) state.cpsr |= kFlagC;
    } else if (alu_op == 2) {  // MOV
      state.regs[rd] = val_s;
    } else if (alu_op == 3) {  // BX
      state.thumb = (val_s & 1) != 0;
      if (state.thumb) state.cpsr |= (1u << 5);
      else state.cpsr &= ~(1u << 5);
      state.regs[15] = val_s & ~1u;
    }
    return 1;
  }

  // Format 6: PC-relative load
  if ((op & 0xF800) == 0x4800) {
    const std::uint32_t rd = (op >> 8) & 7;
    const std::uint32_t imm = (op & 0xFF) << 2;
    const std::uint32_t addr = ((pc + 4) & ~3u) + imm;
    state.regs[rd] = read32(state.memory, addr);
    return 1;
  }

  // Format 7 & 8: Load/store register offset
  if ((op & 0xF200) == 0x5000) {
    const std::uint32_t bit_l = (op >> 11) & 1;
    const std::uint32_t bit_b = (op >> 10) & 1;
    const std::uint32_t ro = (op >> 6) & 7;
    const std::uint32_t rb = (op >> 3) & 7;
    const std::uint32_t rd = op & 7;
    const std::uint32_t addr = state.regs[rb] + state.regs[ro];

    if (bit_l == 0) {
      if (bit_b == 0) write32(state.memory, addr, state.regs[rd]);
      else write8(state.memory, addr, static_cast<std::uint8_t>(state.regs[rd]));
    } else {
      if (bit_b == 0) state.regs[rd] = read32(state.memory, addr);
      else state.regs[rd] = read8(state.memory, addr);
    }
    return 1;
  }

  if ((op & 0xF200) == 0x5200) {
    const std::uint32_t bit_h = (op >> 11) & 1;
    const std::uint32_t bit_s = (op >> 10) & 1;
    const std::uint32_t ro = (op >> 6) & 7;
    const std::uint32_t rb = (op >> 3) & 7;
    const std::uint32_t rd = op & 7;
    const std::uint32_t addr = state.regs[rb] + state.regs[ro];

    if (bit_s == 0 && bit_h == 0) { // STRH
      write16(state.memory, addr, static_cast<std::uint16_t>(state.regs[rd]));
    } else if (bit_s == 0 && bit_h == 1) { // LDRH
      state.regs[rd] = read16(state.memory, addr);
    } else if (bit_s == 1 && bit_h == 0) { // LDSB
      const auto sb = static_cast<std::int8_t>(read8(state.memory, addr));
      state.regs[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(sb));
    } else if (bit_s == 1 && bit_h == 1) { // LDSH
      const auto sh = static_cast<std::int16_t>(read16(state.memory, addr));
      state.regs[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(sh));
    }
    return 1;
  }

  // Format 9: Load/store immediate offset
  if ((op & 0xE000) == 0x6000) {
    const bool is_byte = (op & (1 << 12)) != 0;
    const bool is_load = (op & (1 << 11)) != 0;
    const std::uint32_t offset = ((op >> 6) & 0x1F) << (is_byte ? 0 : 2);
    const std::uint32_t rb = (op >> 3) & 7;
    const std::uint32_t rd = op & 7;
    const std::uint32_t addr = state.regs[rb] + offset;

    if (!is_load) {
      if (is_byte) write8(state.memory, addr, static_cast<std::uint8_t>(state.regs[rd]));
      else write32(state.memory, addr, state.regs[rd]);
    } else {
      if (is_byte) state.regs[rd] = read8(state.memory, addr);
      else state.regs[rd] = read32(state.memory, addr);
    }
    return 1;
  }

  // Format 10: Load/store halfword
  if ((op & 0xF000) == 0x8000) {
    const bool is_load = (op & (1 << 11)) != 0;
    const std::uint32_t offset = ((op >> 6) & 0x1F) << 1;
    const std::uint32_t rb = (op >> 3) & 7;
    const std::uint32_t rd = op & 7;
    const std::uint32_t addr = state.regs[rb] + offset;

    if (!is_load) write16(state.memory, addr, static_cast<std::uint16_t>(state.regs[rd]));
    else state.regs[rd] = read16(state.memory, addr);
    return 1;
  }

  // Format 11: SP-relative load/store
  if ((op & 0xF000) == 0x9000) {
    const bool is_load = (op & (1 << 11)) != 0;
    const std::uint32_t rd = (op >> 8) & 7;
    const std::uint32_t offset = (op & 0xFF) << 2;
    const std::uint32_t addr = state.regs[13] + offset;

    if (!is_load) write32(state.memory, addr, state.regs[rd]);
    else state.regs[rd] = read32(state.memory, addr);
    return 1;
  }

  // Format 12: Load address
  if ((op & 0xF000) == 0xA000) {
    const bool use_sp = (op & (1 << 11)) != 0;
    const std::uint32_t rd = (op >> 8) & 7;
    const std::uint32_t offset = (op & 0xFF) << 2;
    const std::uint32_t base = use_sp ? state.regs[13] : ((pc + 4) & ~3u);
    state.regs[rd] = base + offset;
    return 1;
  }

  // Format 13: Add offset to SP
  if ((op & 0xFF00) == 0xB000) {
    const bool is_neg = (op & 0x80) != 0;
    const std::uint32_t offset = (op & 0x7F) << 2;
    if (is_neg) state.regs[13] -= offset;
    else state.regs[13] += offset;
    return 1;
  }

  // Format 14: Push/Pop registers
  if ((op & 0xF600) == 0xB400) {
    const bool is_pop = (op & (1 << 11)) != 0;
    const bool bit_r = (op & (1 << 8)) != 0;
    const std::uint32_t rlist = op & 0xFF;

    if (!is_pop) { // PUSH
      std::uint32_t addr = state.regs[13];
      if (bit_r) { addr -= 4; write32(state.memory, addr, state.regs[14]); }
      for (int i = 7; i >= 0; --i) {
        if (rlist & (1 << i)) { addr -= 4; write32(state.memory, addr, state.regs[i]); }
      }
      state.regs[13] = addr;
    } else { // POP
      std::uint32_t addr = state.regs[13];
      for (int i = 0; i <= 7; ++i) {
        if (rlist & (1 << i)) { state.regs[i] = read32(state.memory, addr); addr += 4; }
      }
      if (bit_r) {
        const std::uint32_t val_pc = read32(state.memory, addr); addr += 4;
        state.thumb = (val_pc & 1) != 0;
        state.regs[15] = val_pc & ~1u;
      }
      state.regs[13] = addr;
    }
    return 1;
  }

  // Format 15: Multiple load/store
  if ((op & 0xF000) == 0xC000) {
    const bool is_load = (op & (1 << 11)) != 0;
    const std::uint32_t rb = (op >> 8) & 7;
    const std::uint32_t rlist = op & 0xFF;
    std::uint32_t addr = state.regs[rb];

    for (int i = 0; i <= 7; ++i) {
      if (rlist & (1 << i)) {
        if (!is_load) write32(state.memory, addr, state.regs[i]);
        else state.regs[i] = read32(state.memory, addr);
        addr += 4;
      }
    }
    state.regs[rb] = addr;
    return 1;
  }

  // Format 16: Bcond / SWI
  if ((op & 0xF000) == 0xD000) {
    const std::uint32_t cond = (op >> 8) & 0xF;
    if (cond == 0xF) { // SWI
      execute_swi(state, static_cast<std::uint8_t>(op & 0xFF));
    } else { // Bcond
      if (check_condition(state.cpsr, cond)) {
        const auto offset = static_cast<std::int8_t>(op & 0xFF);
        state.regs[15] = (pc + 4 + (offset * 2)) & ~1u;
      }
    }
    return 1;
  }

  // Format 18: Unconditional branch
  if ((op & 0xF800) == 0xE000) {
    std::int32_t offset = (op & 0x7FF) << 1;
    if (offset & 0x800) offset |= ~0xFFF;
    state.regs[15] = (pc + 4 + offset) & ~1u;
    return 1;
  }

  // Format 19: Long branch with link (BL)
  if ((op & 0xF800) == 0xF000) {
    const std::uint16_t next_op = read16(state.memory, pc + 2);
    if ((next_op & 0xF800) == 0xF800) {
      state.regs[15] = pc + 4;
      std::int32_t offset1 = (op & 0x7FF);
      if (offset1 & 0x400) offset1 |= ~0x7FF;
      std::int32_t offset2 = (next_op & 0x7FF);
      const std::uint32_t target = pc + 4 + (offset1 << 12) + (offset2 << 1);
      state.regs[14] = (pc + 2) | 1;
      state.regs[15] = target & ~1u;
      return 2;
    }
  }

  // Format 17: Software Interrupt (SWI)
  if ((op & 0xFF00) == 0xDF00) {
    state.regs[15] = pc + 2;
    const std::uint8_t swi_num = op & 0xFF;
    if (swi_num == 0x04 || swi_num == 0x05) {
      const std::uint16_t irq_mask = (swi_num == 0x05) ? 0x0001 : static_cast<std::uint16_t>(state.regs[1]);
      const bool reset_flags = (swi_num == 0x04) ? (state.regs[0] != 0) : true;
      const std::uint16_t bios_flags = read16(state.memory, 0x03007FF8);

      if (bios_flags & irq_mask) {
        if (reset_flags) {
          write16(state.memory, 0x03007FF8, bios_flags & ~irq_mask);
        }
        state.halted = false;
      } else {
        state.cpsr &= ~(1u << 7); // Enable IRQs in CPSR
        state.halted = true;
        state.halt_irq_mask = irq_mask;
      }
      return 1;
    } else if (swi_num == 0x02) {
      state.cpsr &= ~(1u << 7);
      state.halted = true;
      state.halt_irq_mask = 0xFFFF;
      return 1;
    } else if (swi_num == 0x0B || swi_num == 0x0C) {
      const std::uint32_t src = state.regs[0];
      const std::uint32_t dst = state.regs[1];
      const std::uint32_t cnt_reg = state.regs[2];
      const std::size_t count = (cnt_reg & 0x001FFFFF);
      const bool is_fixed = (cnt_reg & (1u << 24)) != 0;
      const bool is_32bit = (swi_num == 0x0C) || ((cnt_reg & (1u << 26)) != 0);

      if (is_32bit) {
        std::uint32_t val = is_fixed ? read32(state.memory, src) : 0;
        for (std::size_t i = 0; i < count; ++i) {
          if (!is_fixed) val = read32(state.memory, src + i * 4);
          write32(state.memory, dst + i * 4, val);
        }
      } else {
        std::uint16_t val = is_fixed ? read16(state.memory, src) : 0;
        for (std::size_t i = 0; i < count; ++i) {
          if (!is_fixed) val = read16(state.memory, src + i * 2);
          write16(state.memory, dst + i * 2, val);
        }
      }
    }
    return 1;
  }
  return 1;
}

std::uint32_t step_arm(CpuState& state) {
  const std::uint32_t pc = state.regs[15] & ~3u;
  const std::uint32_t ins = read32(state.memory, pc);
  state.regs[15] = pc + 4;

  const std::uint32_t cond = ins >> 28;
  if (!check_condition(state.cpsr, cond)) {
    return 1;
  }

  // SWI
  if ((ins & 0x0F000000) == 0x0F000000) {
    const std::uint8_t swi_num = static_cast<std::uint8_t>((ins >> 16) & 0xFF);
    execute_swi(state, swi_num);
    return 1;
  }

  // Branch and Exchange (BX)
  if ((ins & 0x0FFFFFF0) == 0x012FFF10) {
    const std::uint32_t rm = ins & 0xF;
    const std::uint32_t val = state.regs[rm];
    state.thumb = (val & 1) != 0;
    if (state.thumb) state.cpsr |= (1u << 5);
    else state.cpsr &= ~(1u << 5);
    state.regs[15] = val & ~1u;
    return 1;
  }

  // Branch & Branch with Link (B, BL)
  if ((ins & 0x0E000000) == 0x0A000000) {
    const bool is_bl = (ins & (1u << 24)) != 0;
    std::int32_t offset = (ins & 0x00FFFFFF) << 2;
    if (offset & 0x02000000) offset |= ~0x03FFFFFF;
    if (is_bl) {
      state.regs[14] = pc + 4;
    }
    state.regs[15] = pc + 8 + offset;
    return 1;
  }

  // MRS (Move PSR to register)
  if ((ins & 0x0FBF0FFF) == 0x010F0000) {
    const std::uint32_t rd = (ins >> 12) & 0xF;
    if (state.thumb) state.cpsr |= (1u << 5);
    else state.cpsr &= ~(1u << 5);
    state.regs[rd] = state.cpsr;
    return 1;
  }

  // MSR (Move register/immediate to PSR)
  if ((ins & 0x0DB0F000) == 0x0120F000) {
    const bool is_imm = (ins & (1u << 25)) != 0;
    std::uint32_t val = 0;
    if (is_imm) {
      const std::uint32_t imm8 = ins & 0xFF;
      const std::uint32_t rot = ((ins >> 8) & 0xF) * 2;
      val = (rot == 0) ? imm8 : ((imm8 >> rot) | (imm8 << (32 - rot)));
    } else {
      val = state.regs[ins & 0xF];
    }
    const std::uint32_t field_mask = (ins >> 16) & 0xF;
    std::uint32_t mask = 0;
    if (field_mask & 1) mask |= 0x000000FF;
    if (field_mask & 2) mask |= 0x0000FF00;
    if (field_mask & 4) mask |= 0x00FF0000;
    if (field_mask & 8) mask |= 0xFF000000;
    state.cpsr = (state.cpsr & ~mask) | (val & mask);
    state.thumb = (state.cpsr & (1u << 5)) != 0;
    return 1;
  }

  // Multiply (MUL, MLA) — must check before data processing
  // Pattern: bits[27:22]=000000, bits[7:4]=1001
  if ((ins & 0x0FC000F0) == 0x00000090) {
    const bool accumulate = (ins & (1u << 21)) != 0;
    const bool set_flags = (ins & (1u << 20)) != 0;
    const std::uint32_t rd = (ins >> 16) & 0xF;
    const std::uint32_t rn = (ins >> 12) & 0xF;
    const std::uint32_t rs = (ins >> 8) & 0xF;
    const std::uint32_t rm = ins & 0xF;
    std::uint32_t res = state.regs[rm] * state.regs[rs];
    if (accumulate) res += state.regs[rn];
    state.regs[rd] = res;
    if (set_flags) set_nz(state, res);
    return 1;
  }

  // Multiply Long (UMULL, UMLAL, SMULL, SMLAL)
  if ((ins & 0x0F8000F0) == 0x00800090) {
    const bool is_signed = (ins & (1u << 22)) != 0;
    const bool accumulate = (ins & (1u << 21)) != 0;
    const bool set_flags = (ins & (1u << 20)) != 0;
    const std::uint32_t rd_hi = (ins >> 16) & 0xF;
    const std::uint32_t rd_lo = (ins >> 12) & 0xF;
    const std::uint32_t rs = (ins >> 8) & 0xF;
    const std::uint32_t rm = ins & 0xF;

    std::int64_t result = 0;
    if (is_signed) {
      result = static_cast<std::int64_t>(static_cast<std::int32_t>(state.regs[rm])) *
               static_cast<std::int64_t>(static_cast<std::int32_t>(state.regs[rs]));
    } else {
      result = static_cast<std::int64_t>(
          static_cast<std::uint64_t>(state.regs[rm]) * state.regs[rs]);
    }
    if (accumulate) {
      const std::uint64_t acc = (static_cast<std::uint64_t>(state.regs[rd_hi]) << 32) |
                                state.regs[rd_lo];
      result += static_cast<std::int64_t>(acc);
    }
    state.regs[rd_lo] = static_cast<std::uint32_t>(result);
    state.regs[rd_hi] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(result) >> 32);
    if (set_flags) {
      set_nz(state, state.regs[rd_hi]); // N/Z based on high word
      if (state.regs[rd_lo] == 0 && state.regs[rd_hi] == 0) state.cpsr |= kFlagZ;
    }
    return 1;
  }

  // Single Data Swap (SWP, SWPB)
  if ((ins & 0x0FB00FF0) == 0x01000090) {
    const bool is_byte = (ins & (1u << 22)) != 0;
    const std::uint32_t rn = (ins >> 16) & 0xF;
    const std::uint32_t rd = (ins >> 12) & 0xF;
    const std::uint32_t rm = ins & 0xF;
    const std::uint32_t addr = state.regs[rn];
    if (is_byte) {
      const std::uint8_t old = read8(state.memory, addr);
      write8(state.memory, addr, static_cast<std::uint8_t>(state.regs[rm]));
      state.regs[rd] = old;
    } else {
      const std::uint32_t old = read32(state.memory, addr);
      write32(state.memory, addr, state.regs[rm]);
      state.regs[rd] = old;
    }
    return 1;
  }

  // Halfword / Signed load/store (LDRH, STRH, LDRSB, LDRSH)
  // Pattern: bits[27:25]=000, bit[7]=1, bit[4]=1, not multiply
  if ((ins & 0x0E000090) == 0x00000090 && (ins & 0x00000060) != 0) {
    const bool is_pre = (ins & (1u << 24)) != 0;
    const bool is_add = (ins & (1u << 23)) != 0;
    const bool is_imm = (ins & (1u << 22)) != 0;
    const bool writeback = (ins & (1u << 21)) != 0;
    const bool is_load = (ins & (1u << 20)) != 0;
    const std::uint32_t rn = (ins >> 16) & 0xF;
    const std::uint32_t rd = (ins >> 12) & 0xF;
    const std::uint32_t sh = (ins >> 5) & 3;  // SH bits: 01=H, 10=SB, 11=SH

    std::uint32_t offset = 0;
    if (is_imm) {
      offset = ((ins >> 4) & 0xF0) | (ins & 0xF);
    } else {
      offset = state.regs[ins & 0xF];
    }

    std::uint32_t base = (rn == 15) ? pc + 8 : state.regs[rn];
    std::uint32_t addr = is_pre ? (is_add ? base + offset : base - offset) : base;

    if (is_load) {
      switch (sh) {
        case 1: // LDRH
          state.regs[rd] = read16(state.memory, addr);
          break;
        case 2: { // LDRSB
          const auto sb = static_cast<std::int8_t>(read8(state.memory, addr));
          state.regs[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(sb));
          break;
        }
        case 3: { // LDRSH
          const auto sh_val = static_cast<std::int16_t>(read16(state.memory, addr));
          state.regs[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(sh_val));
          break;
        }
        default: break;
      }
    } else {
      // STRH (sh==1)
      write16(state.memory, addr, static_cast<std::uint16_t>(state.regs[rd]));
    }

    if (!is_pre) {
      // Post-indexed
      state.regs[rn] = is_add ? base + offset : base - offset;
    } else if (writeback) {
      state.regs[rn] = addr;
    }
    return 1;
  }

  // Data processing immediate / register
  if ((ins & 0x0C000000) == 0x00000000) {
    const bool is_imm = (ins & (1u << 25)) != 0;
    const std::uint32_t opcode = (ins >> 21) & 0xF;
    const bool set_flags = (ins & (1u << 20)) != 0;
    const std::uint32_t rn = (ins >> 16) & 0xF;
    const std::uint32_t rd = (ins >> 12) & 0xF;
    const std::uint32_t op1 = (rn == 15) ? pc + 8 : state.regs[rn];
    std::uint32_t op2 = 0;
    bool shifter_carry = (state.cpsr & kFlagC) != 0;

    if (is_imm) {
      const std::uint32_t imm8 = ins & 0xFF;
      const std::uint32_t rot = ((ins >> 8) & 0xF) * 2;
      if (rot == 0) {
        op2 = imm8;
      } else {
        op2 = (imm8 >> rot) | (imm8 << (32 - rot));
        shifter_carry = (op2 >> 31) != 0;
      }
    } else {
      const std::uint32_t rm = ins & 0xF;
      std::uint32_t val = (rm == 15) ? pc + 8 : state.regs[rm];
      const std::uint32_t shift_type = (ins >> 5) & 3;
      std::uint32_t shift_amount = 0;

      if (ins & (1u << 4)) {
        // Register-specified shift
        shift_amount = state.regs[(ins >> 8) & 0xF] & 0xFF;
      } else {
        shift_amount = (ins >> 7) & 0x1F;
      }

      switch (shift_type) {
        case 0: // LSL
          if (shift_amount > 0) {
            if (shift_amount < 32) {
              shifter_carry = (val & (1u << (32 - shift_amount))) != 0;
              val <<= shift_amount;
            } else if (shift_amount == 32) {
              shifter_carry = (val & 1) != 0;
              val = 0;
            } else {
              shifter_carry = false;
              val = 0;
            }
          }
          break;
        case 1: // LSR
          if (shift_amount == 0 && !(ins & (1u << 4))) shift_amount = 32;
          if (shift_amount > 0) {
            if (shift_amount < 32) {
              shifter_carry = (val & (1u << (shift_amount - 1))) != 0;
              val >>= shift_amount;
            } else if (shift_amount == 32) {
              shifter_carry = (val >> 31) != 0;
              val = 0;
            } else {
              shifter_carry = false;
              val = 0;
            }
          }
          break;
        case 2: { // ASR
          if (shift_amount == 0 && !(ins & (1u << 4))) shift_amount = 32;
          const auto sval = static_cast<std::int32_t>(val);
          if (shift_amount >= 32) {
            shifter_carry = (val >> 31) != 0;
            val = (sval < 0) ? 0xFFFFFFFF : 0;
          } else if (shift_amount > 0) {
            shifter_carry = (val & (1u << (shift_amount - 1))) != 0;
            val = static_cast<std::uint32_t>(sval >> shift_amount);
          }
          break;
        }
        case 3: // ROR
          if (shift_amount == 0 && !(ins & (1u << 4))) {
            // RRX (rotate right extended)
            const bool old_carry = (state.cpsr & kFlagC) != 0;
            shifter_carry = (val & 1) != 0;
            val = (val >> 1) | (old_carry ? 0x80000000u : 0);
          } else if (shift_amount > 0) {
            shift_amount &= 0x1F;
            if (shift_amount == 0) {
              shifter_carry = (val >> 31) != 0;
            } else {
              val = (val >> shift_amount) | (val << (32 - shift_amount));
              shifter_carry = (val >> 31) != 0;
            }
          }
          break;
      }
      op2 = val;
    }

    std::uint32_t res = 0;
    bool write_result = true;
    switch (opcode) {
      case 0x0: res = op1 & op2; break; // AND
      case 0x1: res = op1 ^ op2; break; // EOR
      case 0x2: res = op1 - op2; break; // SUB
      case 0x3: res = op2 - op1; break; // RSB
      case 0x4: res = op1 + op2; break; // ADD
      case 0x5: res = op1 + op2 + ((state.cpsr & kFlagC) ? 1 : 0); break; // ADC
      case 0x6: res = op1 - op2 - ((state.cpsr & kFlagC) ? 0 : 1); break; // SBC
      case 0x7: res = op2 - op1 - ((state.cpsr & kFlagC) ? 0 : 1); break; // RSC
      case 0x8: res = op1 & op2; write_result = false; break; // TST
      case 0x9: res = op1 ^ op2; write_result = false; break; // TEQ
      case 0xA: res = op1 - op2; write_result = false; break; // CMP
      case 0xB: res = op1 + op2; write_result = false; break; // CMN
      case 0xC: res = op1 | op2; break; // ORR
      case 0xD: res = op2; break;       // MOV
      case 0xE: res = op1 & ~op2; break;// BIC
      case 0xF: res = ~op2; break;      // MVN
    }

    if (write_result) {
      state.regs[rd] = res;
      if (rd == 15 && set_flags) {
        // SUBS PC, LR / MOVS PC, LR — return from exception
        // Restore CPSR from SPSR and banked registers
        state.cpsr = state.spsr_irq;
        state.thumb = (state.cpsr & (1u << 5)) != 0;

        // Restore user-mode SP and LR from banked copies
        state.regs[13] = state.r13_irq;
        state.regs[14] = state.r14_irq;
      }
    }
    if (set_flags && !(rd == 15 && write_result)) {
      set_nz(state, res);
      switch (opcode) {
        case 0x0: case 0x1: case 0x8: case 0x9:
        case 0xC: case 0xD: case 0xE: case 0xF:
          // Logical ops: carry from shifter
          state.cpsr = (state.cpsr & ~kFlagC) | (shifter_carry ? kFlagC : 0);
          break;
        case 0x2: case 0xA: // SUB, CMP
          state.cpsr &= ~(kFlagC | kFlagV);
          if (op1 >= op2) state.cpsr |= kFlagC;
          if ((op1 ^ op2) & (op1 ^ res) & 0x80000000) state.cpsr |= kFlagV;
          break;
        case 0x3: // RSB
          state.cpsr &= ~(kFlagC | kFlagV);
          if (op2 >= op1) state.cpsr |= kFlagC;
          if ((op2 ^ op1) & (op2 ^ res) & 0x80000000) state.cpsr |= kFlagV;
          break;
        case 0x4: case 0xB: { // ADD, CMN
          const std::uint64_t ures = static_cast<std::uint64_t>(op1) + op2;
          state.cpsr &= ~(kFlagC | kFlagV);
          if (ures > 0xFFFFFFFF) state.cpsr |= kFlagC;
          if (~(op1 ^ op2) & (op1 ^ res) & 0x80000000) state.cpsr |= kFlagV;
          break;
        }
        case 0x5: { // ADC
          const std::uint32_t c = (state.cpsr & kFlagC) ? 1 : 0;
          // Note: flags were set on res which already includes carry
          const std::uint64_t ures = static_cast<std::uint64_t>(op1) + op2 + c;
          state.cpsr &= ~(kFlagC | kFlagV);
          if (ures > 0xFFFFFFFF) state.cpsr |= kFlagC;
          if (~(op1 ^ op2) & (op1 ^ res) & 0x80000000) state.cpsr |= kFlagV;
          break;
        }
        case 0x6: { // SBC
          state.cpsr &= ~(kFlagC | kFlagV);
          const std::uint32_t borrow = (state.cpsr & kFlagC) ? 0 : 1;
          if (op1 >= op2 + borrow) state.cpsr |= kFlagC;
          if ((op1 ^ op2) & (op1 ^ res) & 0x80000000) state.cpsr |= kFlagV;
          break;
        }
        case 0x7: { // RSC
          state.cpsr &= ~(kFlagC | kFlagV);
          const std::uint32_t borrow = (state.cpsr & kFlagC) ? 0 : 1;
          if (op2 >= op1 + borrow) state.cpsr |= kFlagC;
          if ((op2 ^ op1) & (op2 ^ res) & 0x80000000) state.cpsr |= kFlagV;
          break;
        }
      }
    }
    return 1;
  }

  // Single Data Transfer (LDR, STR) — immediate and register offset
  if ((ins & 0x0C000000) == 0x04000000) {
    const bool is_pre = (ins & (1u << 24)) != 0;
    const bool is_add = (ins & (1u << 23)) != 0;
    const bool is_byte = (ins & (1u << 22)) != 0;
    const bool writeback = (ins & (1u << 21)) != 0;
    const bool is_load = (ins & (1u << 20)) != 0;
    const bool is_reg = (ins & (1u << 25)) != 0;
    const std::uint32_t rn = (ins >> 16) & 0xF;
    const std::uint32_t rd = (ins >> 12) & 0xF;
    const std::uint32_t base = (rn == 15) ? pc + 8 : state.regs[rn];

    std::uint32_t offset = 0;
    if (!is_reg) {
      offset = ins & 0xFFF;
    } else {
      const std::uint32_t rm = ins & 0xF;
      std::uint32_t val = state.regs[rm];
      const std::uint32_t shift_type = (ins >> 5) & 3;
      const std::uint32_t shift_amount = (ins >> 7) & 0x1F;
      switch (shift_type) {
        case 0: val <<= shift_amount; break;
        case 1: val = shift_amount ? (val >> shift_amount) : 0; break;
        case 2: {
          if (shift_amount == 0) val = (static_cast<std::int32_t>(val) < 0) ? 0xFFFFFFFF : 0;
          else val = static_cast<std::uint32_t>(static_cast<std::int32_t>(val) >> shift_amount);
          break;
        }
        case 3: {
          if (shift_amount == 0) {
            const bool c = (state.cpsr & kFlagC) != 0;
            val = (val >> 1) | (c ? 0x80000000u : 0);
          } else {
            val = (val >> shift_amount) | (val << (32 - shift_amount));
          }
          break;
        }
      }
      offset = val;
    }

    std::uint32_t addr = is_pre ? (is_add ? base + offset : base - offset) : base;

    if (is_load) {
      state.regs[rd] = is_byte ? read8(state.memory, addr) : read32(state.memory, addr);
      if (rd == 15) {
        state.thumb = (state.regs[15] & 1) != 0;
        state.regs[15] &= ~1u;
      }
    } else {
      const std::uint32_t val = (rd == 15) ? pc + 12 : state.regs[rd];
      if (is_byte) write8(state.memory, addr, static_cast<std::uint8_t>(val));
      else write32(state.memory, addr, val);
    }

    if (!is_pre) {
      state.regs[rn] = is_add ? base + offset : base - offset;
    } else if (writeback) {
      state.regs[rn] = addr;
    }
    return 1;
  }

  // Block Data Transfer (LDM, STM)
  if ((ins & 0x0E000000) == 0x08000000) {
    const bool is_pre = (ins & (1u << 24)) != 0;
    const bool is_add = (ins & (1u << 23)) != 0;
    const bool writeback = (ins & (1u << 21)) != 0;
    const bool is_load = (ins & (1u << 20)) != 0;
    const std::uint32_t rn = (ins >> 16) & 0xF;
    const std::uint16_t rlist = ins & 0xFFFF;
    std::uint32_t addr = state.regs[rn];

    // Count registers in list
    int reg_count = 0;
    for (int i = 0; i < 16; ++i) {
      if (rlist & (1 << i)) ++reg_count;
    }

    if (!is_add) {
      addr -= reg_count * 4;
      if (writeback) state.regs[rn] = addr;
      // For decrement, we still transfer in ascending order from the lowest address
      // DA: start at addr+4, DB: start at addr
      if (is_pre) {
        // Already at right position
      } else {
        addr += 4;
      }
      // Transfer ascending from addr
      for (int i = 0; i < 16; ++i) {
        if (rlist & (1 << i)) {
          if (is_load) {
            state.regs[i] = read32(state.memory, addr);
            if (i == 15) {
              state.thumb = (state.regs[15] & 1) != 0;
              state.regs[15] &= ~1u;
            }
          } else {
            write32(state.memory, addr, state.regs[i]);
          }
          addr += 4;
        }
      }
    } else {
      // Ascending (IA/IB)
      for (int i = 0; i < 16; ++i) {
        if (rlist & (1 << i)) {
          if (is_pre) addr += 4;
          if (is_load) {
            state.regs[i] = read32(state.memory, addr);
            if (i == 15) {
              state.thumb = (state.regs[15] & 1) != 0;
              state.regs[15] &= ~1u;
            }
          } else {
            write32(state.memory, addr, state.regs[i]);
          }
          if (!is_pre) addr += 4;
        }
      }
      if (writeback) state.regs[rn] = addr;
    }
    return 1;
  }

  // Software Interrupt (SWI)
  if ((ins & 0x0F000000) == 0x0F000000) {
    const std::uint8_t swi_num = (ins >> 16) & 0xFF;
    if (swi_num == 0x04 || swi_num == 0x05) {
      const std::uint16_t irq_mask = (swi_num == 0x05) ? 0x0001 : static_cast<std::uint16_t>(state.regs[1]);
      const bool reset_flags = (swi_num == 0x04) ? (state.regs[0] != 0) : true;
      const std::uint16_t bios_flags = read16(state.memory, 0x03007FF8);

      if (bios_flags & irq_mask) {
        if (reset_flags) {
          write16(state.memory, 0x03007FF8, bios_flags & ~irq_mask);
        }
        state.halted = false;
      } else {
        state.cpsr &= ~(1u << 7); // Enable IRQs in CPSR
        state.halted = true;
        state.halt_irq_mask = irq_mask;
      }
    } else if (swi_num == 0x02) {
      state.cpsr &= ~(1u << 7);
      state.halted = true;
      state.halt_irq_mask = 0xFFFF;
    } else if (swi_num == 0x0B || swi_num == 0x0C) {
      const std::uint32_t src = state.regs[0];
      const std::uint32_t dst = state.regs[1];
      const std::uint32_t cnt_reg = state.regs[2];
      const std::size_t count = (cnt_reg & 0x001FFFFF);
      const bool is_fixed = (cnt_reg & (1u << 24)) != 0;
      const bool is_32bit = (swi_num == 0x0C) || ((cnt_reg & (1u << 26)) != 0);

      if (is_32bit) {
        std::uint32_t val = is_fixed ? read32(state.memory, src) : 0;
        for (std::size_t i = 0; i < count; ++i) {
          if (!is_fixed) val = read32(state.memory, src + i * 4);
          write32(state.memory, dst + i * 4, val);
        }
      } else {
        std::uint16_t val = is_fixed ? read16(state.memory, src) : 0;
        for (std::size_t i = 0; i < count; ++i) {
          if (!is_fixed) val = read16(state.memory, src + i * 2);
          write16(state.memory, dst + i * 2, val);
        }
      }
    }
    return 1;
  }

  return 1;
}

}  // namespace

std::uint32_t step_interpreter(CpuState& state) {
  if (state.thumb) {
    return step_thumb(state);
  } else {
    return step_arm(state);
  }
}

}  // namespace aw
