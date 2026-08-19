#include "aw/memory.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

#include "aw/hardware.hpp"

namespace aw {
namespace {

constexpr std::uint32_t kBiosBase  = 0x00000000;
constexpr std::uint32_t kBiosEnd   = 0x02000000;
constexpr std::uint32_t kEwramBase = 0x02000000;
constexpr std::uint32_t kEwramEnd  = 0x03000000;
constexpr std::uint32_t kIwramBase = 0x03000000;
constexpr std::uint32_t kIwramEnd  = 0x04000000;
constexpr std::uint32_t kIoBase    = 0x04000000;
constexpr std::uint32_t kIoEnd     = 0x05000000;
constexpr std::uint32_t kPramBase  = 0x05000000;
constexpr std::uint32_t kPramEnd   = 0x06000000;
constexpr std::uint32_t kVramBase  = 0x06000000;
constexpr std::uint32_t kVramEnd   = 0x07000000;
constexpr std::uint32_t kOamBase   = 0x07000000;
constexpr std::uint32_t kOamEnd    = 0x08000000;
constexpr std::uint32_t kRomBase   = 0x08000000;
constexpr std::uint32_t kRomEnd    = 0x0E000000;
constexpr std::uint32_t kSramBase  = 0x0E000000;
constexpr std::uint32_t kSramEnd   = 0x10000000;

template <std::size_t Size>
std::uint16_t read_le16(const std::array<std::uint8_t, Size>& bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(bytes[offset]) |
      (static_cast<std::uint16_t>(bytes[offset + 1]) << 8));
}

template <std::size_t Size>
std::uint32_t read_le32(const std::array<std::uint8_t, Size>& bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

template <std::size_t Size>
void write_le16(std::array<std::uint8_t, Size>& bytes, std::size_t offset, std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xFF);
  bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

template <std::size_t Size>
void write_le32(std::array<std::uint8_t, Size>& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xFF);
  bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
  bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
  bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

std::size_t vram_offset(std::uint32_t offset) {
  offset %= (128 * 1024);
  if (offset >= 96 * 1024) {
    offset -= 32 * 1024;
  }
  return offset;
}

}  // namespace

Memory::Memory() {
  // Fill BIOS with NOP/return stubs (Thumb `bx lr`)
  for (std::size_t i = 0; i < bios.size(); i += 2) {
    write_le16(bios, i, 0x4770); // Thumb `bx lr`
  }

  // ARM IRQ vector at 0x18: B to IRQ handler at 0x128
  // offset = (0x128 - 0x18 - 8) >> 2 = 0x42
  write_le32(bios, 0x0018, 0xEA000042u);

  // BIOS IRQ handler at 0x128 — mirrors real GBA BIOS behavior:
  // 1. Save regs, read IE&IF, acknowledge, update BIOS flags, call user handler, return
  //
  // 0x128: STMDB SP!, {R0-R3, R12, LR}
  write_le32(bios, 0x0128, 0xE92D500Fu);
  // 0x12C: MOV R12, #0x04000000
  write_le32(bios, 0x012C, 0xE3A0C301u);
  // 0x130: ADD R12, R12, #0x200
  write_le32(bios, 0x0130, 0xE28CCC02u);
  // 0x134: LDR R0, [R12]         ; R0 = IE | (IF << 16)  (32-bit read at 0x04000200)
  write_le32(bios, 0x0134, 0xE59C0000u);
  // 0x138: AND R1, R0, R0, LSR #16 ; R1 = IE & IF (acknowledged interrupts)
  write_le32(bios, 0x0138, 0xE0001820u);
  // 0x13C: STRH R1, [R12, #2]    ; Write to IF (0x04000202) to acknowledge
  write_le32(bios, 0x013C, 0xE1C110B2u);
  // 0x140: MOV R2, #0x03000000
  write_le32(bios, 0x0140, 0xE3A02203u);
  // 0x144: ADD R2, R2, #0x7F00   ; R2 = 0x03007F00
  write_le32(bios, 0x0144, 0xE2822C7Fu);
  // 0x148: LDR R3, [R2, #0xF8]   ; R3 = [0x03007FF8] (BIOS IRQ flags)
  write_le32(bios, 0x0148, 0xE59230F8u);
  // 0x14C: ORR R3, R3, R1        ; Set flags for serviced interrupts
  write_le32(bios, 0x014C, 0xE1833001u);
  // 0x150: STR R3, [R2, #0xF8]   ; Write back BIOS IRQ flags
  write_le32(bios, 0x0150, 0xE58230F8u);
  // 0x154: LDR R0, [R2, #0xFC]   ; R0 = [0x03007FFC] (user IRQ handler)
  write_le32(bios, 0x0154, 0xE59200FCu);
  // 0x158: CMP R0, #0            ; Check if user handler is non-null
  write_le32(bios, 0x0158, 0xE3500000u);
  // 0x15C: MOVNE LR, PC          ; Set return address if non-null
  write_le32(bios, 0x015C, 0x11A0E00Fu);
  // 0x160: BXNE R0               ; Call user handler if non-null
  write_le32(bios, 0x0160, 0x112FFF10u);
  // 0x164: LDMIA SP!, {R0-R3, R12, LR}
  write_le32(bios, 0x0164, 0xE8BD500Fu);
  // 0x168: SUBS PC, LR, #4       ; Return from IRQ (restores CPSR from SPSR)
  write_le32(bios, 0x0168, 0xE25EF004u);

  // Initialize SOUNDCNT_X (0x04000084) bit 7 = 1 (Master sound enabled)
  write_le16(io, 0x0084, 0x0080);
  // Initialize KEYINPUT (0x04000130) = 0x03FF (All keys released)
  write_le16(io, 0x0130, 0x03FF);
}



std::uint32_t read32(const Memory& memory, std::uint32_t address) {
  if (address >= kBiosBase && address < kBiosEnd) {
    const std::size_t off = (address - kBiosBase) % memory.bios.size();
    return read_le32(memory.bios, off);
  }
  if (address >= kEwramBase && address < kEwramEnd) {
    const std::size_t off = (address - kEwramBase) % memory.ewram.size();
    return read_le32(memory.ewram, off);
  }
  if (address >= kIwramBase && address < kIwramEnd) {
    const std::size_t off = (address - kIwramBase) % memory.iwram.size();
    return read_le32(memory.iwram, off);
  }
  if (address >= kIoBase && address < kIoEnd) {
    const std::size_t off = (address - kIoBase) % memory.io.size();
    return read_le32(memory.io, off);
  }
  if (address >= kPramBase && address < kPramEnd) {
    const std::size_t off = (address - kPramBase) % memory.pram.size();
    return read_le32(memory.pram, off);
  }
  if (address >= kVramBase && address < kVramEnd) {
    const std::size_t off = vram_offset(address - kVramBase);
    return read_le32(memory.vram, off);
  }
  if (address >= kOamBase && address < kOamEnd) {
    const std::size_t off = (address - kOamBase) % memory.oam.size();
    return read_le32(memory.oam, off);
  }
  if (address >= kRomBase && address < kRomEnd) {
    if (memory.rom_data != nullptr && memory.rom_size > 0) {
      const std::size_t off = (address - kRomBase) % memory.rom_size;
      if (off + 4 <= memory.rom_size) {
        return static_cast<std::uint32_t>(memory.rom_data[off]) |
               (static_cast<std::uint32_t>(memory.rom_data[off + 1]) << 8) |
               (static_cast<std::uint32_t>(memory.rom_data[off + 2]) << 16) |
               (static_cast<std::uint32_t>(memory.rom_data[off + 3]) << 24);
      }
    }
    return 0;
  }
  if (address >= kSramBase && address < kSramEnd) {
    const std::size_t off = (address - kSramBase) % memory.sram.size();
    if (memory.flash_id_mode) {
      if (off == 0) return 0x000009C2;
    }
    return read_le32(memory.sram, off);
  }
  return 0;
}

std::uint16_t read16(const Memory& memory, std::uint32_t address) {
  if (address >= kBiosBase && address < kBiosEnd) {
    const std::size_t off = (address - kBiosBase) % memory.bios.size();
    return read_le16(memory.bios, off);
  }
  if (address >= kEwramBase && address < kEwramEnd) {
    const std::size_t off = (address - kEwramBase) % memory.ewram.size();
    return read_le16(memory.ewram, off);
  }
  if (address >= kIwramBase && address < kIwramEnd) {
    const std::size_t off = (address - kIwramBase) % memory.iwram.size();
    return read_le16(memory.iwram, off);
  }
  if (address >= kIoBase && address < kIoEnd) {
    const std::size_t off = (address - kIoBase) % memory.io.size();
    return read_le16(memory.io, off);
  }
  if (address >= kPramBase && address < kPramEnd) {
    const std::size_t off = (address - kPramBase) % memory.pram.size();
    return read_le16(memory.pram, off);
  }
  if (address >= kVramBase && address < kVramEnd) {
    const std::size_t off = vram_offset(address - kVramBase);
    return read_le16(memory.vram, off);
  }
  if (address >= kOamBase && address < kOamEnd) {
    const std::size_t off = (address - kOamBase) % memory.oam.size();
    return read_le16(memory.oam, off);
  }
  if (address >= kRomBase && address < kRomEnd) {
    if (memory.rom_data != nullptr && memory.rom_size > 0) {
      const std::size_t off = (address - kRomBase) % memory.rom_size;
      if (off + 2 <= memory.rom_size) {
        return static_cast<std::uint16_t>(memory.rom_data[off]) |
               (static_cast<std::uint16_t>(memory.rom_data[off + 1]) << 8);
      }
    }
    return 0;
  }
  if (address >= kSramBase && address < kSramEnd) {
    const std::size_t off = (address - kSramBase) % memory.sram.size();
    if (memory.flash_id_mode) {
      if (off == 0) return 0x09C2;
    }
    return read_le16(memory.sram, off);
  }
  return 0;
}

std::uint8_t read8(const Memory& memory, std::uint32_t address) {
  if (address >= kBiosBase && address < kBiosEnd) {
    return memory.bios[(address - kBiosBase) % memory.bios.size()];
  }
  if (address >= kEwramBase && address < kEwramEnd) {
    return memory.ewram[(address - kEwramBase) % memory.ewram.size()];
  }
  if (address >= kIwramBase && address < kIwramEnd) {
    return memory.iwram[(address - kIwramBase) % memory.iwram.size()];
  }
  if (address >= kIoBase && address < kIoEnd) {
    return memory.io[(address - kIoBase) % memory.io.size()];
  }
  if (address >= kPramBase && address < kPramEnd) {
    return memory.pram[(address - kPramBase) % memory.pram.size()];
  }
  if (address >= kVramBase && address < kVramEnd) {
    return memory.vram[vram_offset(address - kVramBase)];
  }
  if (address >= kOamBase && address < kOamEnd) {
    return memory.oam[(address - kOamBase) % memory.oam.size()];
  }
  if (address >= kRomBase && address < kRomEnd) {
    if (memory.rom_data != nullptr && memory.rom_size > 0) {
      const std::size_t off = (address - kRomBase) % memory.rom_size;
      return memory.rom_data[off];
    }
    return 0;
  }
  if (address >= kSramBase && address < kSramEnd) {
    const std::size_t off = (address - kSramBase) % memory.sram.size();
    if (memory.flash_id_mode) {
      if (off == 0) return 0xC2; // Macronix Flash Manufacturer ID
      if (off == 1) return 0x09; // 64K Flash Device ID
    }
    return memory.sram[off];
  }
  return 0;
}

void write8(Memory& memory, std::uint32_t address, std::uint8_t value) {
  if (address >= kEwramBase && address < kEwramEnd) {
    memory.ewram[(address - kEwramBase) % memory.ewram.size()] = value;
    return;
  }
  if (address >= kIwramBase && address < kIwramEnd) {
    memory.iwram[(address - kIwramBase) % memory.iwram.size()] = value;
    return;
  }
  if (address >= kIoBase && address < kIoEnd) {
    memory.io[(address - kIoBase) % memory.io.size()] = value;
    return;
  }
  if (address >= kPramBase && address < kPramEnd) {
    const std::size_t off = (address - kPramBase) % memory.pram.size();
    memory.pram[off & ~1] = value;
    memory.pram[off | 1] = value;
    return;
  }
  if (address >= kVramBase && address < kVramEnd) {
    const std::size_t off = vram_offset(address - kVramBase);
    memory.vram[off & ~1] = value;
    memory.vram[off | 1] = value;
    return;
  }
  if (address >= kOamBase && address < kOamEnd) {
    return;
  }
  if (address >= kSramBase && address < kSramEnd) {
    const std::size_t off = (address - kSramBase) % memory.sram.size();
    if (value == 0x90 && off == 0x5555) {
      memory.flash_id_mode = true;
    } else if (value == 0xF0) {
      memory.flash_id_mode = false;
    }
    memory.sram[off] = value;
    return;
  }
}

void write16(Memory& memory, std::uint32_t address, std::uint16_t value) {
  if (address >= kEwramBase && address < kEwramEnd) {
    write_le16(memory.ewram, (address - kEwramBase) % memory.ewram.size(), value);
    return;
  }
  if (address >= kIwramBase && address < kIwramEnd) {
    write_le16(memory.iwram, (address - kIwramBase) % memory.iwram.size(), value);
    return;
  }
  if (address >= kIoBase && address < kIoEnd) {
    if (address == 0x04000202) {
      const std::uint16_t old_if = read_le16(memory.io, 0x0202);
      write_le16(memory.io, 0x0202, old_if & ~value);
      return;
    }
    write_le16(memory.io, (address - kIoBase) % memory.io.size(), value);
    return;
  }
  if (address >= kPramBase && address < kPramEnd) {
    write_le16(memory.pram, (address - kPramBase) % memory.pram.size(), value);
    return;
  }
  if (address >= kVramBase && address < kVramEnd) {
    write_le16(memory.vram, vram_offset(address - kVramBase), value);
    return;
  }
  if (address >= kOamBase && address < kOamEnd) {
    write_le16(memory.oam, (address - kOamBase) % memory.oam.size(), value);
    return;
  }
  if (address >= kSramBase && address < kSramEnd) {
    write_le16(memory.sram, (address - kSramBase) % memory.sram.size(), value);
    return;
  }
}

void write32(Memory& memory, std::uint32_t address, std::uint32_t value) {
  if (address >= kEwramBase && address < kEwramEnd) {
    write_le32(memory.ewram, (address - kEwramBase) % memory.ewram.size(), value);
    return;
  }
  if (address >= kIwramBase && address < kIwramEnd) {
    write_le32(memory.iwram, (address - kIwramBase) % memory.iwram.size(), value);
    return;
  }
  if (address >= kIoBase && address < kIoEnd) {
    if (address == 0x04000200) {
      const std::uint16_t ie_val = static_cast<std::uint16_t>(value & 0xFFFF);
      const std::uint16_t if_ack = static_cast<std::uint16_t>(value >> 16);
      write_le16(memory.io, 0x0200, ie_val);
      const std::uint16_t old_if = read_le16(memory.io, 0x0202);
      write_le16(memory.io, 0x0202, old_if & ~if_ack);
      return;
    }
    write_le32(memory.io, (address - kIoBase) % memory.io.size(), value);
    return;
  }
  if (address >= kPramBase && address < kPramEnd) {
    write_le32(memory.pram, (address - kPramBase) % memory.pram.size(), value);
    return;
  }
  if (address >= kVramBase && address < kVramEnd) {
    write_le32(memory.vram, vram_offset(address - kVramBase), value);
    return;
  }
  if (address >= kOamBase && address < kOamEnd) {
    write_le32(memory.oam, (address - kOamBase) % memory.oam.size(), value);
    return;
  }
  if (address >= kSramBase && address < kSramEnd) {
    write_le32(memory.sram, (address - kSramBase) % memory.sram.size(), value);
    return;
  }
}

}  // namespace aw
