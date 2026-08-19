#!/usr/bin/env python3
import argparse
from pathlib import Path


ROM_BASE = 0x08000000
RESET_PC = 0x08000000
ENTRY_PC = 0x080000C0
THUMB_DISPLAY_ZERO_HELPER_PC = 0x08010444
THUMB_DISPLAY_SHORT_ZERO_HELPER_PC = 0x080104B0
THUMB_DISPLAY_FLAG_RESET_HELPER_PC = 0x080104D4
THUMB_REGISTER_RESET_PC = 0x08010544
THUMB_REGISTER_RESET_AFTER_ZERO_PC = 0x0801055A
THUMB_REGISTER_RESET_AFTER_SECOND_ZERO_PC = 0x0801055E
THUMB_REGISTER_RESET_RETURN_PC = 0x08010562
THUMB_REGISTER_SYNC_PC = 0x08010574
THUMB_SECOND_REGISTER_SYNC_PC = 0x080106A4
THUMB_DISPLAY_RESET_WRAPPER_RETURN_PC = 0x0801096E
THUMB_INPUT_RESET_WRAPPER_PC = 0x08010974
THUMB_INPUT_RESET_AFTER_REGISTER_SYNC_PC = 0x0801097A
THUMB_INPUT_RESET_TAIL_PC = 0x0801097E
THUMB_SEED_STORE_PC = 0x08010A78
THUMB_ALLOC_INIT_PC = 0x08012CF0
THUMB_GLOBAL_INIT_PC = 0x0801A768
THUMB_GLOBAL_INIT_AFTER_SAVE_PC = 0x0801A784
THUMB_GLOBAL_INIT_RETURN_PC = 0x0801A78A
THUMB_SAVE_PROBE_PC = 0x0801AFC8
THUMB_TABLE_INIT_PC = 0x0801B2B8
THUMB_OBJECT_STATE_CHECK_PC = 0x0801AC1C
THUMB_STATE_QUERY_PC = 0x0801AD38
THUMB_OBJECT_COPY_PC = 0x08015D1C
THUMB_OBJECT_SETUP_WRAPPER_PC = 0x08015FB4
THUMB_OBJECT_SETUP_CONTINUATION_PC = 0x08015FC0
THUMB_OBJECT_SETUP_RETURN_PC = 0x08015FCA
THUMB_SLOT_INIT_PC = 0x0803F87C
THUMB_SLOT_LOOP_CONTINUATION_PC = 0x0803F886
THUMB_SLOT_INIT_RETURN_PC = 0x0803F890
THUMB_SLOT_HELPER_PC = 0x0803F898
THUMB_SLOT_HELPER_AFTER_QUERY_PC = 0x0803F8B0
THUMB_ENTRY_PC = 0x0807AD10
THUMB_ENTRY_SECOND_CALL_PC = 0x0807AD16
THUMB_HELPER_PC = 0x0807AD00
THUMB_HELPER_CONTINUATION_PC = 0x0807AD0A
THUMB_OUTER_RETURN_PC = 0x0807AD0E
THUMB_INIT_COPY_PC = 0x0807ACE8
THUMB_COPY_RETURN_PC = 0x0807ACF4
THUMB_HARDWARE_HELPER_PC = 0x0807AE60
THUMB_STRUCT_INIT_PC = 0x0807AFF4
THUMB_LARGE_BOOT_PC = 0x080386E4
THUMB_LARGE_BOOT_AFTER_DISPLAY_PC = 0x0803872A
THUMB_LARGE_BOOT_ALLOC_RESULT_PC = 0x08038734
THUMB_LARGE_BOOT_AFTER_GLOBAL_INIT_PC = 0x08038750
THUMB_LARGE_BOOT_AFTER_OBJECT_SETUP_PC = 0x08038754
THUMB_LARGE_BOOT_AFTER_SLOT_INIT_PC = 0x08038758
THUMB_LARGE_BOOT_AFTER_SEED_PC = 0x0803875E
THUMB_LARGE_BOOT_AFTER_DISPLAY_RESET_PC = 0x08038762
THUMB_LARGE_BOOT_AFTER_INPUT_RESET_PC = 0x08038766
THUMB_LARGE_BOOT_DEFAULT_STATE_PC = 0x0803876E
THUMB_LARGE_BOOT_SAVE_PATH_PC = 0x080387FC
THUMB_REFRESH_INIT_WRAPPER_PC = 0x080386B4
THUMB_SYSTEM_INIT_PC = 0x080385CC
THUMB_ENGINE_CLEAR_PC = 0x0800F1B0
THUMB_ENGINE_REGISTER_RESET_PC = 0x0800F170
THUMB_ENGINE_CLEAR_AFTER_REGISTER_RESET_PC = 0x0800F1B6
THUMB_ENGINE_CLEAR_RETURN_PC = 0x0800F1CA
THUMB_SYSTEM_INIT_AFTER_ENGINE_CLEAR_PC = 0x080385D8
THUMB_STATE_STORE_FIRST_PC = 0x08038260
THUMB_SYSTEM_INIT_AFTER_FIRST_STATE_STORE_PC = 0x080385DE
THUMB_STATE_STORE_SECOND_PC = 0x0803826C
THUMB_SYSTEM_INIT_CLEAR_GLOBALS_PC = 0x080385E4
THUMB_SYSTEM_INIT_LOCAL_RESET_PC = 0x080385BC
THUMB_SYSTEM_INIT_DMA_COPY_CALL_PC = 0x08038600
THUMB_DMA_SETUP_PC = 0x0801B820
THUMB_DMA_SETUP_RETURN_PC = 0x0801B840
THUMB_SYSTEM_INIT_NEXT_CALL_PC = 0x08038604
THUMB_NEXT_INITIALIZER_PC = 0x08034C98
THUMB_BYTE_RESET_HELPER_PC = 0x08018AAC
THUMB_NEXT_INITIALIZER_AFTER_RESET_PC = 0x08034CAC
THUMB_TABLE_COPY_HELPER_PC = 0x0801F114
THUMB_NEXT_INITIALIZER_RETURN_PC = 0x08034CB0
THUMB_SYSTEM_INIT_MODE_CALL_PC = 0x08038608
THUMB_MODE_INITIALIZER_PC = 0x08034C74
THUMB_DEFAULTS_INITIALIZER_PC = 0x08034BA8
THUMB_ORDER_HELPER_PC = 0x08024EC4
THUMB_DEFAULTS_AFTER_ORDER_PC = 0x08034BFA
THUMB_MODE_INITIALIZER_AFTER_DEFAULTS_PC = 0x08034C86
THUMB_MODE_CLEAR_HELPER_PC = 0x08034C64
THUMB_MODE_INITIALIZER_RETURN_PC = 0x08034C8A
THUMB_SYSTEM_INIT_UNIT_CALL_PC = 0x0803860C
THUMB_UNIT_INITIALIZER_PC = 0x080149E0
THUMB_GLOBAL_TABLE_RESET_PC = 0x0801A69C
THUMB_UNIT_INITIALIZER_AFTER_RESET_PC = 0x080149EC
THUMB_UNIT_ENTRY_SETUP_PC = 0x08014C98
THUMB_UNIT_GEOMETRY_HELPER_PC = 0x08014BF8
THUMB_FIRST_MATH_WRAPPER_PC = 0x0807AF30
THUMB_TRIG_LOOKUP_PC = 0x0807AED4
THUMB_FIRST_MATH_RETURN_PC = 0x0807AF38
THUMB_UNIT_GEOMETRY_AFTER_FIRST_MATH_PC = 0x08014C14
THUMB_SIGNED_DIVIDE_HELPER_PC = 0x0807B488
THUMB_UNIT_GEOMETRY_AFTER_FIRST_DIVIDE_PC = 0x08014C20
THUMB_BIOS_CPUSET_BOOT_PC = 0x080796C4
THUMB_DISPLAY_RESET_WRAPPER_PC = 0x08010968
THUMB_DISPLAY_SETUP_PC = 0x0807AE14
THUMB_DISPLAY_SETUP_CONTINUATION_PC = 0x0807AE1E
THUMB_DISPLAY_SECOND_DMA_PC = 0x0807AE28
THUMB_DISPLAY_SETUP_TAIL_PC = 0x0807AE36
THUMB_CALLBACK_REGISTER_PC = 0x0807AE50
THUMB_DMA_HELPER_PC = 0x0807B2DC


def read_u32(data, offset):
    return int.from_bytes(data[offset:offset + 4], "little")


def read_u16(data, offset):
    return int.from_bytes(data[offset:offset + 2], "little")


def rom_word(data, address):
    offset = address - ROM_BASE
    if offset < 0 or offset + 4 > len(data):
        raise ValueError(f"address 0x{address:08X} is outside the ROM")
    return read_u32(data, offset)


def rom_halfword(data, address):
    offset = address - ROM_BASE
    if offset < 0 or offset + 2 > len(data):
        raise ValueError(f"address 0x{address:08X} is outside the ROM")
    return read_u16(data, offset)


def arm_branch_target(instruction, pc):
    if instruction & 0x0E000000 != 0x0A000000:
        raise ValueError(f"reset instruction 0x{instruction:08X} is not ARM B/BL")
    imm24 = instruction & 0x00FFFFFF
    offset = imm24 << 2
    if imm24 & 0x00800000:
        offset |= 0xFC000000
    return (pc + 8 + offset) & 0xFFFFFFFF


def arm_immediate(imm12):
    imm8 = imm12 & 0xFF
    rotate = ((imm12 >> 8) & 0xF) * 2
    if rotate == 0:
        return imm8
    return ((imm8 >> rotate) | (imm8 << (32 - rotate))) & 0xFFFFFFFF


def reg_name(index):
    names = {
        13: "sp",
        14: "lr",
        15: "pc",
    }
    return names.get(index, f"r{index}")


def hex32(value):
    return f"0x{value:08X}"


def decode_entry_block(data):
    reset = rom_word(data, RESET_PC)
    entry = arm_branch_target(reset, RESET_PC)
    if entry != ENTRY_PC:
        raise ValueError(f"expected reset branch to 0x{ENTRY_PC:08X}, got 0x{entry:08X}")

    pc = entry
    ops = []
    while True:
        instruction = rom_word(data, pc)
        cond = (instruction >> 28) & 0xF
        if cond != 0xE:
            raise ValueError(f"unsupported condition at 0x{pc:08X}: 0x{cond:X}")

        if instruction & 0x0FE00000 == 0x03A00000:
            rd = (instruction >> 12) & 0xF
            value = arm_immediate(instruction & 0xFFF)
            ops.append(("mov_imm", pc, rd, value))
        elif instruction == 0xE129F000:
            ops.append(("msr_cpsr_fc_r0", pc))
        elif instruction & 0x0FFF0000 == 0x059F0000:
            rd = (instruction >> 12) & 0xF
            imm = instruction & 0xFFF
            literal_address = pc + 8 + imm
            value = rom_word(data, literal_address)
            ops.append(("ldr_literal", pc, rd, literal_address, value))
        elif instruction == 0xE1A0E00F:
            ops.append(("mov_lr_pc", pc, pc + 8))
        elif instruction & 0x0FFFFFF0 == 0x012FFF10:
            rm = instruction & 0xF
            ops.append(("bx", pc, rm))
            break
        else:
            raise ValueError(f"unsupported instruction at 0x{pc:08X}: 0x{instruction:08X}")
        pc += 4

    return ops


def thumb_bl_target(first, second, pc):
    if first & 0xF800 != 0xF000 or second & 0xF800 != 0xF800:
        raise ValueError(f"invalid Thumb BL pair: 0x{first:04X} 0x{second:04X}")
    s = (first >> 10) & 1
    imm10 = first & 0x03FF
    j1 = (second >> 13) & 1
    j2 = (second >> 11) & 1
    imm11 = second & 0x07FF
    i1 = (~(j1 ^ s)) & 1
    i2 = (~(j2 ^ s)) & 1
    offset = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
    if s:
        offset |= 0xFE000000
    return (pc + 4 + offset) & 0xFFFFFFFF


def decode_thumb_entry_block(data):
    pc = THUMB_ENTRY_PC
    ops = []

    first = rom_halfword(data, pc)
    if first != 0xB500:
        raise ValueError(f"expected push {{lr}} at 0x{pc:08X}, got 0x{first:04X}")
    ops.append(("thumb_push_lr", pc))
    pc += 2

    first = rom_halfword(data, pc)
    second = rom_halfword(data, pc + 2)
    target = thumb_bl_target(first, second, pc) | 1
    ops.append(("thumb_bl", pc, target, pc + 5))
    return ops


def decode_thumb_entry_second_call_block(data):
    pc = THUMB_ENTRY_SECOND_CALL_PC
    first = rom_halfword(data, pc)
    second = rom_halfword(data, pc + 2)
    target = thumb_bl_target(first, second, pc) | 1
    return [("thumb_bl", pc, target, pc + 5)]


def decode_thumb_alloc_init_block(data):
    expected = {
        0x08012CC4: 0x1C02,
        0x08012CC6: 0x1C0B,
        0x08012CC8: 0x2B1F,
        0x08012CCA: 0xD90D,
        0x08012CCC: 0x300F,
        0x08012CCE: 0x2110,
        0x08012CD0: 0x4249,
        0x08012CD2: 0x4008,
        0x08012CD4: 0x1A81,
        0x08012CD6: 0x1A5B,
        0x08012CD8: 0x2200,
        0x08012CDA: 0x6002,
        0x08012CDC: 0x1C19,
        0x08012CDE: 0x3910,
        0x08012CE0: 0x6041,
        0x08012CE2: 0x6082,
        0x08012CE4: 0x60C3,
        0x08012CE6: 0xE001,
        0x08012CE8: 0x2001,
        0x08012CEA: 0x4240,
        0x08012CEC: 0x4770,
        0x08012CF0: 0xB510,
        0x08012CF2: 0x4C05,
        0x08012CF4: 0xF7FF,
        0x08012CF6: 0xFFE6,
        0x08012CF8: 0x1C01,
        0x08012CFA: 0x6021,
        0x08012CFC: 0x2001,
        0x08012CFE: 0x4240,
        0x08012D00: 0x4281,
        0x08012D02: 0xD003,
        0x08012D04: 0x2000,
        0x08012D06: 0xE002,
        0x08012D0C: 0x1C08,
        0x08012D0E: 0xBC10,
        0x08012D10: 0xBC02,
        0x08012D12: 0x4708,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_alloc_init",
        THUMB_ALLOC_INIT_PC,
        rom_word(data, 0x08012D08),
        thumb_bl_target(rom_halfword(data, 0x08012CF4), rom_halfword(data, 0x08012CF6), 0x08012CF4) | 1,
    )]


def decode_thumb_seed_store_block(data):
    expected = {
        0x08010A78: 0x4901,
        0x08010A7A: 0x6008,
        0x08010A7C: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_seed_store", THUMB_SEED_STORE_PC, rom_word(data, 0x08010A80))]


def decode_thumb_register_reset_block(data):
    expected = {
        0x08010544: 0xB500,
        0x08010546: 0x4908,
        0x08010548: 0x2080,
        0x0801054A: 0x8008,
        0x0801054C: 0x4807,
        0x0801054E: 0x2100,
        0x08010550: 0x8001,
        0x08010552: 0x4807,
        0x08010554: 0x8001,
        0x08010556: 0xF7FF,
        0x08010558: 0xFF75,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08010556), rom_halfword(data, 0x08010558), 0x08010556) | 1
    return [(
        "thumb_register_reset",
        THUMB_REGISTER_RESET_PC,
        rom_word(data, 0x08010568),
        rom_word(data, 0x0801056C),
        rom_word(data, 0x08010570),
        target,
    )]


def decode_thumb_display_zero_helper_block(data):
    expected = {
        0x08010444: 0x480D,
        0x08010446: 0x2100,
        0x08010448: 0x8001,
        0x0801044A: 0x480D,
        0x0801044C: 0x8001,
        0x0801044E: 0x480D,
        0x08010450: 0x8001,
        0x08010452: 0x480D,
        0x08010454: 0x8001,
        0x08010456: 0x480D,
        0x08010458: 0x8001,
        0x0801045A: 0x480D,
        0x0801045C: 0x8001,
        0x0801045E: 0x480D,
        0x08010460: 0x8001,
        0x08010462: 0x480D,
        0x08010464: 0x8001,
        0x08010466: 0x480D,
        0x08010468: 0x8001,
        0x0801046A: 0x480D,
        0x0801046C: 0x8001,
        0x0801046E: 0x480D,
        0x08010470: 0x8001,
        0x08010472: 0x480D,
        0x08010474: 0x8001,
        0x08010476: 0x480D,
        0x08010478: 0x8001,
        0x0801047A: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_display_zero_helper",
        THUMB_DISPLAY_ZERO_HELPER_PC,
        [rom_word(data, address) for address in range(0x0801047C, 0x080104B0, 4)],
    )]


def decode_thumb_display_short_zero_helper_block(data):
    expected = {
        0x080104B0: 0x4804,
        0x080104B2: 0x2100,
        0x080104B4: 0x8001,
        0x080104B6: 0x4804,
        0x080104B8: 0x8001,
        0x080104BA: 0x4804,
        0x080104BC: 0x8001,
        0x080104BE: 0x4804,
        0x080104C0: 0x8001,
        0x080104C2: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_display_short_zero_helper",
        THUMB_DISPLAY_SHORT_ZERO_HELPER_PC,
        [rom_word(data, address) for address in range(0x080104C4, 0x080104D4, 4)],
    )]


def decode_thumb_display_flag_reset_helper_block(data):
    expected = {
        0x080104D4: 0x4A10,
        0x080104D6: 0x2021,
        0x080104D8: 0x4240,
        0x080104DA: 0x7851,
        0x080104DC: 0x4008,
        0x080104DE: 0x2141,
        0x080104E0: 0x4249,
        0x080104E2: 0x4008,
        0x080104E4: 0x217F,
        0x080104E6: 0x4008,
        0x080104E8: 0x7050,
        0x080104EA: 0x480C,
        0x080104EC: 0x2100,
        0x080104EE: 0x7001,
        0x080104F0: 0x480B,
        0x080104F2: 0x7001,
        0x080104F4: 0x480B,
        0x080104F6: 0x7001,
        0x080104F8: 0x480B,
        0x080104FA: 0x7001,
        0x080104FC: 0x480B,
        0x080104FE: 0x7001,
        0x08010500: 0x480B,
        0x08010502: 0x7001,
        0x08010504: 0x480B,
        0x08010506: 0x7001,
        0x08010508: 0x480B,
        0x0801050A: 0x7001,
        0x0801050C: 0x480B,
        0x0801050E: 0x2100,
        0x08010510: 0x8001,
        0x08010512: 0x480B,
        0x08010514: 0x8001,
        0x08010516: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_display_flag_reset_helper",
        THUMB_DISPLAY_FLAG_RESET_HELPER_PC,
        rom_word(data, 0x08010518),
        [rom_word(data, address) for address in range(0x0801051C, 0x0801053C, 4)],
        [rom_word(data, address) for address in range(0x0801053C, 0x08010544, 4)],
    )]


def decode_thumb_register_reset_after_zero_block(data):
    expected = {
        0x0801055A: 0xF7FF,
        0x0801055C: 0xFFA9,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0801055A), rom_halfword(data, 0x0801055C), 0x0801055A) | 1
    return [("thumb_register_reset_after_zero", THUMB_REGISTER_RESET_AFTER_ZERO_PC, target)]


def decode_thumb_register_reset_after_second_zero_block(data):
    expected = {
        0x0801055E: 0xF7FF,
        0x08010560: 0xFFB9,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0801055E), rom_halfword(data, 0x08010560), 0x0801055E) | 1
    return [("thumb_register_reset_after_second_zero", THUMB_REGISTER_RESET_AFTER_SECOND_ZERO_PC, target)]


def decode_thumb_register_reset_return_block(data):
    expected = {
        0x08010562: 0xBC01,
        0x08010564: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_register_reset_return", THUMB_REGISTER_RESET_RETURN_PC)]


def decode_thumb_register_sync_block(data):
    expected = {
        0x08010574: 0x2180,
        0x08010576: 0x04C9,
        0x08010578: 0x4832,
        0x0801057A: 0x8800,
        0x0801057C: 0x8008,
        0x0801057E: 0x3104,
        0x08010580: 0x4831,
        0x08010582: 0x8800,
        0x08010584: 0x8008,
        0x08010586: 0x310C,
        0x08010588: 0x4830,
        0x0801058A: 0x8800,
        0x0801058C: 0x8008,
        0x0801058E: 0x3102,
        0x08010590: 0x482F,
        0x08010592: 0x8800,
        0x08010594: 0x8008,
        0x08010596: 0x3102,
        0x08010598: 0x482E,
        0x0801059A: 0x8800,
        0x0801059C: 0x8008,
        0x0801059E: 0x3102,
        0x080105A0: 0x482D,
        0x080105A2: 0x8800,
        0x080105A4: 0x8008,
        0x080105A6: 0x3102,
        0x080105A8: 0x482C,
        0x080105AA: 0x8800,
        0x080105AC: 0x8008,
        0x080105AE: 0x3102,
        0x080105B0: 0x482B,
        0x080105B2: 0x8800,
        0x080105B4: 0x8008,
        0x080105B6: 0x3102,
        0x080105B8: 0x482A,
        0x080105BA: 0x8800,
        0x080105BC: 0x8008,
        0x080105BE: 0x3102,
        0x080105C0: 0x4829,
        0x080105C2: 0x8800,
        0x080105C4: 0x8008,
        0x080105C6: 0x312E,
        0x080105C8: 0x4828,
        0x080105CA: 0x8800,
        0x080105CC: 0x8008,
        0x080105CE: 0x3944,
        0x080105D0: 0x4827,
        0x080105D2: 0x8800,
        0x080105D4: 0x8008,
        0x080105D6: 0x3102,
        0x080105D8: 0x4826,
        0x080105DA: 0x8800,
        0x080105DC: 0x8008,
        0x080105DE: 0x3102,
        0x080105E0: 0x4825,
        0x080105E2: 0x8800,
        0x080105E4: 0x8008,
        0x080105E6: 0x3102,
        0x080105E8: 0x4824,
        0x080105EA: 0x8800,
        0x080105EC: 0x8008,
        0x080105EE: 0x3142,
        0x080105F0: 0x4823,
        0x080105F2: 0x8800,
        0x080105F4: 0x8008,
        0x080105F6: 0x4B23,
        0x080105F8: 0x4A23,
        0x080105FA: 0x4824,
        0x080105FC: 0x8801,
        0x080105FE: 0x0209,
        0x08010600: 0x8810,
        0x08010602: 0x1840,
        0x08010604: 0x8018,
        0x08010606: 0x4922,
        0x08010608: 0x4822,
        0x0801060A: 0x8800,
        0x0801060C: 0x8008,
        0x0801060E: 0x4A22,
        0x08010610: 0x4922,
        0x08010612: 0x6808,
        0x08010614: 0x6010,
        0x08010616: 0x3204,
        0x08010618: 0x6848,
        0x0801061A: 0x6010,
        0x0801061C: 0x3204,
        0x0801061E: 0x6888,
        0x08010620: 0x6010,
        0x08010622: 0x3204,
        0x08010624: 0x68C8,
        0x08010626: 0x6010,
        0x08010628: 0x3204,
        0x0801062A: 0x491D,
        0x0801062C: 0x6808,
        0x0801062E: 0x6010,
        0x08010630: 0x3204,
        0x08010632: 0x6848,
        0x08010634: 0x6010,
        0x08010636: 0x3204,
        0x08010638: 0x6888,
        0x0801063A: 0x6010,
        0x0801063C: 0x3204,
        0x0801063E: 0x68C8,
        0x08010640: 0x6010,
        0x08010642: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    halfword_sources = [
        rom_word(data, address)
        for address in [
            0x08010644, 0x08010648, 0x0801064C, 0x08010650,
            0x08010654, 0x08010658, 0x0801065C, 0x08010660,
            0x08010664, 0x08010668, 0x0801066C, 0x08010670,
            0x08010674, 0x08010678, 0x0801067C, 0x08010680,
            0x0801068C,
        ]
    ]
    halfword_destinations = [
        0x04000000, 0x04000004, 0x04000010, 0x04000012,
        0x04000014, 0x04000016, 0x04000018, 0x0400001A,
        0x0400001C, 0x0400001E, 0x0400004C, 0x04000008,
        0x0400000A, 0x0400000C, 0x0400000E, 0x04000050,
        0x04000054,
    ]
    return [(
        "thumb_register_sync",
        THUMB_REGISTER_SYNC_PC,
        list(zip(halfword_sources, halfword_destinations)),
        rom_word(data, 0x08010684),
        rom_word(data, 0x08010688),
        rom_word(data, 0x08010690),
        rom_word(data, 0x08010694),
        rom_word(data, 0x08010698),
        rom_word(data, 0x0801069C),
        rom_word(data, 0x080106A0),
    )]


def decode_thumb_second_register_sync_block(data):
    halfword_prefix_count = 13
    byte_count = 8
    halfword_suffix_count = 23
    pc = THUMB_SECOND_REGISTER_SYNC_PC

    for _ in range(halfword_prefix_count):
        expected = (0x4958, 0x4859, 0x8800, 0x8008)
        for halfword in expected:
            actual = rom_halfword(data, pc)
            if actual != halfword:
                raise ValueError(
                    f"expected 0x{halfword:04X} at 0x{pc:08X}, got 0x{actual:04X}"
                )
            pc += 2
    for _ in range(byte_count):
        expected = (0x4958, 0x4859, 0x7800, 0x7008)
        for halfword in expected:
            actual = rom_halfword(data, pc)
            if actual != halfword:
                raise ValueError(
                    f"expected 0x{halfword:04X} at 0x{pc:08X}, got 0x{actual:04X}"
                )
            pc += 2
    for _ in range(halfword_suffix_count):
        expected = (0x4958, 0x4859, 0x8800, 0x8008)
        for halfword in expected:
            actual = rom_halfword(data, pc)
            if actual != halfword:
                raise ValueError(
                    f"expected 0x{halfword:04X} at 0x{pc:08X}, got 0x{actual:04X}"
                )
            pc += 2
    if rom_halfword(data, pc) != 0x4770:
        raise ValueError(f"expected bx lr at 0x{pc:08X}")

    literals = [rom_word(data, address) for address in range(0x08010808, 0x08010968, 4)]
    if len(literals) != 88:
        raise ValueError("unexpected second register sync literal count")

    first_halfword_pairs = list(zip(literals[0:26:2], literals[1:26:2]))
    byte_pairs = list(zip(literals[26:42:2], literals[27:42:2]))
    second_halfword_pairs = list(zip(literals[42::2], literals[43::2]))
    return [(
        "thumb_second_register_sync",
        THUMB_SECOND_REGISTER_SYNC_PC,
        first_halfword_pairs,
        byte_pairs,
        second_halfword_pairs,
    )]


def decode_thumb_display_reset_wrapper_return_block(data):
    expected = {
        0x0801096E: 0xBC01,
        0x08010970: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_display_reset_wrapper_return", THUMB_DISPLAY_RESET_WRAPPER_RETURN_PC)]


def decode_thumb_input_reset_wrapper_block(data):
    expected = {
        0x08010974: 0xB500,
        0x08010976: 0xF7FF,
        0x08010978: 0xFDFD,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08010976), rom_halfword(data, 0x08010978), 0x08010976) | 1
    return [("thumb_input_reset_wrapper", THUMB_INPUT_RESET_WRAPPER_PC, target)]


def decode_thumb_input_reset_after_register_sync_block(data):
    expected = {
        0x0801097A: 0xF7FF,
        0x0801097C: 0xFE93,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0801097A), rom_halfword(data, 0x0801097C), 0x0801097A) | 1
    return [("thumb_input_reset_after_register_sync", THUMB_INPUT_RESET_AFTER_REGISTER_SYNC_PC, target)]


def decode_thumb_input_reset_tail_block(data):
    expected = {
        0x0801097E: 0x4915,
        0x08010980: 0x4815,
        0x08010982: 0x7800,
        0x08010984: 0x7008,
        0x08010986: 0x3101,
        0x08010988: 0x4814,
        0x0801098A: 0x7800,
        0x0801098C: 0x7008,
        0x0801098E: 0x3103,
        0x08010990: 0x4813,
        0x08010992: 0x7800,
        0x08010994: 0x7008,
        0x08010996: 0x3101,
        0x08010998: 0x4812,
        0x0801099A: 0x7800,
        0x0801099C: 0x7008,
        0x0801099E: 0x3903,
        0x080109A0: 0x4811,
        0x080109A2: 0x7800,
        0x080109A4: 0x7008,
        0x080109A6: 0x3101,
        0x080109A8: 0x4810,
        0x080109AA: 0x7800,
        0x080109AC: 0x7008,
        0x080109AE: 0x3103,
        0x080109B0: 0x480F,
        0x080109B2: 0x7800,
        0x080109B4: 0x7008,
        0x080109B6: 0x3101,
        0x080109B8: 0x480E,
        0x080109BA: 0x7800,
        0x080109BC: 0x7008,
        0x080109BE: 0x3101,
        0x080109C0: 0x480D,
        0x080109C2: 0x8800,
        0x080109C4: 0x8008,
        0x080109C6: 0x3102,
        0x080109C8: 0x480C,
        0x080109CA: 0x8800,
        0x080109CC: 0x8008,
        0x080109CE: 0xBC01,
        0x080109D0: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_input_reset_tail",
        THUMB_INPUT_RESET_TAIL_PC,
        rom_word(data, 0x080109D4),
        [
            rom_word(data, 0x080109D8),
            rom_word(data, 0x080109DC),
            rom_word(data, 0x080109E0),
            rom_word(data, 0x080109E4),
            rom_word(data, 0x080109E8),
            rom_word(data, 0x080109EC),
            rom_word(data, 0x080109F0),
            rom_word(data, 0x080109F4),
        ],
        [
            rom_word(data, 0x080109F8),
            rom_word(data, 0x080109FC),
        ],
    )]


def decode_thumb_global_init_block(data):
    expected = {
        0x0801A768: 0xB530,
        0x0801A76A: 0x9D03,
        0x0801A76C: 0x4C08,
        0x0801A76E: 0x6020,
        0x0801A770: 0x4808,
        0x0801A772: 0x6001,
        0x0801A774: 0x4808,
        0x0801A776: 0x6002,
        0x0801A778: 0x4808,
        0x0801A77A: 0x7003,
        0x0801A77C: 0x4808,
        0x0801A77E: 0x6005,
        0x0801A780: 0xF000,
        0x0801A782: 0xFC22,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0801A780), rom_halfword(data, 0x0801A782), 0x0801A780) | 1
    return [(
        "thumb_global_init_prefix",
        THUMB_GLOBAL_INIT_PC,
        rom_word(data, 0x0801A790),
        rom_word(data, 0x0801A794),
        rom_word(data, 0x0801A798),
        rom_word(data, 0x0801A79C),
        rom_word(data, 0x0801A7A0),
        target,
    )]


def decode_thumb_global_init_after_save_block(data):
    expected = {
        0x0801A784: 0x2000,
        0x0801A786: 0xF000,
        0x0801A788: 0xFD97,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0801A786), rom_halfword(data, 0x0801A788), 0x0801A786) | 1
    return [("thumb_global_init_after_save", THUMB_GLOBAL_INIT_AFTER_SAVE_PC, target)]


def decode_thumb_global_init_return_block(data):
    expected = {
        0x0801A78A: 0xBC30,
        0x0801A78C: 0xBC01,
        0x0801A78E: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_global_init_return", THUMB_GLOBAL_INIT_RETURN_PC)]


def decode_thumb_save_probe_block(data):
    expected = {
        0x0801AFC8: 0xB510,
        0x0801AFCA: 0x4C04,
        0x0801AFCC: 0xF05E,
        0x0801AFCE: 0xFC16,
        0x0801AFD0: 0x7020,
        0x0801AFD2: 0x0600,
        0x0801AFD4: 0x2800,
        0x0801AFD6: 0xD103,
        0x0801AFD8: 0x2001,
        0x0801AFDA: 0xE002,
        0x0801AFE0: 0x2000,
        0x0801AFE2: 0x7020,
        0x0801AFE4: 0xBC10,
        0x0801AFE6: 0xBC01,
        0x0801AFE8: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_save_probe",
        THUMB_SAVE_PROBE_PC,
        rom_word(data, 0x0801AFDC),
        thumb_bl_target(rom_halfword(data, 0x0801AFCC), rom_halfword(data, 0x0801AFCE), 0x0801AFCC) | 1,
    )]


def decode_thumb_table_init_block(data):
    expected = {
        0x0801B2B8: 0xB5F0,
        0x0801B2BA: 0x4657,
        0x0801B2BC: 0x464E,
        0x0801B2BE: 0x4645,
        0x0801B2C0: 0xB4E0,
        0x0801B2C2: 0xB081,
        0x0801B2C4: 0x9000,
        0x0801B2CC: 0x4D38,
        0x0801B2CE: 0x4839,
        0x0801B2D0: 0x2100,
        0x0801B2D2: 0x220F,
        0x0801B2D4: 0x307C,
        0x0801B2D6: 0x6001,
        0x0801B2D8: 0x3804,
        0x0801B2DA: 0x3A01,
        0x0801B2DC: 0x2A00,
        0x0801B2DE: 0xDAFA,
        0x0801B2E0: 0x2000,
        0x0801B2E2: 0x6028,
        0x0801B45A: 0xB001,
        0x0801B45C: 0xBC38,
        0x0801B45E: 0x4698,
        0x0801B460: 0x46A1,
        0x0801B462: 0x46AA,
        0x0801B464: 0xBCF0,
        0x0801B466: 0xBC01,
        0x0801B468: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_table_init",
        THUMB_TABLE_INIT_PC,
        rom_word(data, 0x0801B3B0),
        rom_word(data, 0x0801B3B4),
    )]


def decode_thumb_object_setup_wrapper_block(data):
    expected = {
        0x08015FB4: 0xB510,
        0x08015FB6: 0x4C06,
        0x08015FB8: 0x2000,
        0x08015FBA: 0x1C21,
        0x08015FBC: 0xF004,
        0x08015FBE: 0xFE2E,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08015FBC), rom_halfword(data, 0x08015FBE), 0x08015FBC) | 1
    return [(
        "thumb_object_setup_wrapper",
        THUMB_OBJECT_SETUP_WRAPPER_PC,
        rom_word(data, 0x08015FD0),
        target,
    )]


def decode_thumb_object_state_check_block(data):
    expected = {
        0x0801AC1C: 0xB5F0,
        0x0801AC1E: 0x4657,
        0x0801AC20: 0x464E,
        0x0801AC22: 0x4645,
        0x0801AC24: 0xB4E0,
        0x0801AC26: 0xB084,
        0x0801AC28: 0x0600,
        0x0801AC2A: 0x0E00,
        0x0801AC2C: 0x4680,
        0x0801AC2E: 0x2400,
        0x0801AC30: 0x4B11,
        0x0801AC32: 0x2510,
        0x0801AC48: 0x4640,
        0x0801AC4A: 0xF000,
        0x0801AC4C: 0xF875,
        0x0801ACF6: 0x4642,
        0x0801ACF8: 0x2A00,
        0x0801ACFA: 0xD107,
        0x0801ACFC: 0x4909,
        0x0801AD0C: 0x2000,
        0x0801AD0E: 0xB004,
        0x0801AD10: 0xBC38,
        0x0801AD12: 0x4698,
        0x0801AD14: 0x46A1,
        0x0801AD16: 0x46AA,
        0x0801AD18: 0xBCF0,
        0x0801AD1A: 0xBC02,
        0x0801AD1C: 0x4708,
        0x0801AD38: 0xB570,
        0x0801AD3A: 0x0600,
        0x0801AD3C: 0x0E04,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_object_state_check",
        THUMB_OBJECT_STATE_CHECK_PC,
        rom_word(data, 0x0801AC78),
        rom_word(data, 0x0801AD24),
    )]


def decode_thumb_object_setup_continuation_block(data):
    expected = {
        0x08015FC0: 0x2800,
        0x08015FC2: 0xD102,
        0x08015FC4: 0x1C20,
        0x08015FC6: 0xF7FF,
        0x08015FC8: 0xFEA9,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08015FC6), rom_halfword(data, 0x08015FC8), 0x08015FC6) | 1
    return [("thumb_object_setup_continuation", THUMB_OBJECT_SETUP_CONTINUATION_PC, target)]


def decode_thumb_state_query_block(data):
    expected = {
        0x0801AD38: 0xB570,
        0x0801AD3A: 0x0600,
        0x0801AD3C: 0x0E04,
        0x0801AD3E: 0x2CFF,
        0x0801AD40: 0xD006,
        0x0801AD42: 0x1C20,
        0x0801AD44: 0xF000,
        0x0801AD46: 0xF9D4,
        0x0801AD48: 0x0400,
        0x0801AD4A: 0x4902,
        0x0801AD4C: 0x4288,
        0x0801AD4E: 0xD103,
        0x0801AD50: 0x2001,
        0x0801AD52: 0xE01B,
        0x0801AD8C: 0xBC70,
        0x0801AD8E: 0xBC02,
        0x0801AD90: 0x4708,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_state_query_boot_unavailable",
        THUMB_STATE_QUERY_PC,
        rom_word(data, 0x0801AD54),
        thumb_bl_target(rom_halfword(data, 0x0801AD44), rom_halfword(data, 0x0801AD46), 0x0801AD44) | 1,
    )]


def decode_thumb_object_copy_block(data):
    expected = {
        0x08015D1C: 0xB5F0,
        0x08015D1E: 0x4647,
        0x08015D20: 0xB480,
        0x08015D22: 0x1C06,
        0x08015D24: 0x2100,
        0x08015D26: 0x481C,
        0x08015D28: 0x4680,
        0x08015D2A: 0x4F1C,
        0x08015D58: 0x22C8,
        0x08015D5A: 0x0092,
        0x08015D62: 0x4640,
        0x08015D64: 0xF064,
        0x08015D66: 0xFD9C,
        0x08015D76: 0x490A,
        0x08015D78: 0x4A0A,
        0x08015D88: 0x20A3,
        0x08015D8A: 0x00C0,
        0x08015D8C: 0xBC08,
        0x08015D8E: 0x4698,
        0x08015D90: 0xBCF0,
        0x08015D92: 0xBC02,
        0x08015D94: 0x4708,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_object_copy",
        THUMB_OBJECT_COPY_PC,
        rom_word(data, 0x08015D98),
        rom_word(data, 0x08015D9C),
        rom_word(data, 0x08015DA0),
        rom_word(data, 0x08015DA4),
        thumb_bl_target(rom_halfword(data, 0x08015D64), rom_halfword(data, 0x08015D66), 0x08015D64) | 1,
    )]


def decode_thumb_object_setup_return_block(data):
    expected = {
        0x08015FCA: 0xBC10,
        0x08015FCC: 0xBC01,
        0x08015FCE: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_object_setup_return", THUMB_OBJECT_SETUP_RETURN_PC)]


def decode_thumb_slot_init_block(data):
    expected = {
        0x0803F87C: 0xB510,
        0x0803F87E: 0x2400,
        0x0803F880: 0x1C20,
        0x0803F882: 0xF000,
        0x0803F884: 0xF809,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0803F882), rom_halfword(data, 0x0803F884), 0x0803F882) | 1
    return [("thumb_slot_init", THUMB_SLOT_INIT_PC, target)]


def decode_thumb_slot_loop_continuation_block(data):
    expected = {
        0x0803F886: 0x1C60,
        0x0803F888: 0x0600,
        0x0803F88A: 0x0E04,
        0x0803F88C: 0x2C0B,
        0x0803F88E: 0xD9F7,
        0x0803F880: 0x1C20,
        0x0803F882: 0xF000,
        0x0803F884: 0xF809,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0803F882), rom_halfword(data, 0x0803F884), 0x0803F882) | 1
    return [("thumb_slot_loop_continuation", THUMB_SLOT_LOOP_CONTINUATION_PC, target)]


def decode_thumb_slot_init_return_block(data):
    expected = {
        0x0803F890: 0xBC10,
        0x0803F892: 0xBC01,
        0x0803F894: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_slot_init_return", THUMB_SLOT_INIT_RETURN_PC)]


def decode_thumb_slot_helper_block(data):
    expected = {
        0x0803F898: 0xB570,
        0x0803F89A: 0x0600,
        0x0803F89C: 0x0E04,
        0x0803F89E: 0x4E09,
        0x0803F8A0: 0x2C03,
        0x0803F8A2: 0xD807,
        0x0803F8A4: 0x1D60,
        0x0803F8A6: 0x0600,
        0x0803F8A8: 0x0E05,
        0x0803F8AA: 0x1C28,
        0x0803F8AC: 0xF7DB,
        0x0803F8AE: 0xFA44,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0803F8AC), rom_halfword(data, 0x0803F8AE), 0x0803F8AC) | 1
    return [(
        "thumb_slot_helper_prefix",
        THUMB_SLOT_HELPER_PC,
        rom_word(data, 0x0803F8C4),
        target,
    )]


def decode_thumb_slot_helper_after_query_block(data):
    expected = {
        0x0803F8B0: 0x2800,
        0x0803F8B2: 0xD00B,
        0x0803F8B4: 0x4804,
        0x0803F8B6: 0x0161,
        0x0803F8B8: 0x1809,
        0x0803F8BA: 0x20FF,
        0x0803F8BC: 0x7548,
        0x0803F8BE: 0x2000,
        0x0803F8C0: 0xE02C,
        0x0803F91C: 0xBC70,
        0x0803F91E: 0xBC02,
        0x0803F920: 0x4708,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_slot_helper_after_query",
        THUMB_SLOT_HELPER_AFTER_QUERY_PC,
        rom_word(data, 0x0803F8C4),
    )]


def decode_thumb_helper_block(data):
    pc = THUMB_HELPER_PC
    ops = []

    first = rom_halfword(data, pc)
    if first != 0xB500:
        raise ValueError(f"expected push {{lr}} at 0x{pc:08X}, got 0x{first:04X}")
    ops.append(("thumb_push_lr", pc))
    pc += 2

    for expected_rd in (0, 1):
        instruction = rom_halfword(data, pc)
        if instruction & 0xF800 != 0x2000:
            raise ValueError(f"expected movs immediate at 0x{pc:08X}, got 0x{instruction:04X}")
        rd = (instruction >> 8) & 0x7
        if rd != expected_rd:
            raise ValueError(f"expected movs r{expected_rd} at 0x{pc:08X}, got r{rd}")
        value = instruction & 0xFF
        ops.append(("thumb_movs_imm", pc, rd, value))
        pc += 2

    first = rom_halfword(data, pc)
    second = rom_halfword(data, pc + 2)
    target = thumb_bl_target(first, second, pc) | 1
    ops.append(("thumb_bl", pc, target, pc + 5))
    return ops


def decode_thumb_helper_continuation_block(data):
    pc = THUMB_HELPER_CONTINUATION_PC
    first = rom_halfword(data, pc)
    second = rom_halfword(data, pc + 2)
    target = thumb_bl_target(first, second, pc) | 1
    return [("thumb_bl", pc, target, pc + 5)]


def decode_thumb_init_copy_block(data):
    pc = THUMB_INIT_COPY_PC
    ops = []

    if rom_halfword(data, pc) != 0xB500:
        raise ValueError(f"expected push {{lr}} at 0x{pc:08X}")
    ops.append(("thumb_push_lr", pc))
    pc += 2

    for expected_rd in (0, 1):
        instruction = rom_halfword(data, pc)
        if instruction & 0xF800 != 0x4800:
            raise ValueError(f"expected ldr literal at 0x{pc:08X}, got 0x{instruction:04X}")
        rd = (instruction >> 8) & 0x7
        if rd != expected_rd:
            raise ValueError(f"expected ldr r{expected_rd} at 0x{pc:08X}, got r{rd}")
        imm = (instruction & 0xFF) << 2
        literal_address = ((pc + 4) & ~3) + imm
        ops.append(("thumb_ldr_literal", pc, rd, literal_address, rom_word(data, literal_address)))
        pc += 2

    instruction = rom_halfword(data, pc)
    if instruction != 0x2210:
        raise ValueError(f"expected movs r2, #0x10 at 0x{pc:08X}, got 0x{instruction:04X}")
    ops.append(("thumb_movs_imm", pc, 2, 0x10))
    pc += 2

    first = rom_halfword(data, pc)
    second = rom_halfword(data, pc + 2)
    target = thumb_bl_target(first, second, pc) | 1
    ops.append(("thumb_bl", pc, target, pc + 5))
    return ops


def decode_thumb_copy_return_block(data):
    instruction = rom_halfword(data, THUMB_COPY_RETURN_PC)
    if instruction != 0xBD00:
        raise ValueError(f"expected pop {{pc}} at 0x{THUMB_COPY_RETURN_PC:08X}, got 0x{instruction:04X}")
    return [("thumb_pop_pc", THUMB_COPY_RETURN_PC)]


def decode_thumb_outer_return_block(data):
    instruction = rom_halfword(data, THUMB_OUTER_RETURN_PC)
    if instruction != 0xBD00:
        raise ValueError(f"expected pop {{pc}} at 0x{THUMB_OUTER_RETURN_PC:08X}, got 0x{instruction:04X}")
    return [("thumb_pop_pc", THUMB_OUTER_RETURN_PC)]


def decode_thumb_hardware_helper_block(data):
    expected = {
        0x0807AE60: 0x1C0A,
        0x0807AE62: 0x2801,
        0x0807AE64: 0xD014,
        0x0807AE66: 0x2801,
        0x0807AE68: 0xDC06,
        0x0807AE6A: 0x2800,
        0x0807AE6C: 0xD00A,
        0x0807AE84: 0x4801,
        0x0807AE86: 0x6002,
        0x0807AE88: 0x1C01,
        0x0807AE8A: 0xE00B,
        0x0807AEA4: 0x4806,
        0x0807AEA6: 0x6809,
        0x0807AEA8: 0x8001,
        0x0807AEAA: 0x2280,
        0x0807AEAC: 0x0252,
        0x0807AEAE: 0x400A,
        0x0807AEB0: 0x2A00,
        0x0807AEB2: 0xD009,
        0x0807AEC8: 0x4801,
        0x0807AECA: 0x8002,
        0x0807AECC: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )

    return [
        ("thumb_hw_init_path",
         THUMB_HARDWARE_HELPER_PC,
         rom_word(data, 0x0807AE8C),
         rom_word(data, 0x0807AEC0),
         rom_word(data, 0x0807AED0)),
    ]


def decode_thumb_struct_init_block(data):
    expected = {
        0x0807AFF4: 0x1C13,
        0x0807AFF6: 0x2200,
        0x0807AFF8: 0x7002,
        0x0807AFFA: 0x6042,
        0x0807AFFC: 0x7043,
        0x0807AFFE: 0x7082,
        0x0807B000: 0x60C1,
        0x0807B002: 0x6081,
        0x0807B004: 0x2B00,
        0x0807B006: 0xDD06,
        0x0807B008: 0x2000,
        0x0807B00A: 0x1C1A,
        0x0807B00C: 0x6008,
        0x0807B00E: 0x310C,
        0x0807B010: 0x3A01,
        0x0807B012: 0x2A00,
        0x0807B014: 0xD1FA,
        0x0807B016: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_struct_init", THUMB_STRUCT_INIT_PC)]


def decode_thumb_large_boot_block(data):
    expected = {
        0x080386E4: 0xB510,
        0x080386E6: 0xB084,
        0x080386E8: 0x2200,
        0x080386EA: 0x9202,
        0x080386EC: 0x4934,
        0x080386EE: 0xA802,
        0x080386F0: 0x6008,
        0x080386F2: 0x20C0,
        0x080386F4: 0x0480,
        0x080386F6: 0x6048,
        0x080386F8: 0x4832,
        0x080386FA: 0x6088,
        0x080386FC: 0x6888,
        0x080386FE: 0xA803,
        0x08038700: 0x8002,
        0x08038702: 0x6008,
        0x08038704: 0x2080,
        0x08038706: 0x0480,
        0x08038708: 0x6048,
        0x0803870A: 0x482F,
        0x0803870C: 0x6088,
        0x0803870E: 0x6888,
        0x08038710: 0x492E,
        0x08038712: 0x4A2F,
        0x08038714: 0x1C10,
        0x08038716: 0x8008,
        0x08038718: 0xAA01,
        0x0803871A: 0x482E,
        0x0803871C: 0x8801,
        0x0803871E: 0x4B2E,
        0x08038720: 0x1C18,
        0x08038722: 0x4388,
        0x08038724: 0x8010,
        0x08038726: 0xF042,
        0x08038728: 0xFB75,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_large_boot_prefix",
        THUMB_LARGE_BOOT_PC,
        rom_word(data, 0x080387C0),
        rom_word(data, 0x080387C4),
        rom_word(data, 0x080387CC),
        rom_word(data, 0x080387D0),
        rom_word(data, 0x080387D4),
        rom_word(data, 0x080387D8),
        thumb_bl_target(rom_halfword(data, 0x08038726), rom_halfword(data, 0x08038728), 0x08038726) | 1,
    )]


def decode_thumb_large_boot_after_display_block(data):
    expected = {
        0x0803872A: 0x482C,
        0x0803872C: 0x2180,
        0x0803872E: 0x0209,
        0x08038730: 0xF7DA,
        0x08038732: 0xFADE,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08038730), rom_halfword(data, 0x08038732), 0x08038730) | 1
    return [(
        "thumb_large_boot_after_display",
        THUMB_LARGE_BOOT_AFTER_DISPLAY_PC,
        rom_word(data, 0x080387DC),
        target,
    )]


def decode_thumb_large_boot_alloc_result_block(data):
    expected = {
        0x08038734: 0x2101,
        0x08038736: 0x4249,
        0x08038738: 0x4288,
        0x0803873A: 0xD101,
        0x08038740: 0x4827,
        0x08038742: 0x4928,
        0x08038744: 0x4A28,
        0x08038746: 0x4B29,
        0x08038748: 0x9300,
        0x0803874A: 0x2302,
        0x0803874C: 0xF7E2,
        0x0803874E: 0xF80C,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0803874C), rom_halfword(data, 0x0803874E), 0x0803874C) | 1
    return [(
        "thumb_large_boot_alloc_result",
        THUMB_LARGE_BOOT_ALLOC_RESULT_PC,
        rom_word(data, 0x080387E0),
        rom_word(data, 0x080387E4),
        rom_word(data, 0x080387E8),
        rom_word(data, 0x080387EC),
        target,
    )]


def decode_thumb_large_boot_after_global_init_block(data):
    expected = {
        0x08038750: 0xF7DD,
        0x08038752: 0xFC30,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08038750), rom_halfword(data, 0x08038752), 0x08038750) | 1
    return [("thumb_large_boot_after_global_init", THUMB_LARGE_BOOT_AFTER_GLOBAL_INIT_PC, target)]


def decode_thumb_large_boot_after_object_setup_block(data):
    expected = {
        0x08038754: 0xF007,
        0x08038756: 0xF892,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08038754), rom_halfword(data, 0x08038756), 0x08038754) | 1
    return [("thumb_large_boot_after_object_setup", THUMB_LARGE_BOOT_AFTER_OBJECT_SETUP_PC, target)]


def decode_thumb_large_boot_after_slot_init_block(data):
    expected = {
        0x08038758: 0x4825,
        0x0803875A: 0xF7D8,
        0x0803875C: 0xF98D,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0803875A), rom_halfword(data, 0x0803875C), 0x0803875A) | 1
    return [(
        "thumb_large_boot_after_slot_init",
        THUMB_LARGE_BOOT_AFTER_SLOT_INIT_PC,
        rom_word(data, 0x080387F0),
        target,
    )]


def decode_thumb_large_boot_after_seed_block(data):
    expected = {
        0x0803875E: 0xF7D8,
        0x08038760: 0xF903,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0803875E), rom_halfword(data, 0x08038760), 0x0803875E) | 1
    return [("thumb_large_boot_after_seed", THUMB_LARGE_BOOT_AFTER_SEED_PC, target)]


def decode_thumb_large_boot_after_display_reset_block(data):
    expected = {
        0x08038762: 0xF7D8,
        0x08038764: 0xF907,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08038762), rom_halfword(data, 0x08038764), 0x08038762) | 1
    return [("thumb_large_boot_after_display_reset", THUMB_LARGE_BOOT_AFTER_DISPLAY_RESET_PC, target)]


def decode_thumb_large_boot_after_input_reset_block(data):
    expected = {
        0x08038766: 0x4923,
        0x08038768: 0x2000,
        0x0803876A: 0xF042,
        0x0803876C: 0xFB71,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0803876A), rom_halfword(data, 0x0803876C), 0x0803876A) | 1
    return [(
        "thumb_large_boot_after_input_reset",
        THUMB_LARGE_BOOT_AFTER_INPUT_RESET_PC,
        rom_word(data, 0x080387F4),
        target,
    )]


def decode_thumb_large_boot_default_state_block(data):
    expected = {
        0x0803876E: 0x4A22,
        0x08038770: 0x6810,
        0x08038772: 0x3085,
        0x08038774: 0x2100,
        0x08038776: 0x7001,
        0x08038778: 0x6810,
        0x0803877A: 0x30D0,
        0x0803877C: 0x2103,
        0x0803877E: 0x7741,
        0x08038780: 0x6810,
        0x08038782: 0x219C,
        0x08038784: 0x0049,
        0x08038786: 0x1840,
        0x08038788: 0x2108,
        0x0803878A: 0x7741,
        0x0803878C: 0x6810,
        0x0803878E: 0x22D0,
        0x08038790: 0x0052,
        0x08038792: 0x1880,
        0x08038794: 0x2106,
        0x08038796: 0x7741,
        0x08038798: 0x2200,
        0x0803879A: 0xA801,
        0x0803879C: 0x8801,
        0x0803879E: 0x200F,
        0x080387A0: 0x4008,
        0x080387A2: 0x280F,
        0x080387A4: 0xD006,
        0x080387A6: 0xA801,
        0x080387A8: 0x8801,
        0x080387AA: 0x2085,
        0x080387AC: 0x0080,
        0x080387AE: 0x4281,
        0x080387B0: 0xD100,
        0x080387B2: 0x2201,
        0x080387B4: 0x2A00,
        0x080387B6: 0xD021,
        0x080387B8: 0xF000,
        0x080387BA: 0xF846,
        0x080387BC: 0xE020,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    pointer_table = rom_word(data, 0x080387F8)
    state_base = rom_word(data, pointer_table)
    true_target = thumb_bl_target(rom_halfword(data, 0x080387B8), rom_halfword(data, 0x080387BA), 0x080387B8) | 1
    return [(
        "thumb_large_boot_default_state",
        THUMB_LARGE_BOOT_DEFAULT_STATE_PC,
        state_base,
        0x080387FD,
        true_target,
    )]


def decode_thumb_large_boot_save_path_block(data):
    expected = {
        0x080387FC: 0xF7FF,
        0x080387FE: 0xFF5A,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x080387FC), rom_halfword(data, 0x080387FE), 0x080387FC) | 1
    return [("thumb_large_boot_save_path", THUMB_LARGE_BOOT_SAVE_PATH_PC, target)]


def decode_thumb_refresh_init_wrapper_block(data):
    expected = {
        0x080386B4: 0xB500,
        0x080386B6: 0xF7FF,
        0x080386B8: 0xFF89,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x080386B6), rom_halfword(data, 0x080386B8), 0x080386B6) | 1
    return [("thumb_refresh_init_wrapper", THUMB_REFRESH_INIT_WRAPPER_PC, target)]


def decode_thumb_system_init_block(data):
    expected = {
        0x080385CC: 0xB510,
        0x080385CE: 0x4822,
        0x080385D0: 0x2400,
        0x080385D2: 0x6004,
        0x080385D4: 0xF7D6,
        0x080385D6: 0xFDEC,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x080385D4), rom_halfword(data, 0x080385D6), 0x080385D4) | 1
    return [(
        "thumb_system_init_prefix",
        THUMB_SYSTEM_INIT_PC,
        rom_word(data, 0x08038658),
        target,
    )]


def decode_thumb_engine_clear_block(data):
    expected = {
        0x0800F1B0: 0xB500,
        0x0800F1B2: 0xF7FF,
        0x0800F1B4: 0xFFDD,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0800F1B2), rom_halfword(data, 0x0800F1B4), 0x0800F1B2) | 1
    return [("thumb_engine_clear", THUMB_ENGINE_CLEAR_PC, target)]


def decode_thumb_engine_register_reset_block(data):
    expected = {
        0x0800F170: 0x4A0B,
        0x0800F172: 0x2300,
        0x0800F174: 0x8013,
        0x0800F176: 0x2001,
        0x0800F178: 0x7811,
        0x0800F17A: 0x4308,
        0x0800F17C: 0x2102,
        0x0800F17E: 0x4308,
        0x0800F180: 0x2104,
        0x0800F182: 0x4308,
        0x0800F184: 0x2108,
        0x0800F186: 0x4308,
        0x0800F188: 0x2110,
        0x0800F18A: 0x4308,
        0x0800F18C: 0x2120,
        0x0800F18E: 0x4308,
        0x0800F190: 0x7010,
        0x0800F192: 0x4804,
        0x0800F194: 0x8003,
        0x0800F196: 0x4804,
        0x0800F198: 0x8003,
        0x0800F19A: 0x4804,
        0x0800F19C: 0x8003,
        0x0800F19E: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_engine_register_reset",
        THUMB_ENGINE_REGISTER_RESET_PC,
        rom_word(data, 0x0800F1A0),
        rom_word(data, 0x0800F1A4),
        rom_word(data, 0x0800F1A8),
        rom_word(data, 0x0800F1AC),
    )]


def decode_thumb_engine_clear_after_register_reset_block(data):
    expected = {
        0x0800F1B6: 0x4906,
        0x0800F1B8: 0x20C0,
        0x0800F1BA: 0x780A,
        0x0800F1BC: 0x4310,
        0x0800F1BE: 0x7008,
        0x0800F1C0: 0x4904,
        0x0800F1C2: 0x201F,
        0x0800F1C4: 0x8008,
        0x0800F1C6: 0xF001,
        0x0800F1C8: 0xFBD5,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0800F1C6), rom_halfword(data, 0x0800F1C8), 0x0800F1C6) | 1
    return [(
        "thumb_engine_clear_after_register_reset",
        THUMB_ENGINE_CLEAR_AFTER_REGISTER_RESET_PC,
        rom_word(data, 0x0800F1D0),
        rom_word(data, 0x0800F1D4),
        target,
    )]


def decode_thumb_engine_clear_return_block(data):
    expected = {
        0x0800F1CA: 0xBC01,
        0x0800F1CC: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_engine_clear_return", THUMB_ENGINE_CLEAR_RETURN_PC)]


def decode_thumb_system_init_after_engine_clear_block(data):
    expected = {
        0x080385D8: 0x2000,
        0x080385DA: 0xF7FF,
        0x080385DC: 0xFE41,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x080385DA), rom_halfword(data, 0x080385DC), 0x080385DA) | 1
    return [("thumb_system_init_after_engine_clear", THUMB_SYSTEM_INIT_AFTER_ENGINE_CLEAR_PC, target)]


def decode_thumb_state_store_first_block(data):
    expected = {
        0x08038260: 0x4901,
        0x08038262: 0x6008,
        0x08038264: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_state_store_first", THUMB_STATE_STORE_FIRST_PC, rom_word(data, 0x08038268))]


def decode_thumb_system_init_after_first_state_store_block(data):
    expected = {
        0x080385DE: 0x2000,
        0x080385E0: 0xF7FF,
        0x080385E2: 0xFE44,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x080385E0), rom_halfword(data, 0x080385E2), 0x080385E0) | 1
    return [("thumb_system_init_after_first_state_store", THUMB_SYSTEM_INIT_AFTER_FIRST_STATE_STORE_PC, target)]


def decode_thumb_state_store_second_block(data):
    expected = {
        0x0803826C: 0x4901,
        0x0803826E: 0x6008,
        0x08038270: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_state_store_second", THUMB_STATE_STORE_SECOND_PC, rom_word(data, 0x08038274))]


def decode_thumb_system_init_clear_globals_block(data):
    expected = {
        0x080385E4: 0x481D,
        0x080385E6: 0x7004,
        0x080385E8: 0x481D,
        0x080385EA: 0x6004,
        0x080385EC: 0x481D,
        0x080385EE: 0x6004,
        0x080385F0: 0x481D,
        0x080385F2: 0x6004,
        0x080385F4: 0x481D,
        0x080385F6: 0x6004,
        0x080385F8: 0x481D,
        0x080385FA: 0x8004,
        0x080385FC: 0xF7FF,
        0x080385FE: 0xFFDE,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x080385FC), rom_halfword(data, 0x080385FE), 0x080385FC) | 1
    return [(
        "thumb_system_init_clear_globals",
        THUMB_SYSTEM_INIT_CLEAR_GLOBALS_PC,
        rom_word(data, 0x0803865C),
        rom_word(data, 0x08038660),
        rom_word(data, 0x08038664),
        rom_word(data, 0x08038668),
        rom_word(data, 0x0803866C),
        rom_word(data, 0x08038670),
        target,
    )]


def decode_thumb_system_init_local_reset_block(data):
    expected = {
        0x080385BC: 0x4901,
        0x080385BE: 0x2000,
        0x080385C0: 0x8008,
        0x080385C2: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_system_init_local_reset", THUMB_SYSTEM_INIT_LOCAL_RESET_PC, rom_word(data, 0x080385C4))]


def decode_thumb_system_init_dma_copy_call_block(data):
    expected = {
        0x08038600: 0xF7E3,
        0x08038602: 0xF90E,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08038600), rom_halfword(data, 0x08038602), 0x08038600) | 1
    return [("thumb_system_init_dma_copy_call", THUMB_SYSTEM_INIT_DMA_COPY_CALL_PC, target)]


def decode_thumb_dma_setup_block(data):
    expected = {
        0x0801B820: 0xB500,
        0x0801B822: 0x4B08,
        0x0801B824: 0x4908,
        0x0801B826: 0x4809,
        0x0801B828: 0x1A40,
        0x0801B82A: 0x2800,
        0x0801B82C: 0xDA00,
        0x0801B82E: 0x3003,
        0x0801B830: 0x0242,
        0x0801B832: 0x0AD2,
        0x0801B834: 0x2080,
        0x0801B836: 0x04C0,
        0x0801B838: 0x4302,
        0x0801B83A: 0x1C18,
        0x0801B83C: 0xF05D,
        0x0801B83E: 0xFF42,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0801B83C), rom_halfword(data, 0x0801B83E), 0x0801B83C) | 1
    return [(
        "thumb_dma_setup",
        THUMB_DMA_SETUP_PC,
        rom_word(data, 0x0801B844),
        rom_word(data, 0x0801B848),
        rom_word(data, 0x0801B84C),
        target,
    )]


def decode_thumb_bios_cpuset_boot_block(data):
    expected = {
        0x080796C4: 0xDF0B,
        0x080796C6: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    source = 0x083F752C
    destination = 0x0300666C
    control = 0x06000212
    count = control & 0x001FFFFF
    words = [rom_word(data, source + index * 4) for index in range(count)]
    return [("thumb_bios_cpuset_boot", THUMB_BIOS_CPUSET_BOOT_PC, source, destination, control, words)]


def decode_thumb_dma_setup_return_block(data):
    expected = {
        0x0801B840: 0xBC01,
        0x0801B842: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_dma_setup_return", THUMB_DMA_SETUP_RETURN_PC)]


def decode_thumb_system_init_next_call_block(data):
    expected = {
        0x08038604: 0xF7FC,
        0x08038606: 0xFB48,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08038604), rom_halfword(data, 0x08038606), 0x08038604) | 1
    return [("thumb_system_init_next_call", THUMB_SYSTEM_INIT_NEXT_CALL_PC, target)]


def decode_thumb_next_initializer_block(data):
    expected = {
        0x08034C98: 0xB500,
        0x08034C9A: 0x4A06,
        0x08034C9C: 0x4806,
        0x08034C9E: 0x6801,
        0x08034CA0: 0x6011,
        0x08034CA2: 0x4806,
        0x08034CA4: 0x6001,
        0x08034CA6: 0x2000,
        0x08034CA8: 0xF7E3,
        0x08034CAA: 0xFF00,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08034CA8), rom_halfword(data, 0x08034CAA), 0x08034CA8) | 1
    pointer_address = rom_word(data, 0x08034CB8)
    return [(
        "thumb_next_initializer",
        THUMB_NEXT_INITIALIZER_PC,
        rom_word(data, 0x08034CB4),
        pointer_address,
        rom_word(data, pointer_address),
        rom_word(data, 0x08034CBC),
        target,
    )]


def decode_thumb_byte_reset_helper_block(data):
    expected = {
        0x08018AAC: 0x4A02,
        0x08018AAE: 0x21FF,
        0x08018AB0: 0x7011,
        0x08018AB2: 0x4902,
        0x08018AB4: 0x7008,
        0x08018AB6: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_byte_reset_helper",
        THUMB_BYTE_RESET_HELPER_PC,
        rom_word(data, 0x08018AB8),
        rom_word(data, 0x08018ABC),
    )]


def decode_thumb_next_initializer_after_reset_block(data):
    expected = {
        0x08034CAC: 0xF7EA,
        0x08034CAE: 0xFA32,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08034CAC), rom_halfword(data, 0x08034CAE), 0x08034CAC) | 1
    return [("thumb_next_initializer_after_reset", THUMB_NEXT_INITIALIZER_AFTER_RESET_PC, target)]


def decode_thumb_table_copy_helper_block(data):
    expected = {
        0x0801F114: 0xB530,
        0x0801F116: 0x4D07,
        0x0801F118: 0x4807,
        0x0801F11A: 0x6803,
        0x0801F11C: 0x2200,
        0x0801F11E: 0x4C07,
        0x0801F120: 0x1898,
        0x0801F122: 0x18A9,
        0x0801F124: 0x7809,
        0x0801F126: 0x7001,
        0x0801F128: 0x3201,
        0x0801F12A: 0x42A2,
        0x0801F12C: 0xDDF8,
        0x0801F12E: 0xBC30,
        0x0801F130: 0xBC01,
        0x0801F132: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    source = rom_word(data, 0x0801F134)
    destination_pointer_address = rom_word(data, 0x0801F138)
    count = rom_word(data, 0x0801F13C)
    destination = rom_word(data, destination_pointer_address)
    bytes_to_copy = [rom_halfword(data, source + index) & 0xFF for index in range(count + 1)]
    return [(
        "thumb_table_copy_helper",
        THUMB_TABLE_COPY_HELPER_PC,
        source,
        destination_pointer_address,
        destination,
        count,
        bytes_to_copy,
    )]


def decode_thumb_next_initializer_return_block(data):
    expected = {
        0x08034CB0: 0xBC01,
        0x08034CB2: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_next_initializer_return", THUMB_NEXT_INITIALIZER_RETURN_PC)]


def decode_thumb_system_init_mode_call_block(data):
    expected = {
        0x08038608: 0xF7FC,
        0x0803860A: 0xFB34,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08038608), rom_halfword(data, 0x0803860A), 0x08038608) | 1
    return [("thumb_system_init_mode_call", THUMB_SYSTEM_INIT_MODE_CALL_PC, target)]


def decode_thumb_mode_initializer_block(data):
    expected = {
        0x08034C74: 0xB530,
        0x08034C76: 0x4C07,
        0x08034C78: 0x2501,
        0x08034C7A: 0x7325,
        0x08034C7C: 0x2003,
        0x08034C7E: 0x7060,
        0x08034C80: 0x70A5,
        0x08034C82: 0xF7FF,
        0x08034C84: 0xFF91,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08034C82), rom_halfword(data, 0x08034C84), 0x08034C82) | 1
    return [("thumb_mode_initializer", THUMB_MODE_INITIALIZER_PC, rom_word(data, 0x08034C94), target)]


def decode_thumb_defaults_initializer_block(data):
    expected = {
        0x08034BA8: 0xB5F0,
        0x08034BAA: 0x4826,
        0x08034BAC: 0x2400,
        0x08034BAE: 0x6004,
        0x08034BB0: 0x4825,
        0x08034BB2: 0x6004,
        0x08034BB4: 0x4E25,
        0x08034BB6: 0x1C30,
        0x08034BB8: 0x3039,
        0x08034BBA: 0x2701,
        0x08034BBC: 0x7007,
        0x08034BBE: 0x3001,
        0x08034BC0: 0x7007,
        0x08034BC2: 0x3001,
        0x08034BC4: 0x7007,
        0x08034BC6: 0x3001,
        0x08034BC8: 0x7007,
        0x08034BCA: 0x3808,
        0x08034BCC: 0x7007,
        0x08034BCE: 0x1C31,
        0x08034BD0: 0x3135,
        0x08034BD2: 0x2002,
        0x08034BD4: 0x7008,
        0x08034BD6: 0x3101,
        0x08034BD8: 0x2503,
        0x08034BDA: 0x700D,
        0x08034BDC: 0x1C32,
        0x08034BDE: 0x3237,
        0x08034BE0: 0x2104,
        0x08034BE2: 0x7011,
        0x08034BE4: 0x3207,
        0x08034BE6: 0x7017,
        0x08034BE8: 0x3201,
        0x08034BEA: 0x7010,
        0x08034BEC: 0x1C30,
        0x08034BEE: 0x3040,
        0x08034BF0: 0x7001,
        0x08034BF2: 0x3001,
        0x08034BF4: 0x7005,
        0x08034BF6: 0xF7F0,
        0x08034BF8: 0xF965,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08034BF6), rom_halfword(data, 0x08034BF8), 0x08034BF6) | 1
    return [(
        "thumb_defaults_initializer",
        THUMB_DEFAULTS_INITIALIZER_PC,
        rom_word(data, 0x08034C44),
        rom_word(data, 0x08034C48),
        rom_word(data, 0x08034C4C),
        target,
    )]


def decode_thumb_order_helper_block(data):
    expected = {
        0x08024EC4: 0x4A07,
        0x08024EC6: 0x1C11,
        0x08024EC8: 0x3143,
        0x08024ECA: 0x2000,
        0x08024ECC: 0x7008,
        0x08024ECE: 0x3101,
        0x08024ED0: 0x2001,
        0x08024ED2: 0x7008,
        0x08024ED4: 0x3101,
        0x08024ED6: 0x2002,
        0x08024ED8: 0x7008,
        0x08024EDA: 0x3101,
        0x08024EDC: 0x2003,
        0x08024EDE: 0x7008,
        0x08024EE0: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_order_helper", THUMB_ORDER_HELPER_PC, rom_word(data, 0x08024EE4))]


def decode_thumb_defaults_after_order_block(data):
    expected = {
        0x08034BFA: 0x1C30,
        0x08034BFC: 0x3030,
        0x08034BFE: 0x7004,
        0x08034C00: 0x3001,
        0x08034C02: 0x7004,
        0x08034C04: 0x20FA,
        0x08034C06: 0x0080,
        0x08034C08: 0x62B0,
        0x08034C0A: 0x6274,
        0x08034C0C: 0x6174,
        0x08034C0E: 0x61B4,
        0x08034C10: 0x61F4,
        0x08034C12: 0x6234,
        0x08034C14: 0x7374,
        0x08034C16: 0x1C30,
        0x08034C18: 0x302F,
        0x08034C1A: 0x7004,
        0x08034C1C: 0x3803,
        0x08034C1E: 0x7004,
        0x08034C20: 0x3001,
        0x08034C22: 0x7004,
        0x08034C24: 0x3001,
        0x08034C26: 0x7004,
        0x08034C28: 0x7135,
        0x08034C2A: 0x7177,
        0x08034C2C: 0x71B4,
        0x08034C2E: 0x7237,
        0x08034C30: 0x4807,
        0x08034C32: 0x8B40,
        0x08034C34: 0x7270,
        0x08034C36: 0x7870,
        0x08034C38: 0x2803,
        0x08034C3A: 0xDC0B,
        0x08034C3C: 0x2802,
        0x08034C3E: 0xDB09,
        0x08034C40: 0x71F7,
        0x08034C42: 0xE00A,
        0x08034C54: 0x4902,
        0x08034C56: 0x2000,
        0x08034C58: 0x71C8,
        0x08034C5A: 0xBCF0,
        0x08034C5C: 0xBC01,
        0x08034C5E: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_defaults_after_order",
        THUMB_DEFAULTS_AFTER_ORDER_PC,
        rom_word(data, 0x08034C50),
        rom_word(data, 0x08034C60),
    )]


def decode_thumb_mode_initializer_after_defaults_block(data):
    expected = {
        0x08034C86: 0xF7FF,
        0x08034C88: 0xFFED,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08034C86), rom_halfword(data, 0x08034C88), 0x08034C86) | 1
    return [("thumb_mode_initializer_after_defaults", THUMB_MODE_INITIALIZER_AFTER_DEFAULTS_PC, target)]


def decode_thumb_mode_clear_helper_block(data):
    expected = {
        0x08034C64: 0x4802,
        0x08034C66: 0x3032,
        0x08034C68: 0x2100,
        0x08034C6A: 0x7001,
        0x08034C6C: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_mode_clear_helper", THUMB_MODE_CLEAR_HELPER_PC, rom_word(data, 0x08034C70))]


def decode_thumb_mode_initializer_return_block(data):
    expected = {
        0x08034C8A: 0x7265,
        0x08034C8C: 0xBC30,
        0x08034C8E: 0xBC01,
        0x08034C90: 0x4700,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_mode_initializer_return", THUMB_MODE_INITIALIZER_RETURN_PC)]


def decode_thumb_system_init_unit_call_block(data):
    expected = {
        0x0803860C: 0xF7DC,
        0x0803860E: 0xF9E8,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0803860C), rom_halfword(data, 0x0803860E), 0x0803860C) | 1
    return [("thumb_system_init_unit_call", THUMB_SYSTEM_INIT_UNIT_CALL_PC, target)]


def decode_thumb_unit_initializer_block(data):
    expected = {
        0x080149E0: 0xB570,
        0x080149E2: 0x4810,
        0x080149E4: 0x2400,
        0x080149E6: 0x6004,
        0x080149E8: 0xF005,
        0x080149EA: 0xFE58,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x080149E8), rom_halfword(data, 0x080149EA), 0x080149E8) | 1
    return [("thumb_unit_initializer", THUMB_UNIT_INITIALIZER_PC, rom_word(data, 0x08014A24), target)]


def decode_thumb_global_table_reset_block(data):
    expected = {
        0x0801A69C: 0xB530,
        0x0801A69E: 0x2000,
        0x0801A6A0: 0x4D0B,
        0x0801A6A2: 0x4C0C,
        0x0801A6A4: 0x1C2B,
        0x0801A6A6: 0x2200,
        0x0801A6A8: 0x0401,
        0x0801A6AA: 0x1409,
        0x0801A6AC: 0x0048,
        0x0801A6AE: 0x1840,
        0x0801A6B0: 0x0080,
        0x0801A6B2: 0x18C0,
        0x0801A6B4: 0x6002,
        0x0801A6B6: 0x3101,
        0x0801A6B8: 0x0409,
        0x0801A6BA: 0x0C08,
        0x0801A6BC: 0x1409,
        0x0801A6BE: 0x2980,
        0x0801A6C0: 0xDDF2,
        0x0801A6C2: 0x2000,
        0x0801A6C4: 0x6060,
        0x0801A6C6: 0x8020,
        0x0801A6C8: 0x6068,
        0x0801A6CA: 0xBC30,
        0x0801A6CC: 0xBC02,
        0x0801A6CE: 0x4708,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_global_table_reset",
        THUMB_GLOBAL_TABLE_RESET_PC,
        rom_word(data, 0x0801A6D0),
        rom_word(data, 0x0801A6D4),
    )]


def decode_thumb_unit_initializer_after_reset_block(data):
    expected = {
        0x080149EC: 0x490E,
        0x080149EE: 0x2010,
        0x080149F0: 0x8008,
        0x080149F2: 0x480E,
        0x080149F4: 0x8004,
        0x080149F6: 0x2580,
        0x080149F8: 0x006D,
        0x080149FA: 0x4E0D,
        0x080149FC: 0x19A0,
        0x080149FE: 0x7801,
        0x08014A00: 0x2100,
        0x08014A02: 0x7001,
        0x08014A04: 0x1C20,
        0x08014A06: 0x1C29,
        0x08014A08: 0x1C2A,
        0x08014A0A: 0x2300,
        0x08014A0C: 0xF000,
        0x08014A0E: 0xF944,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08014A0C), rom_halfword(data, 0x08014A0E), 0x08014A0C) | 1
    return [(
        "thumb_unit_initializer_after_reset",
        THUMB_UNIT_INITIALIZER_AFTER_RESET_PC,
        rom_word(data, 0x08014A28),
        rom_word(data, 0x08014A2C),
        rom_word(data, 0x08014A30),
        target,
    )]


def decode_thumb_unit_entry_setup_block(data):
    expected = {
        0x08014C98: 0xB530,
        0x08014C9A: 0x4D05,
        0x08014C9C: 0x0104,
        0x08014C9E: 0x1964,
        0x08014CA0: 0x8021,
        0x08014CA2: 0x8062,
        0x08014CA4: 0x80A3,
        0x08014CA6: 0xF7FF,
        0x08014CA8: 0xFFA7,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08014CA6), rom_halfword(data, 0x08014CA8), 0x08014CA6) | 1
    return [("thumb_unit_entry_setup", THUMB_UNIT_ENTRY_SETUP_PC, rom_word(data, 0x08014CB0), target)]


def decode_thumb_unit_geometry_helper_block(data):
    expected = {
        0x08014BF8: 0xB570,
        0x08014BFA: 0x464E,
        0x08014BFC: 0x4645,
        0x08014BFE: 0xB460,
        0x08014C00: 0xB081,
        0x08014C02: 0x4681,
        0x08014C04: 0x4823,
        0x08014C06: 0x4649,
        0x08014C08: 0x010C,
        0x08014C0A: 0x1824,
        0x08014C0C: 0x2204,
        0x08014C0E: 0x5EA0,
        0x08014C10: 0xF066,
        0x08014C12: 0xF98E,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08014C10), rom_halfword(data, 0x08014C12), 0x08014C10) | 1
    return [("thumb_unit_geometry_helper", THUMB_UNIT_GEOMETRY_HELPER_PC, rom_word(data, 0x08014C94), target)]


def decode_thumb_first_math_wrapper_block(data):
    expected = {
        0x0807AF30: 0xB500,
        0x0807AF32: 0x305A,
        0x0807AF34: 0xF7FF,
        0x0807AF36: 0xFFCE,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0807AF34), rom_halfword(data, 0x0807AF36), 0x0807AF34) | 1
    return [("thumb_first_math_wrapper", THUMB_FIRST_MATH_WRAPPER_PC, target)]


def decode_thumb_trig_lookup_block(data):
    expected = {
        0x0807AED4: 0x1C01,
        0x0807AED6: 0x4B0F,
        0x0807AED8: 0x2900,
        0x0807AEDA: 0xDA04,
        0x0807AEDC: 0x20B4,
        0x0807AEDE: 0x0040,
        0x0807AEE0: 0x1809,
        0x0807AEE2: 0x2900,
        0x0807AEE4: 0xDBFC,
        0x0807AEE6: 0x480C,
        0x0807AEE8: 0x4281,
        0x0807AEEA: 0xDD03,
        0x0807AEEC: 0x4A0B,
        0x0807AEEE: 0x1889,
        0x0807AEF0: 0x4281,
        0x0807AEF2: 0xDCFC,
        0x0807AEF4: 0x1C0A,
        0x0807AEF6: 0x29B3,
        0x0807AEF8: 0xDD00,
        0x0807AEFA: 0x39B4,
        0x0807AEFC: 0x295A,
        0x0807AEFE: 0xDD01,
        0x0807AF00: 0x20B4,
        0x0807AF02: 0x1A41,
        0x0807AF04: 0x2AB3,
        0x0807AF06: 0xDC0B,
        0x0807AF08: 0x0048,
        0x0807AF0A: 0x18C0,
        0x0807AF0C: 0x2100,
        0x0807AF0E: 0x5E40,
        0x0807AF10: 0xE00C,
        0x0807AF20: 0x0048,
        0x0807AF22: 0x18C0,
        0x0807AF24: 0x8800,
        0x0807AF26: 0x4240,
        0x0807AF28: 0x0400,
        0x0807AF2A: 0x1400,
        0x0807AF2C: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    table_base = rom_word(data, 0x0807AF14)
    max_angle = rom_word(data, 0x0807AF18)
    wrap_delta = rom_word(data, 0x0807AF1C)
    if table_base != 0x0827D3A8 or max_angle != 0x00000167 or wrap_delta != 0xFFFFFE98:
        raise ValueError("unexpected trig lookup literals")
    table = [rom_halfword(data, table_base + index * 2) for index in range(181)]
    return [("thumb_trig_lookup", THUMB_TRIG_LOOKUP_PC, table)]


def decode_thumb_first_math_return_block(data):
    expected = {
        0x0807AF38: 0x0400,
        0x0807AF3A: 0x1400,
        0x0807AF3C: 0xBD00,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_first_math_return", THUMB_FIRST_MATH_RETURN_PC)]


def decode_thumb_unit_geometry_after_first_math_block(data):
    expected = {
        0x08014C14: 0x0400,
        0x08014C16: 0x1380,
        0x08014C18: 0x2200,
        0x08014C1A: 0x5EA1,
        0x08014C1C: 0xF066,
        0x08014C1E: 0xFC34,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08014C1C), rom_halfword(data, 0x08014C1E), 0x08014C1C) | 1
    return [("thumb_unit_geometry_after_first_math", THUMB_UNIT_GEOMETRY_AFTER_FIRST_MATH_PC, target)]


def decode_thumb_signed_divide_helper_block(data):
    expected = {
        0x0807B488: 0x2900,
        0x0807B48A: 0xD041,
        0x0807B48C: 0xB410,
        0x0807B48E: 0x1C04,
        0x0807B490: 0x404C,
        0x0807B492: 0x46A4,
        0x0807B494: 0x2301,
        0x0807B496: 0x2200,
        0x0807B498: 0x2900,
        0x0807B49A: 0xD500,
        0x0807B49C: 0x4249,
        0x0807B49E: 0x2800,
        0x0807B4A0: 0xD500,
        0x0807B4A2: 0x4240,
        0x0807B4A4: 0x4288,
        0x0807B4A6: 0xD32C,
        0x0807B4A8: 0x2401,
        0x0807B4AA: 0x0724,
        0x0807B4AC: 0x42A1,
        0x0807B4AE: 0xD204,
        0x0807B4B0: 0x4281,
        0x0807B4B2: 0xD202,
        0x0807B4B4: 0x0109,
        0x0807B4B6: 0x011B,
        0x0807B4B8: 0xE7F8,
        0x0807B4BA: 0x00E4,
        0x0807B4BC: 0x42A1,
        0x0807B4BE: 0xD204,
        0x0807B4C0: 0x4281,
        0x0807B4C2: 0xD202,
        0x0807B4C4: 0x0049,
        0x0807B4C6: 0x005B,
        0x0807B4C8: 0xE7F8,
        0x0807B4CA: 0x4288,
        0x0807B4CC: 0xD301,
        0x0807B4CE: 0x1A40,
        0x0807B4D0: 0x431A,
        0x0807B4D2: 0x084C,
        0x0807B4D4: 0x42A0,
        0x0807B4D6: 0xD302,
        0x0807B4D8: 0x1B00,
        0x0807B4DA: 0x085C,
        0x0807B4DC: 0x4322,
        0x0807B4DE: 0x088C,
        0x0807B4E0: 0x42A0,
        0x0807B4E2: 0xD302,
        0x0807B4E4: 0x1B00,
        0x0807B4E6: 0x089C,
        0x0807B4E8: 0x4322,
        0x0807B4EA: 0x08CC,
        0x0807B4EC: 0x42A0,
        0x0807B4EE: 0xD302,
        0x0807B4F0: 0x1B00,
        0x0807B4F2: 0x08DC,
        0x0807B4F4: 0x4322,
        0x0807B4F6: 0x2800,
        0x0807B4F8: 0xD003,
        0x0807B4FA: 0x091B,
        0x0807B4FC: 0xD001,
        0x0807B4FE: 0x0909,
        0x0807B500: 0xE7E3,
        0x0807B502: 0x1C10,
        0x0807B504: 0x4664,
        0x0807B506: 0x2C00,
        0x0807B508: 0xD500,
        0x0807B50A: 0x4240,
        0x0807B50C: 0xBC10,
        0x0807B50E: 0x46F7,
        0x0807B510: 0xB500,
        0x0807B512: 0xF000,
        0x0807B514: 0xF803,
        0x0807B516: 0x2000,
        0x0807B518: 0xBD00,
        0x0807B51C: 0x46F7,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_signed_divide_helper", THUMB_SIGNED_DIVIDE_HELPER_PC)]


def decode_thumb_unit_geometry_after_first_divide_block(data):
    expected = {
        0x08014C20: 0x4680,
        0x08014C22: 0x0400,
        0x08014C24: 0x1400,
        0x08014C26: 0x4680,
        0x08014C28: 0x2104,
        0x08014C2A: 0x5E60,
        0x08014C2C: 0xF066,
        0x08014C2E: 0xF952,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x08014C2C), rom_halfword(data, 0x08014C2E), 0x08014C2C) | 1
    return [("thumb_unit_geometry_after_first_divide", THUMB_UNIT_GEOMETRY_AFTER_FIRST_DIVIDE_PC, target)]


def decode_thumb_callback_register_block(data):
    expected = {
        0x0807AE50: 0x4A02,
        0x0807AE52: 0x0080,
        0x0807AE54: 0x1880,
        0x0807AE56: 0x6001,
        0x0807AE58: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [("thumb_callback_register", THUMB_CALLBACK_REGISTER_PC, rom_word(data, 0x0807AE5C))]


def decode_thumb_display_reset_wrapper_block(data):
    expected = {
        0x08010968: 0xB500,
        0x0801096A: 0xF7FF,
        0x0801096C: 0xFDEB,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0801096A), rom_halfword(data, 0x0801096C), 0x0801096A) | 1
    return [("thumb_display_reset_wrapper", THUMB_DISPLAY_RESET_WRAPPER_PC, target)]


def decode_thumb_display_setup_block(data):
    expected = {
        0x0807AE14: 0xB510,
        0x0807AE16: 0x2000,
        0x0807AE18: 0x2100,
        0x0807AE1A: 0xF000,
        0x0807AE1C: 0xF821,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0807AE1A), rom_halfword(data, 0x0807AE1C), 0x0807AE1A) | 1
    return [("thumb_display_setup_prefix", THUMB_DISPLAY_SETUP_PC, target)]


def decode_thumb_display_setup_continuation_block(data):
    expected = {
        0x0807AE1E: 0x4807,
        0x0807AE20: 0x4907,
        0x0807AE22: 0x221E,
        0x0807AE24: 0xF000,
        0x0807AE26: 0xFA5A,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0807AE24), rom_halfword(data, 0x0807AE26), 0x0807AE24) | 1
    return [(
        "thumb_display_setup_continuation",
        THUMB_DISPLAY_SETUP_CONTINUATION_PC,
        rom_word(data, 0x0807AE3C),
        rom_word(data, 0x0807AE40),
        target,
    )]


def decode_thumb_display_second_dma_block(data):
    expected = {
        0x0807AE28: 0x4806,
        0x0807AE2A: 0x4C07,
        0x0807AE2C: 0x2280,
        0x0807AE2E: 0x0052,
        0x0807AE30: 0x1C21,
        0x0807AE32: 0xF000,
        0x0807AE34: 0xFA53,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    target = thumb_bl_target(rom_halfword(data, 0x0807AE32), rom_halfword(data, 0x0807AE34), 0x0807AE32) | 1
    return [(
        "thumb_display_second_dma",
        THUMB_DISPLAY_SECOND_DMA_PC,
        rom_word(data, 0x0807AE44),
        rom_word(data, 0x0807AE48),
        target,
    )]


def decode_thumb_display_setup_tail_block(data):
    expected = {
        0x0807AE36: 0x4805,
        0x0807AE38: 0x6004,
        0x0807AE3A: 0xBD10,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_display_setup_tail",
        THUMB_DISPLAY_SETUP_TAIL_PC,
        rom_word(data, 0x0807AE4C),
    )]


def decode_thumb_dma_helper_block(data):
    expected = {
        0x0807B2DC: 0x0412,
        0x0807B2DE: 0x0C12,
        0x0807B2E0: 0x4B08,
        0x0807B2E2: 0x8018,
        0x0807B2E4: 0x3302,
        0x0807B2E6: 0x1400,
        0x0807B2E8: 0x8018,
        0x0807B2EA: 0x4807,
        0x0807B2EC: 0x8001,
        0x0807B2EE: 0x3002,
        0x0807B2F0: 0x1409,
        0x0807B2F2: 0x8001,
        0x0807B2F4: 0x3002,
        0x0807B2F6: 0x8002,
        0x0807B2F8: 0x4904,
        0x0807B2FA: 0x2280,
        0x0807B2FC: 0x0212,
        0x0807B2FE: 0x1C10,
        0x0807B300: 0x8008,
        0x0807B302: 0x4770,
    }
    for address, halfword in expected.items():
        actual = rom_halfword(data, address)
        if actual != halfword:
            raise ValueError(
                f"expected 0x{halfword:04X} at 0x{address:08X}, got 0x{actual:04X}"
            )
    return [(
        "thumb_dma_helper",
        THUMB_DMA_HELPER_PC,
        rom_word(data, 0x0807B304),
        rom_word(data, 0x0807B308),
        rom_word(data, 0x0807B30C),
    )]


def emit_header(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "#pragma once\n\n"
        "#include \"aw/cpu_state.hpp\"\n\n"
        "namespace aw::generated {\n\n"
        "void block_080000C0(aw::CpuState& state);\n"
        "void block_08010444(aw::CpuState& state);\n"
        "void block_080104B0(aw::CpuState& state);\n"
        "void block_080104D4(aw::CpuState& state);\n"
        "void block_08010544(aw::CpuState& state);\n"
        "void block_0801055A(aw::CpuState& state);\n"
        "void block_0801055E(aw::CpuState& state);\n"
        "void block_08010562(aw::CpuState& state);\n"
        "void block_08010574(aw::CpuState& state);\n"
        "void block_080106A4(aw::CpuState& state);\n"
        "void block_08010968(aw::CpuState& state);\n"
        "void block_0801096E(aw::CpuState& state);\n"
        "void block_08010974(aw::CpuState& state);\n"
        "void block_0801097A(aw::CpuState& state);\n"
        "void block_0801097E(aw::CpuState& state);\n"
        "void block_08010A78(aw::CpuState& state);\n"
        "void block_08012CF0(aw::CpuState& state);\n"
        "void block_0801A768(aw::CpuState& state);\n"
        "void block_0801A784(aw::CpuState& state);\n"
        "void block_0801A78A(aw::CpuState& state);\n"
        "void block_0801AFC8(aw::CpuState& state);\n"
        "void block_0801B2B8(aw::CpuState& state);\n"
        "void block_0801AC1C(aw::CpuState& state);\n"
        "void block_0801AD38(aw::CpuState& state);\n"
        "void block_08015D1C(aw::CpuState& state);\n"
        "void block_08015FB4(aw::CpuState& state);\n"
        "void block_08015FC0(aw::CpuState& state);\n"
        "void block_08015FCA(aw::CpuState& state);\n"
        "void block_0803F87C(aw::CpuState& state);\n"
        "void block_0803F886(aw::CpuState& state);\n"
        "void block_0803F890(aw::CpuState& state);\n"
        "void block_0803F898(aw::CpuState& state);\n"
        "void block_0803F8B0(aw::CpuState& state);\n"
        "void block_080386E4(aw::CpuState& state);\n"
        "void block_0803872A(aw::CpuState& state);\n"
        "void block_08038734(aw::CpuState& state);\n"
        "void block_08038750(aw::CpuState& state);\n"
        "void block_08038754(aw::CpuState& state);\n"
        "void block_08038758(aw::CpuState& state);\n"
        "void block_0803875E(aw::CpuState& state);\n"
        "void block_08038762(aw::CpuState& state);\n"
        "void block_08038766(aw::CpuState& state);\n"
        "void block_0803876E(aw::CpuState& state);\n"
        "void block_080387FC(aw::CpuState& state);\n"
        "void block_080386B4(aw::CpuState& state);\n"
        "void block_080385CC(aw::CpuState& state);\n"
        "void block_0800F1B0(aw::CpuState& state);\n"
        "void block_0800F170(aw::CpuState& state);\n"
        "void block_0800F1B6(aw::CpuState& state);\n"
        "void block_0800F1CA(aw::CpuState& state);\n"
        "void block_080385D8(aw::CpuState& state);\n"
        "void block_08038260(aw::CpuState& state);\n"
        "void block_080385DE(aw::CpuState& state);\n"
        "void block_0803826C(aw::CpuState& state);\n"
        "void block_080385E4(aw::CpuState& state);\n"
        "void block_080385BC(aw::CpuState& state);\n"
        "void block_08038600(aw::CpuState& state);\n"
        "void block_0801B820(aw::CpuState& state);\n"
        "void block_0801B840(aw::CpuState& state);\n"
        "void block_08038604(aw::CpuState& state);\n"
        "void block_08034C98(aw::CpuState& state);\n"
        "void block_08018AAC(aw::CpuState& state);\n"
        "void block_08034CAC(aw::CpuState& state);\n"
        "void block_0801F114(aw::CpuState& state);\n"
        "void block_08034CB0(aw::CpuState& state);\n"
        "void block_08038608(aw::CpuState& state);\n"
        "void block_08034C74(aw::CpuState& state);\n"
        "void block_08034BA8(aw::CpuState& state);\n"
        "void block_08024EC4(aw::CpuState& state);\n"
        "void block_08034BFA(aw::CpuState& state);\n"
        "void block_08034C86(aw::CpuState& state);\n"
        "void block_08034C64(aw::CpuState& state);\n"
        "void block_08034C8A(aw::CpuState& state);\n"
        "void block_0803860C(aw::CpuState& state);\n"
        "void block_080149E0(aw::CpuState& state);\n"
        "void block_0801A69C(aw::CpuState& state);\n"
        "void block_080149EC(aw::CpuState& state);\n"
        "void block_08014C98(aw::CpuState& state);\n"
        "void block_08014BF8(aw::CpuState& state);\n"
        "void block_0807AF30(aw::CpuState& state);\n"
        "void block_0807AED4(aw::CpuState& state);\n"
        "void block_0807AF38(aw::CpuState& state);\n"
        "void block_08014C14(aw::CpuState& state);\n"
        "void block_0807B488(aw::CpuState& state);\n"
        "void block_08014C20(aw::CpuState& state);\n"
        "void block_080796C4(aw::CpuState& state);\n"
        "void block_0807ACE8(aw::CpuState& state);\n"
        "void block_0807ACF4(aw::CpuState& state);\n"
        "void block_0807AD00(aw::CpuState& state);\n"
        "void block_0807AD0A(aw::CpuState& state);\n"
        "void block_0807AD0E(aw::CpuState& state);\n"
        "void block_0807AD10(aw::CpuState& state);\n"
        "void block_0807AD16(aw::CpuState& state);\n"
        "void block_0807AE14(aw::CpuState& state);\n"
        "void block_0807AE1E(aw::CpuState& state);\n"
        "void block_0807AE28(aw::CpuState& state);\n"
        "void block_0807AE36(aw::CpuState& state);\n"
        "void block_0807AE50(aw::CpuState& state);\n"
        "void block_0807AE60(aw::CpuState& state);\n"
        "void block_0807AFF4(aw::CpuState& state);\n"
        "void block_0807B2DC(aw::CpuState& state);\n"
        "void dispatch_one(aw::CpuState& state);\n\n"
        "}  // namespace aw::generated\n",
        encoding="utf-8",
    )


def emit_thumb_ops(lines, ops):
    for op in ops:
        kind = op[0]
        if kind == "thumb_push_lr":
            _, pc = op
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[14]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: push {{lr}}\");")
        elif kind == "thumb_movs_imm":
            _, pc, rd, value = op
            lines.append(f"  state.regs[{rd}] = {hex32(value)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: movs {reg_name(rd)}, #{hex32(value)}\");")
        elif kind == "thumb_ldr_literal":
            _, pc, rd, literal_address, value = op
            lines.append(f"  state.regs[{rd}] = {hex32(value)}u;")
            lines.append(
                f"  aw::trace(state, \"{pc:08X}: ldr {reg_name(rd)}, "
                f"[pc] ; [{literal_address:08X}] = {hex32(value)}\");"
            )
        elif kind == "thumb_bl":
            _, pc, target, return_address = op
            lines.append(f"  state.regs[14] = {hex32(return_address)}u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        elif kind == "thumb_pop_pc":
            _, pc = op
            lines.append("  state.regs[15] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: pop {{pc}}\");")
            lines.append("  aw::stop_at(state, state.regs[15]);")
        else:
            raise ValueError(f"internal generator error: unknown Thumb op {kind}")


def emit_source(path, arm_ops, thumb_display_zero_helper_ops, thumb_display_short_zero_helper_ops, thumb_display_flag_reset_helper_ops, thumb_register_reset_ops, thumb_register_reset_after_zero_ops, thumb_register_reset_after_second_zero_ops, thumb_register_reset_return_ops, thumb_register_sync_ops, thumb_second_register_sync_ops, thumb_seed_store_ops, thumb_display_reset_wrapper_ops, thumb_display_reset_wrapper_return_ops, thumb_input_reset_wrapper_ops, thumb_input_reset_after_register_sync_ops, thumb_input_reset_tail_ops, thumb_alloc_init_ops, thumb_global_init_ops, thumb_global_init_after_save_ops, thumb_global_init_return_ops, thumb_save_probe_ops, thumb_table_init_ops, thumb_object_state_check_ops, thumb_state_query_ops, thumb_object_copy_ops, thumb_object_setup_wrapper_ops, thumb_object_setup_continuation_ops, thumb_object_setup_return_ops, thumb_slot_init_ops, thumb_slot_loop_continuation_ops, thumb_slot_init_return_ops, thumb_slot_helper_ops, thumb_slot_helper_after_query_ops, thumb_large_boot_ops, thumb_large_boot_after_display_ops, thumb_large_boot_alloc_result_ops, thumb_large_boot_after_global_init_ops, thumb_large_boot_after_object_setup_ops, thumb_large_boot_after_slot_init_ops, thumb_large_boot_after_seed_ops, thumb_large_boot_after_display_reset_ops, thumb_large_boot_after_input_reset_ops, thumb_large_boot_default_state_ops, thumb_large_boot_save_path_ops, thumb_refresh_init_wrapper_ops, thumb_system_init_ops, thumb_engine_clear_ops, thumb_engine_register_reset_ops, thumb_engine_clear_after_register_reset_ops, thumb_engine_clear_return_ops, thumb_system_init_after_engine_clear_ops, thumb_state_store_first_ops, thumb_system_init_after_first_state_store_ops, thumb_state_store_second_ops, thumb_system_init_clear_globals_ops, thumb_system_init_local_reset_ops, thumb_system_init_dma_copy_call_ops, thumb_dma_setup_ops, thumb_bios_cpuset_boot_ops, thumb_dma_setup_return_ops, thumb_system_init_next_call_ops, thumb_next_initializer_ops, thumb_byte_reset_helper_ops, thumb_next_initializer_after_reset_ops, thumb_table_copy_helper_ops, thumb_next_initializer_return_ops, thumb_system_init_mode_call_ops, thumb_mode_initializer_ops, thumb_defaults_initializer_ops, thumb_order_helper_ops, thumb_defaults_after_order_ops, thumb_mode_initializer_after_defaults_ops, thumb_mode_clear_helper_ops, thumb_mode_initializer_return_ops, thumb_system_init_unit_call_ops, thumb_unit_initializer_ops, thumb_global_table_reset_ops, thumb_unit_initializer_after_reset_ops, thumb_unit_entry_setup_ops, thumb_unit_geometry_helper_ops, thumb_callback_register_ops, thumb_display_setup_ops, thumb_display_setup_continuation_ops, thumb_display_second_dma_ops, thumb_display_setup_tail_ops, thumb_dma_helper_ops, thumb_entry_ops, thumb_entry_second_call_ops, thumb_helper_ops, thumb_continuation_ops, thumb_outer_return_ops, thumb_init_copy_ops, thumb_copy_return_ops, thumb_hardware_ops, thumb_struct_init_ops, thumb_first_math_wrapper_ops, thumb_trig_lookup_ops, thumb_first_math_return_ops, thumb_unit_geometry_after_first_math_ops, thumb_signed_divide_helper_ops, thumb_unit_geometry_after_first_divide_ops):
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "#include \"aw/generated_blocks.hpp\"",
        "",
        "#include \"aw/cpu_state.hpp\"",
        "#include \"aw/memory.hpp\"",
        "",
        "#include <stdexcept>",
        "",
        "namespace aw::generated {",
        "",
        "void block_080000C0(aw::CpuState& state) {",
        "  state.regs[15] = 0x080000C0u;",
        "  aw::trace(state, \"Executing generated block 0x080000C0\");",
    ]

    for op in arm_ops:
        kind = op[0]
        if kind == "mov_imm":
            _, pc, rd, value = op
            lines.append(f"  state.regs[{rd}] = {hex32(value)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: mov {reg_name(rd)}, #{hex32(value)}\");")
        elif kind == "msr_cpsr_fc_r0":
            _, pc = op
            lines.append("  state.cpsr = state.regs[0];")
            lines.append(f"  aw::trace(state, \"{pc:08X}: msr cpsr_fc, r0\");")
        elif kind == "ldr_literal":
            _, pc, rd, literal_address, value = op
            lines.append(f"  state.regs[{rd}] = {hex32(value)}u;")
            lines.append(
                f"  aw::trace(state, \"{pc:08X}: ldr {reg_name(rd)}, "
                f"[pc] ; [{literal_address:08X}] = {hex32(value)}\");"
            )
        elif kind == "mov_lr_pc":
            _, pc, value = op
            lines.append(f"  state.regs[14] = {hex32(value)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: mov lr, pc ; lr = {hex32(value)}\");")
        elif kind == "bx":
            _, pc, rm = op
            lines.append(f"  aw::trace(state, \"{pc:08X}: bx {reg_name(rm)}\");")
            lines.append(f"  state.regs[15] = state.regs[{rm}];")
            lines.append(f"  aw::stop_at(state, state.regs[{rm}]);")
        else:
            raise ValueError(f"internal generator error: unknown op {kind}")

    lines += [
        "}",
        "",
        "void block_08010444(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08010445u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08010444\");",
    ]

    for op in thumb_display_zero_helper_ops:
        kind = op[0]
        if kind == "thumb_display_zero_helper":
            _, pc, addresses = op
            lines.append("  state.regs[1] = 0;")
            for address in addresses:
                lines.append(f"  state.regs[0] = {hex32(address)}u;")
                lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[1]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: zero display globals\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"0801047A: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown display zero helper op {kind}")

    lines += [
        "}",
        "",
        "void block_080104B0(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080104B1u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080104B0\");",
    ]

    for op in thumb_display_short_zero_helper_ops:
        kind = op[0]
        if kind == "thumb_display_short_zero_helper":
            _, pc, addresses = op
            lines.append("  state.regs[1] = 0;")
            for address in addresses:
                lines.append(f"  state.regs[0] = {hex32(address)}u;")
                lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[1]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: zero short display globals\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"080104C2: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown display short zero helper op {kind}")

    lines += [
        "}",
        "",
        "void block_080104D4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080104D5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080104D4\");",
    ]

    for op in thumb_display_flag_reset_helper_ops:
        kind = op[0]
        if kind == "thumb_display_flag_reset_helper":
            _, pc, flag_base, byte_addresses, halfword_addresses = op
            lines.append(f"  state.regs[2] = {hex32(flag_base)}u;")
            lines.append("  state.regs[0] = aw::read8(state.memory, state.regs[2] + 1u);")
            lines.append("  state.regs[0] &= 0x1Fu;")
            lines.append("  aw::write8(state.memory, state.regs[2] + 1u, static_cast<std::uint8_t>(state.regs[0]));")
            lines.append("  state.regs[1] = 0;")
            for address in byte_addresses:
                lines.append(f"  state.regs[0] = {hex32(address)}u;")
                lines.append("  aw::write8(state.memory, state.regs[0], static_cast<std::uint8_t>(state.regs[1]));")
            for address in halfword_addresses:
                lines.append(f"  state.regs[0] = {hex32(address)}u;")
                lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[1]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: reset display flag globals\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"08010516: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown display flag reset helper op {kind}")

    lines += [
        "}",
        "",
        "void block_08010544(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08010545u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08010544\");",
    ]

    for op in thumb_register_reset_ops:
        kind = op[0]
        if kind == "thumb_register_reset":
            _, pc, display_flag, reset_global_1, reset_global_2, target = op
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[14]);")
            lines.append(f"  state.regs[1] = {hex32(display_flag)}u;")
            lines.append("  state.regs[0] = 0x80u;")
            lines.append("  aw::write16(state.memory, state.regs[1], static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  state.regs[0] = {hex32(reset_global_1)}u;")
            lines.append("  state.regs[1] = 0;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[1]));")
            lines.append(f"  state.regs[0] = {hex32(reset_global_2)}u;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[1]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: reset display globals\");")
            lines.append("  state.regs[14] = 0x0801055Bu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08010556: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown register reset op {kind}")

    lines += [
        "}",
        "",
        "void block_0801055A(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801055Bu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801055A\");",
    ]

    for op in thumb_register_reset_after_zero_ops:
        kind = op[0]
        if kind == "thumb_register_reset_after_zero":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x0801055Fu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown register reset after zero op {kind}")

    lines += [
        "}",
        "",
        "void block_0801055E(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801055Fu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801055E\");",
    ]

    for op in thumb_register_reset_after_second_zero_ops:
        kind = op[0]
        if kind == "thumb_register_reset_after_second_zero":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08010563u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown register reset after second zero op {kind}")

    lines += [
        "}",
        "",
        "void block_08010562(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08010563u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08010562\");",
    ]

    for op in thumb_register_reset_return_ops:
        kind = op[0]
        if kind == "thumb_register_reset_return":
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append("  aw::trace(state, \"08010562: pop {r0}; bx r0\");")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown register reset return op {kind}")

    lines += [
        "}",
        "",
        "void block_08010574(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08010575u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08010574\");",
    ]

    for op in thumb_register_sync_ops:
        kind = op[0]
        if kind == "thumb_register_sync":
            (_, pc, halfword_copies, blend_dest, blend_low_source, blend_high_dest,
             blend_high_source, word_dest, first_word_source, second_word_source) = op
            for source, destination in halfword_copies:
                lines.append(f"  state.regs[0] = aw::read16(state.memory, {hex32(source)}u);")
                lines.append(f"  aw::write16(state.memory, {hex32(destination)}u, static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  state.regs[1] = aw::read16(state.memory, {hex32(blend_high_source)}u);")
            lines.append("  state.regs[1] <<= 8u;")
            lines.append(f"  state.regs[0] = aw::read16(state.memory, {hex32(blend_low_source)}u);")
            lines.append("  state.regs[0] += state.regs[1];")
            lines.append(f"  aw::write16(state.memory, {hex32(blend_dest)}u, static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  state.regs[2] = {hex32(word_dest)}u;")
            lines.append(f"  state.regs[1] = {hex32(first_word_source)}u;")
            for offset in range(0, 16, 4):
                lines.append(f"  state.regs[0] = aw::read32(state.memory, state.regs[1] + {offset}u);")
                lines.append(f"  aw::write32(state.memory, state.regs[2] + {offset}u, state.regs[0]);")
            lines.append("  state.regs[2] += 16u;")
            lines.append(f"  state.regs[1] = {hex32(second_word_source)}u;")
            for offset in range(0, 16, 4):
                lines.append(f"  state.regs[0] = aw::read32(state.memory, state.regs[1] + {offset}u);")
                lines.append(f"  aw::write32(state.memory, state.regs[2] + {offset}u, state.regs[0]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: sync display registers\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"08010642: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown register sync op {kind}")

    lines += [
        "}",
        "",
        "void block_080106A4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080106A5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080106A4\");",
    ]

    for op in thumb_second_register_sync_ops:
        kind = op[0]
        if kind == "thumb_second_register_sync":
            _, pc, first_halfword_pairs, byte_pairs, second_halfword_pairs = op
            for destination, source in first_halfword_pairs:
                lines.append(f"  state.regs[0] = aw::read16(state.memory, {hex32(source)}u);")
                lines.append(f"  aw::write16(state.memory, {hex32(destination)}u, static_cast<std::uint16_t>(state.regs[0]));")
            for destination, source in byte_pairs:
                lines.append(f"  state.regs[0] = aw::read8(state.memory, {hex32(source)}u);")
                lines.append(f"  aw::write8(state.memory, {hex32(destination)}u, static_cast<std::uint8_t>(state.regs[0]));")
            for destination, source in second_halfword_pairs:
                lines.append(f"  state.regs[0] = aw::read16(state.memory, {hex32(source)}u);")
                lines.append(f"  aw::write16(state.memory, {hex32(destination)}u, static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: sync display shadow globals\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"08010804: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown second register sync op {kind}")

    lines += [
        "}",
        "",
        "void block_08010968(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08010969u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08010968\");",
    ]

    for op in thumb_display_reset_wrapper_ops:
        kind = op[0]
        if kind == "thumb_display_reset_wrapper":
            _, pc, target = op
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[14]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: push {{lr}}\");")
            lines.append("  state.regs[14] = 0x0801096Fu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0801096A: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown display reset wrapper op {kind}")

    lines += [
        "}",
        "",
        "void block_0801096E(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801096Fu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801096E\");",
    ]

    for op in thumb_display_reset_wrapper_return_ops:
        kind = op[0]
        if kind == "thumb_display_reset_wrapper_return":
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append("  aw::trace(state, \"0801096E: pop {r0}; bx r0\");")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown display reset wrapper return op {kind}")

    lines += [
        "}",
        "",
        "void block_08010974(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08010975u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08010974\");",
    ]

    for op in thumb_input_reset_wrapper_ops:
        kind = op[0]
        if kind == "thumb_input_reset_wrapper":
            _, pc, target = op
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[14]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: push {{lr}}\");")
            lines.append("  state.regs[14] = 0x0801097Bu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08010976: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown input reset wrapper op {kind}")

    lines += [
        "}",
        "",
        "void block_0801097A(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801097Bu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801097A\");",
    ]

    for op in thumb_input_reset_after_register_sync_ops:
        kind = op[0]
        if kind == "thumb_input_reset_after_register_sync":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x0801097Fu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown input reset after register sync op {kind}")

    lines += [
        "}",
        "",
        "void block_0801097E(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801097Fu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801097E\");",
    ]

    for op in thumb_input_reset_tail_ops:
        kind = op[0]
        if kind == "thumb_input_reset_tail":
            _, pc, destination_base, byte_sources, halfword_sources = op
            byte_offsets = [0, 1, 4, 5, 2, 3, 6, 7]
            for offset, source in zip(byte_offsets, byte_sources):
                lines.append(f"  state.regs[0] = aw::read8(state.memory, {hex32(source)}u);")
                lines.append(f"  aw::write8(state.memory, {hex32(destination_base + offset)}u, static_cast<std::uint8_t>(state.regs[0]));")
            for offset, source in zip([8, 10], halfword_sources):
                lines.append(f"  state.regs[0] = aw::read16(state.memory, {hex32(source)}u);")
                lines.append(f"  aw::write16(state.memory, {hex32(destination_base + offset)}u, static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: sync input tail registers\");")
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append("  aw::trace(state, \"080109CE: pop {r0}; bx r0\");")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown input reset tail op {kind}")

    lines += [
        "}",
        "",
        "void block_08010A78(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08010A79u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08010A78\");",
    ]

    for op in thumb_seed_store_ops:
        kind = op[0]
        if kind == "thumb_seed_store":
            _, pc, seed_global = op
            lines.append(f"  state.regs[1] = {hex32(seed_global)}u;")
            lines.append("  aw::write32(state.memory, state.regs[1], state.regs[0]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: store seed literal\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"08010A7C: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown seed store op {kind}")

    lines += [
        "}",
        "",
        "void block_08012CF0(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08012CF1u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08012CF0\");",
    ]

    for op in thumb_alloc_init_ops:
        kind = op[0]
        if kind == "thumb_alloc_init":
            _, pc, global_pointer_address, helper_target = op
            lines.append("  const std::uint32_t saved_r4 = state.regs[4];")
            lines.append("  const std::uint32_t saved_lr = state.regs[14];")
            lines.append("  state.regs[13] -= 8u;")
            lines.append("  aw::write32(state.memory, state.regs[13], saved_r4);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, saved_lr);")
            lines.append(f"  state.regs[4] = {hex32(global_pointer_address)}u;")
            lines.append(f"  state.regs[14] = 0x08012CF9u;")
            lines.append(f"  aw::trace(state, \"08012CF4: bl {hex32(helper_target)}\");")
            lines.append("  state.regs[2] = state.regs[0];")
            lines.append("  state.regs[3] = state.regs[1];")
            lines.append("  if (state.regs[3] <= 0x1Fu) {")
            lines.append("    state.regs[0] = 0xFFFFFFFFu;")
            lines.append("  } else {")
            lines.append("    state.regs[0] = (state.regs[0] + 0x0Fu) & 0xFFFFFFF0u;")
            lines.append("    state.regs[1] = state.regs[0] - state.regs[2];")
            lines.append("    state.regs[3] -= state.regs[1];")
            lines.append("    state.regs[2] = 0;")
            lines.append("    aw::write32(state.memory, state.regs[0], state.regs[2]);")
            lines.append("    state.regs[1] = state.regs[3] - 0x10u;")
            lines.append("    aw::write32(state.memory, state.regs[0] + 4u, state.regs[1]);")
            lines.append("    aw::write32(state.memory, state.regs[0] + 8u, state.regs[2]);")
            lines.append("    aw::write32(state.memory, state.regs[0] + 12u, state.regs[3]);")
            lines.append("  }")
            lines.append("  aw::trace(state, \"08012CC4: initialize allocator arena\");")
            lines.append("  state.regs[1] = state.regs[0];")
            lines.append("  aw::write32(state.memory, state.regs[4], state.regs[1]);")
            lines.append("  state.regs[0] = 0xFFFFFFFFu;")
            lines.append("  if (state.regs[1] != state.regs[0]) {")
            lines.append("    state.regs[0] = 0;")
            lines.append("  }")
            lines.append(f"  aw::trace(state, \"{pc:08X}: store allocator root and return status\");")
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[1] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[13] += 8u;")
            lines.append("  state.regs[15] = state.regs[1];")
            lines.append("  aw::trace(state, \"08012D12: bx r1\");")
            lines.append("  aw::stop_at(state, state.regs[1]);")
        else:
            raise ValueError(f"internal generator error: unknown allocator init op {kind}")

    lines += [
        "}",
        "",
        "void block_0801A768(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801A769u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801A768\");",
    ]

    for op in thumb_global_init_ops:
        kind = op[0]
        if kind == "thumb_global_init_prefix":
            (_, pc, first_callback_address, second_callback_address, data_pointer_address,
             mode_address, stack_argument_address, target) = op
            lines.append("  state.regs[13] -= 12u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[5]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 8u, state.regs[14]);")
            lines.append("  state.regs[5] = aw::read32(state.memory, state.regs[13] + 12u);")
            lines.append(f"  state.regs[4] = {hex32(first_callback_address)}u;")
            lines.append("  aw::write32(state.memory, state.regs[4], state.regs[0]);")
            lines.append(f"  state.regs[0] = {hex32(second_callback_address)}u;")
            lines.append("  aw::write32(state.memory, state.regs[0], state.regs[1]);")
            lines.append(f"  state.regs[0] = {hex32(data_pointer_address)}u;")
            lines.append("  aw::write32(state.memory, state.regs[0], state.regs[2]);")
            lines.append(f"  state.regs[0] = {hex32(mode_address)}u;")
            lines.append("  aw::write8(state.memory, state.regs[0], static_cast<std::uint8_t>(state.regs[3]));")
            lines.append(f"  state.regs[0] = {hex32(stack_argument_address)}u;")
            lines.append("  aw::write32(state.memory, state.regs[0], state.regs[5]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: store global init parameters\");")
            lines.append("  state.regs[14] = 0x0801A785u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0801A780: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown global init op {kind}")

    lines += [
        "}",
        "",
        "void block_0801A784(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801A785u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801A784\");",
    ]

    for op in thumb_global_init_after_save_ops:
        kind = op[0]
        if kind == "thumb_global_init_after_save":
            _, pc, target = op
            lines.append("  state.regs[0] = 0;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: movs r0, #0\");")
            lines.append("  state.regs[14] = 0x0801A78Bu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0801A786: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown global init after save op {kind}")

    lines += [
        "}",
        "",
        "void block_0801A78A(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801A78Bu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801A78A\");",
    ]

    for op in thumb_global_init_return_ops:
        kind = op[0]
        if kind == "thumb_global_init_return":
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[5] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[13] += 8u;")
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append("  aw::trace(state, \"0801A78A: pop {r4, r5}; pop {r0}; bx r0\");")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown global init return op {kind}")

    lines += [
        "}",
        "",
        "void block_0801AFC8(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801AFC9u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801AFC8\");",
    ]

    for op in thumb_save_probe_ops:
        kind = op[0]
        if kind == "thumb_save_probe":
            _, pc, save_flag_address, helper_target = op
            lines.append("  state.regs[13] -= 8u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[14]);")
            lines.append(f"  state.regs[4] = {hex32(save_flag_address)}u;")
            lines.append(f"  aw::trace(state, \"0801AFCC: host hardware probe for {hex32(helper_target)}\");")
            lines.append("  state.regs[0] = 0;")
            lines.append("  aw::write8(state.memory, state.regs[4], static_cast<std::uint8_t>(state.regs[0]));")
            lines.append("  if ((state.regs[0] << 24u) == 0u) {")
            lines.append("    state.regs[0] = 1u;")
            lines.append("  } else {")
            lines.append("    state.regs[0] = 0u;")
            lines.append("  }")
            lines.append("  aw::write8(state.memory, state.regs[4], static_cast<std::uint8_t>(state.regs[0]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: normalize save probe flag\");")
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[13] += 8u;")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::trace(state, \"0801AFE8: bx r0\");")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown save probe op {kind}")

    lines += [
        "}",
        "",
        "void block_0801B2B8(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801B2B9u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801B2B8\");",
    ]

    for op in thumb_table_init_ops:
        kind = op[0]
        if kind == "thumb_table_init":
            _, pc, selected_index_address, table_base = op
            lines.append("  const std::uint32_t saved_r4 = state.regs[4];")
            lines.append("  const std::uint32_t saved_r5 = state.regs[5];")
            lines.append("  const std::uint32_t saved_r6 = state.regs[6];")
            lines.append("  const std::uint32_t saved_r7 = state.regs[7];")
            lines.append("  const std::uint32_t saved_lr = state.regs[14];")
            lines.append("  state.regs[13] -= 36u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[0]);")
            lines.append(f"  for (std::uint32_t address = {hex32(table_base + 0x40)}u; address <= {hex32(table_base + 0x7C)}u; address += 4u) {{")
            lines.append("    aw::write32(state.memory, address, 0);")
            lines.append("  }")
            lines.append(f"  aw::write32(state.memory, {hex32(selected_index_address)}u, 0);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: initialize global table state\");")
            lines.append("  state.regs[13] += 36u;")
            lines.append("  state.regs[4] = saved_r4;")
            lines.append("  state.regs[5] = saved_r5;")
            lines.append("  state.regs[6] = saved_r6;")
            lines.append("  state.regs[7] = saved_r7;")
            lines.append("  state.regs[0] = saved_lr;")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::trace(state, \"0801B468: bx r0\");")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown table init op {kind}")

    lines += [
        "}",
        "",
        "void block_0801AC1C(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801AC1Du;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801AC1C\");",
    ]

    for op in thumb_object_state_check_ops:
        kind = op[0]
        if kind == "thumb_object_state_check":
            _, pc, object_flags, visible_flags = op
            lines.append("  const std::uint32_t saved_r4 = state.regs[4];")
            lines.append("  const std::uint32_t saved_r5 = state.regs[5];")
            lines.append("  const std::uint32_t saved_r6 = state.regs[6];")
            lines.append("  const std::uint32_t saved_r7 = state.regs[7];")
            lines.append("  const std::uint32_t saved_lr = state.regs[14];")
            lines.append("  state.regs[13] -= 44u;")
            lines.append(f"  for (std::uint32_t i = 0; i < 16u; ++i) {{")
            lines.append(f"    const std::uint8_t bit = static_cast<std::uint8_t>(aw::read8(state.memory, {hex32(object_flags)}u + i) & 0x10u);")
            lines.append(f"    aw::write8(state.memory, {hex32(object_flags)}u + i, static_cast<std::uint8_t>((aw::read8(state.memory, {hex32(object_flags)}u + i) & 0xEFu) | bit));")
            lines.append("  }")
            lines.append(f"  for (std::uint32_t i = 0; i < 16u; ++i) {{")
            lines.append(f"    aw::write8(state.memory, {hex32(visible_flags)}u + i, aw::read8(state.memory, {hex32(visible_flags + 0x10)}u + i));")
            lines.append("  }")
            lines.append("  state.regs[0] = 0;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: refresh object state flags and return status\");")
            lines.append("  state.regs[13] += 44u;")
            lines.append("  state.regs[4] = saved_r4;")
            lines.append("  state.regs[5] = saved_r5;")
            lines.append("  state.regs[6] = saved_r6;")
            lines.append("  state.regs[7] = saved_r7;")
            lines.append("  state.regs[1] = saved_lr;")
            lines.append("  state.regs[15] = state.regs[1];")
            lines.append("  aw::trace(state, \"0801AD1C: bx r1\");")
            lines.append("  aw::stop_at(state, state.regs[1]);")
        else:
            raise ValueError(f"internal generator error: unknown object state check op {kind}")

    lines += [
        "}",
        "",
        "void block_0801AD38(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801AD39u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801AD38\");",
    ]

    for op in thumb_state_query_ops:
        kind = op[0]
        if kind == "thumb_state_query_boot_unavailable":
            _, pc, unavailable_literal, query_target = op
            lines.append("  const std::uint32_t saved_r4 = state.regs[4];")
            lines.append("  const std::uint32_t saved_r5 = state.regs[5];")
            lines.append("  const std::uint32_t saved_r6 = state.regs[6];")
            lines.append("  const std::uint32_t saved_lr = state.regs[14];")
            lines.append("  state.regs[13] -= 16u;")
            lines.append("  state.regs[4] = state.regs[0] & 0xFFu;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: query state table via {hex32(query_target)}\");")
            lines.append(f"  state.regs[1] = {hex32(unavailable_literal)}u;")
            lines.append("  state.regs[0] = 1u;")
            lines.append("  state.regs[13] += 16u;")
            lines.append("  state.regs[4] = saved_r4;")
            lines.append("  state.regs[5] = saved_r5;")
            lines.append("  state.regs[6] = saved_r6;")
            lines.append("  state.regs[1] = saved_lr;")
            lines.append("  state.regs[15] = state.regs[1];")
            lines.append("  aw::trace(state, \"0801AD90: bx r1\");")
            lines.append("  aw::stop_at(state, state.regs[1]);")
        else:
            raise ValueError(f"internal generator error: unknown state query op {kind}")

    lines += [
        "}",
        "",
        "void block_08015D1C(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08015D1Du;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08015D1C\");",
    ]

    for op in thumb_object_copy_ops:
        kind = op[0]
        if kind == "thumb_object_copy":
            _, pc, scratch_base, table_destination, config_destination, config_offset, copy_target = op
            lines.append("  const std::uint32_t saved_r4 = state.regs[4];")
            lines.append("  const std::uint32_t saved_r5 = state.regs[5];")
            lines.append("  const std::uint32_t saved_r6 = state.regs[6];")
            lines.append("  const std::uint32_t saved_r7 = state.regs[7];")
            lines.append("  const std::uint32_t saved_lr = state.regs[14];")
            lines.append("  state.regs[13] -= 24u;")
            lines.append(f"  for (std::uint32_t i = 0; i < 800u; ++i) {{")
            lines.append(f"    aw::write8(state.memory, {hex32(table_destination)}u + i, aw::read8(state.memory, state.regs[0] + i));")
            lines.append("  }")
            lines.append(f"  for (std::uint32_t i = 0; i < 0x1D4u; ++i) {{")
            lines.append(f"    aw::write8(state.memory, {hex32(scratch_base)}u + i, aw::read8(state.memory, state.regs[0] + 0x320u + i));")
            lines.append("  }")
            lines.append(f"  for (std::uint32_t i = 0; i < 36u; ++i) {{")
            lines.append(f"    aw::write8(state.memory, {hex32(config_destination)}u + i, aw::read8(state.memory, state.regs[0] + {hex32(config_offset)}u + i));")
            lines.append("  }")
            lines.append(f"  aw::trace(state, \"{pc:08X}: copy object tables via {hex32(copy_target)}\");")
            lines.append("  state.regs[0] = 0x00000518u;")
            lines.append("  state.regs[13] += 24u;")
            lines.append("  state.regs[4] = saved_r4;")
            lines.append("  state.regs[5] = saved_r5;")
            lines.append("  state.regs[6] = saved_r6;")
            lines.append("  state.regs[7] = saved_r7;")
            lines.append("  state.regs[1] = saved_lr;")
            lines.append("  state.regs[15] = state.regs[1];")
            lines.append("  aw::trace(state, \"08015D94: bx r1\");")
            lines.append("  aw::stop_at(state, state.regs[1]);")
        else:
            raise ValueError(f"internal generator error: unknown object copy op {kind}")

    lines += [
        "}",
        "",
        "void block_08015FB4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08015FB5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08015FB4\");",
    ]

    for op in thumb_object_setup_wrapper_ops:
        kind = op[0]
        if kind == "thumb_object_setup_wrapper":
            _, pc, object_base, target = op
            lines.append("  state.regs[13] -= 8u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[14]);")
            lines.append(f"  state.regs[4] = {hex32(object_base)}u;")
            lines.append("  state.regs[0] = 0;")
            lines.append("  state.regs[1] = state.regs[4];")
            lines.append(f"  aw::trace(state, \"{pc:08X}: push {{r4, lr}}; prepare object state check\");")
            lines.append("  state.regs[14] = 0x08015FC1u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08015FBC: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown object setup wrapper op {kind}")

    lines += [
        "}",
        "",
        "void block_08015FC0(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08015FC1u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08015FC0\");",
    ]

    for op in thumb_object_setup_continuation_ops:
        kind = op[0]
        if kind == "thumb_object_setup_continuation":
            _, pc, target = op
            lines.append("  if (state.regs[0] != 0u) {")
            lines.append("    state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("    state.regs[0] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("    state.regs[13] += 8u;")
            lines.append("    state.regs[15] = state.regs[0];")
            lines.append("    aw::trace(state, \"08015FCA: pop {r4}; pop {r0}; bx r0\");")
            lines.append("    aw::stop_at(state, state.regs[0]);")
            lines.append("    return;")
            lines.append("  }")
            lines.append("  state.regs[0] = state.regs[4];")
            lines.append(f"  aw::trace(state, \"{pc:08X}: state check passed; call object copy\");")
            lines.append("  state.regs[14] = 0x08015FCBu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08015FC6: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown object setup continuation op {kind}")

    lines += [
        "}",
        "",
        "void block_08015FCA(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08015FCBu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08015FCA\");",
    ]

    for op in thumb_object_setup_return_ops:
        kind = op[0]
        if kind == "thumb_object_setup_return":
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[13] += 8u;")
            lines.append("  aw::trace(state, \"08015FCA: pop {r4}; pop {r0}; bx r0\");")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown object setup return op {kind}")

    lines += [
        "}",
        "",
        "void block_0803F87C(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0803F87Du;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0803F87C\");",
    ]

    for op in thumb_slot_init_ops:
        kind = op[0]
        if kind == "thumb_slot_init":
            _, pc, target = op
            lines.append("  state.regs[13] -= 8u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[14]);")
            lines.append("  state.regs[4] = 0;")
            lines.append("  state.regs[0] = state.regs[4];")
            lines.append(f"  aw::trace(state, \"{pc:08X}: push {{r4, lr}}; start slot init loop\");")
            lines.append("  state.regs[14] = 0x0803F887u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0803F882: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown slot init op {kind}")

    lines += [
        "}",
        "",
        "void block_0803F886(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0803F887u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0803F886\");",
    ]

    for op in thumb_slot_loop_continuation_ops:
        kind = op[0]
        if kind == "thumb_slot_loop_continuation":
            _, pc, target = op
            lines.append("  state.regs[0] = (state.regs[4] + 1u) & 0xFFu;")
            lines.append("  state.regs[4] = state.regs[0];")
            lines.append(f"  aw::trace(state, \"{pc:08X}: advance slot init loop\");")
            lines.append("  if (state.regs[4] <= 0x0Bu) {")
            lines.append("    state.regs[0] = state.regs[4];")
            lines.append("    state.regs[14] = 0x0803F887u;")
            lines.append(f"    state.regs[15] = {hex32(target)}u;")
            lines.append(f"    aw::trace(state, \"0803F882: bl {hex32(target)}\");")
            lines.append(f"    aw::stop_at(state, {hex32(target)}u);")
            lines.append("    return;")
            lines.append("  }")
            lines.append("  aw::stop_at(state, 0x0803F891u);")
        else:
            raise ValueError(f"internal generator error: unknown slot loop continuation op {kind}")

    lines += [
        "}",
        "",
        "void block_0803F890(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0803F891u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0803F890\");",
    ]

    for op in thumb_slot_init_return_ops:
        kind = op[0]
        if kind == "thumb_slot_init_return":
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[13] += 8u;")
            lines.append("  aw::trace(state, \"0803F890: pop {r4}; pop {r0}; bx r0\");")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown slot init return op {kind}")

    lines += [
        "}",
        "",
        "void block_0803F898(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0803F899u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0803F898\");",
    ]

    for op in thumb_slot_helper_ops:
        kind = op[0]
        if kind == "thumb_slot_helper_prefix":
            _, pc, object_base, target = op
            lines.append("  state.regs[13] -= 16u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[5]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 8u, state.regs[6]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 12u, state.regs[14]);")
            lines.append("  state.regs[4] = state.regs[0] & 0xFFu;")
            lines.append(f"  state.regs[6] = {hex32(object_base)}u;")
            lines.append("  if (state.regs[4] <= 3u) {")
            lines.append("    state.regs[5] = (state.regs[4] + 5u) & 0xFFu;")
            lines.append("    state.regs[0] = state.regs[5];")
            lines.append(f"    aw::trace(state, \"{pc:08X}: prepare slot state query\");")
            lines.append("    state.regs[14] = 0x0803F8B1u;")
            lines.append(f"    state.regs[15] = {hex32(target)}u;")
            lines.append(f"    aw::trace(state, \"0803F8AC: bl {hex32(target)}\");")
            lines.append(f"    aw::stop_at(state, {hex32(target)}u);")
            lines.append("    return;")
            lines.append("  }")
            lines.append(f"  const std::uint32_t slot = {hex32(object_base)}u + (state.regs[4] << 5u);")
            lines.append("  aw::write8(state.memory, slot + 0x15u, 0xFFu);")
            lines.append("  state.regs[0] = 0;")
            lines.append("  aw::trace(state, \"0803F8B4: mark slot disabled\");")
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[5] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[6] = aw::read32(state.memory, state.regs[13] + 8u);")
            lines.append("  state.regs[1] = aw::read32(state.memory, state.regs[13] + 12u);")
            lines.append("  state.regs[13] += 16u;")
            lines.append("  state.regs[15] = state.regs[1];")
            lines.append("  aw::trace(state, \"0803F920: bx r1\");")
            lines.append("  aw::stop_at(state, state.regs[1]);")
        else:
            raise ValueError(f"internal generator error: unknown slot helper op {kind}")

    lines += [
        "}",
        "",
        "void block_0803F8B0(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0803F8B1u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0803F8B0\");",
    ]

    for op in thumb_slot_helper_after_query_ops:
        kind = op[0]
        if kind == "thumb_slot_helper_after_query":
            _, pc, object_base = op
            lines.append("  if (state.regs[0] == 0u) {")
            lines.append("    aw::stop_at(state, 0x0803F8CDu);")
            lines.append("    return;")
            lines.append("  }")
            lines.append(f"  const std::uint32_t slot = {hex32(object_base)}u + (state.regs[4] << 5u);")
            lines.append("  aw::write8(state.memory, slot + 0x15u, 0xFFu);")
            lines.append("  state.regs[0] = 0;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: mark slot disabled\");")
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[5] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[6] = aw::read32(state.memory, state.regs[13] + 8u);")
            lines.append("  state.regs[1] = aw::read32(state.memory, state.regs[13] + 12u);")
            lines.append("  state.regs[13] += 16u;")
            lines.append("  state.regs[15] = state.regs[1];")
            lines.append("  aw::trace(state, \"0803F920: bx r1\");")
            lines.append("  aw::stop_at(state, state.regs[1]);")
        else:
            raise ValueError(f"internal generator error: unknown slot helper after query op {kind}")

    lines += [
        "}",
        "",
        "void block_080386E4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080386E5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080386E4\");",
    ]

    for op in thumb_large_boot_ops:
        kind = op[0]
        if kind == "thumb_large_boot_prefix":
            (_, pc, dma3_base, dma_control_1, ie_address, io_control_value,
             keyinput_address, key_mask, call_target) = op
            lines.append("  state.regs[13] -= 8u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[14]);")
            lines.append("  state.regs[13] -= 0x10u;")
            lines.append("  state.regs[2] = 0;")
            lines.append("  aw::write32(state.memory, state.regs[13] + 8u, state.regs[2]);")
            lines.append(f"  state.regs[1] = {hex32(dma3_base)}u;")
            lines.append("  state.regs[0] = state.regs[13] + 8u;")
            lines.append("  aw::write32(state.memory, state.regs[1], state.regs[0]);")
            lines.append("  state.regs[0] = 0x03000000u;")
            lines.append("  aw::write32(state.memory, state.regs[1] + 4u, state.regs[0]);")
            lines.append(f"  state.regs[0] = {hex32(dma_control_1)}u;")
            lines.append("  aw::write32(state.memory, state.regs[1] + 8u, state.regs[0]);")
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[1] + 8u);")
            lines.append("  state.regs[0] = state.regs[13] + 12u;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[2]));")
            lines.append("  aw::write32(state.memory, state.regs[1], state.regs[0]);")
            lines.append("  state.regs[0] = 0x02000000u;")
            lines.append("  aw::write32(state.memory, state.regs[1] + 4u, state.regs[0]);")
            lines.append(f"  state.regs[0] = {hex32(ie_address)}u;")
            lines.append("  aw::write32(state.memory, state.regs[1] + 8u, state.regs[0]);")
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[1] + 8u);")
            lines.append(f"  state.regs[1] = {hex32(ie_address)}u;")
            lines.append(f"  state.regs[2] = {hex32(io_control_value)}u;")
            lines.append("  state.regs[0] = state.regs[2];")
            lines.append("  aw::write16(state.memory, state.regs[1], static_cast<std::uint16_t>(state.regs[0]));")
            lines.append("  state.regs[2] = state.regs[13] + 4u;")
            lines.append(f"  state.regs[0] = {hex32(keyinput_address)}u;")
            lines.append("  state.regs[1] = aw::read16(state.memory, state.regs[0]);")
            lines.append(f"  state.regs[3] = {hex32(key_mask)}u;")
            lines.append("  state.regs[0] = state.regs[3];")
            lines.append("  state.regs[0] &= ~state.regs[1];")
            lines.append("  aw::write16(state.memory, state.regs[2], static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: setup DMA, IO, and keyinput locals\");")
            lines.append("  state.regs[14] = 0x0803872Bu;")
            lines.append(f"  state.regs[15] = {hex32(call_target)}u;")
            lines.append(f"  aw::trace(state, \"08038726: bl {hex32(call_target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(call_target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot op {kind}")

    lines += [
        "}",
        "",
        "void block_0803872A(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0803872Bu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0803872A\");",
    ]

    for op in thumb_large_boot_after_display_ops:
        kind = op[0]
        if kind == "thumb_large_boot_after_display":
            _, pc, allocator_base, target = op
            lines.append(f"  state.regs[0] = {hex32(allocator_base)}u;")
            lines.append("  state.regs[1] = 0x00008000u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: ldr r0; movs/lsls r1, #0x8000\");")
            lines.append("  state.regs[14] = 0x08038735u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08038730: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot after display op {kind}")

    lines += [
        "}",
        "",
        "void block_08038734(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08038735u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08038734\");",
    ]

    for op in thumb_large_boot_alloc_result_ops:
        kind = op[0]
        if kind == "thumb_large_boot_alloc_result":
            _, pc, first_callback, second_callback, data_pointer, stack_argument, target = op
            lines.append("  state.regs[1] = 1u;")
            lines.append("  state.regs[1] = 0u - state.regs[1];")
            lines.append(f"  aw::trace(state, \"{pc:08X}: compare allocator result against -1\");")
            lines.append("  if (state.regs[0] == state.regs[1]) {")
            lines.append("    state.regs[14] = 0x08038741u;")
            lines.append("    state.regs[15] = 0x080385C9u;")
            lines.append("    aw::trace(state, \"0803873C: bl 0x080385C9\");")
            lines.append("    aw::stop_at(state, 0x080385C9u);")
            lines.append("    return;")
            lines.append("  }")
            lines.append(f"  state.regs[0] = {hex32(first_callback)}u;")
            lines.append(f"  state.regs[1] = {hex32(second_callback)}u;")
            lines.append(f"  state.regs[2] = {hex32(data_pointer)}u;")
            lines.append(f"  state.regs[3] = {hex32(stack_argument)}u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[3]);")
            lines.append("  state.regs[3] = 2u;")
            lines.append("  aw::trace(state, \"08038740: prepare init call arguments\");")
            lines.append("  state.regs[14] = 0x08038751u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0803874C: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot alloc result op {kind}")

    lines += [
        "}",
        "",
        "void block_08038750(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08038751u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08038750\");",
    ]

    for op in thumb_large_boot_after_global_init_ops:
        kind = op[0]
        if kind == "thumb_large_boot_after_global_init":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08038755u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot after global init op {kind}")

    lines += [
        "}",
        "",
        "void block_08038754(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08038755u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08038754\");",
    ]

    for op in thumb_large_boot_after_object_setup_ops:
        kind = op[0]
        if kind == "thumb_large_boot_after_object_setup":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08038759u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot after object setup op {kind}")

    lines += [
        "}",
        "",
        "void block_08038758(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08038759u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08038758\");",
    ]

    for op in thumb_large_boot_after_slot_init_ops:
        kind = op[0]
        if kind == "thumb_large_boot_after_slot_init":
            _, pc, seed_literal, target = op
            lines.append(f"  state.regs[0] = {hex32(seed_literal)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: ldr r0, seed literal\");")
            lines.append("  state.regs[14] = 0x0803875Fu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0803875A: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot after slot init op {kind}")

    lines += [
        "}",
        "",
        "void block_0803875E(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0803875Fu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0803875E\");",
    ]

    for op in thumb_large_boot_after_seed_ops:
        kind = op[0]
        if kind == "thumb_large_boot_after_seed":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08038763u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot after seed op {kind}")

    lines += [
        "}",
        "",
        "void block_08038762(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08038763u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08038762\");",
    ]

    for op in thumb_large_boot_after_display_reset_ops:
        kind = op[0]
        if kind == "thumb_large_boot_after_display_reset":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08038767u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot after display reset op {kind}")

    lines += [
        "}",
        "",
        "void block_08038766(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08038767u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08038766\");",
    ]

    for op in thumb_large_boot_after_input_reset_ops:
        kind = op[0]
        if kind == "thumb_large_boot_after_input_reset":
            _, pc, callback_target, target = op
            lines.append(f"  state.regs[1] = {hex32(callback_target)}u;")
            lines.append("  state.regs[0] = 0u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: prepare callback register\");")
            lines.append("  state.regs[14] = 0x0803876Fu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0803876A: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot after input reset op {kind}")

    lines += [
        "}",
        "",
        "void block_0803876E(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0803876Fu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0803876E\");",
    ]

    for op in thumb_large_boot_default_state_ops:
        kind = op[0]
        if kind == "thumb_large_boot_default_state":
            _, pc, state_base, default_target, true_target = op
            lines.append(f"  state.regs[2] = {hex32(state_base)}u;")
            lines.append("  state.regs[0] = state.regs[2] + 0x85u;")
            lines.append("  state.regs[1] = 0u;")
            lines.append("  aw::write8(state.memory, state.regs[0], static_cast<std::uint8_t>(state.regs[1]));")
            lines.append("  state.regs[0] = state.regs[2] + 0xD0u;")
            lines.append("  state.regs[1] = 3u;")
            lines.append("  aw::write8(state.memory, state.regs[0] + 0x1Du, static_cast<std::uint8_t>(state.regs[1]));")
            lines.append("  state.regs[0] = state.regs[2] + 0x138u;")
            lines.append("  state.regs[1] = 8u;")
            lines.append("  aw::write8(state.memory, state.regs[0] + 0x1Du, static_cast<std::uint8_t>(state.regs[1]));")
            lines.append("  state.regs[0] = state.regs[2] + 0x1A0u;")
            lines.append("  state.regs[1] = 6u;")
            lines.append("  aw::write8(state.memory, state.regs[0] + 0x1Du, static_cast<std::uint8_t>(state.regs[1]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: write boot state bytes\");")
            lines.append("  state.regs[2] = 0u;")
            lines.append("  state.regs[0] = state.regs[13] + 4u;")
            lines.append("  state.regs[1] = aw::read16(state.memory, state.regs[0]);")
            lines.append("  state.regs[0] = state.regs[1] & 0xFu;")
            lines.append("  if (state.regs[0] != 0xFu) {")
            lines.append("    state.regs[0] = state.regs[13] + 4u;")
            lines.append("    state.regs[1] = aw::read16(state.memory, state.regs[0]);")
            lines.append("    state.regs[0] = 0x214u;")
            lines.append("    if (state.regs[1] == state.regs[0]) {")
            lines.append("      state.regs[2] = 1u;")
            lines.append("    }")
            lines.append("  }")
            lines.append("  if (state.regs[2] == 0u) {")
            lines.append(f"    state.regs[15] = {hex32(default_target)}u;")
            lines.append("    aw::trace(state, \"080387B6: beq 0x080387FD\");")
            lines.append(f"    aw::stop_at(state, {hex32(default_target)}u);")
            lines.append("    return;")
            lines.append("  }")
            lines.append("  state.regs[14] = 0x080387BDu;")
            lines.append(f"  state.regs[15] = {hex32(true_target)}u;")
            lines.append(f"  aw::trace(state, \"080387B8: bl {hex32(true_target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(true_target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot default state op {kind}")

    lines += [
        "}",
        "",
        "void block_080387FC(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080387FDu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080387FC\");",
    ]

    for op in thumb_large_boot_save_path_ops:
        kind = op[0]
        if kind == "thumb_large_boot_save_path":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08038801u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown large boot save path op {kind}")

    lines += [
        "}",
        "",
        "void block_080386B4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080386B5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080386B4\");",
    ]

    for op in thumb_refresh_init_wrapper_ops:
        kind = op[0]
        if kind == "thumb_refresh_init_wrapper":
            _, pc, target = op
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[14]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: push {{lr}}\");")
            lines.append("  state.regs[14] = 0x080386BBu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"080386B6: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown refresh init wrapper op {kind}")

    lines += [
        "}",
        "",
        "void block_080385CC(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080385CDu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080385CC\");",
    ]

    for op in thumb_system_init_ops:
        kind = op[0]
        if kind == "thumb_system_init_prefix":
            _, pc, global_address, target = op
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[14]);")
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append(f"  state.regs[0] = {hex32(global_address)}u;")
            lines.append("  state.regs[4] = 0u;")
            lines.append("  aw::write32(state.memory, state.regs[0], state.regs[4]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: clear system init global\");")
            lines.append("  state.regs[14] = 0x080385D9u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"080385D4: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown system init op {kind}")

    lines += [
        "}",
        "",
        "void block_0800F1B0(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0800F1B1u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0800F1B0\");",
    ]

    for op in thumb_engine_clear_ops:
        kind = op[0]
        if kind == "thumb_engine_clear":
            _, pc, target = op
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[14]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: push {{lr}}\");")
            lines.append("  state.regs[14] = 0x0800F1B7u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0800F1B2: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown engine clear op {kind}")

    lines += [
        "}",
        "",
        "void block_0800F170(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0800F171u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0800F170\");",
    ]

    for op in thumb_engine_register_reset_ops:
        kind = op[0]
        if kind == "thumb_engine_register_reset":
            _, pc, sound_control, dma_control, sound_bias, dma_source = op
            lines.append(f"  state.regs[2] = {hex32(sound_control)}u;")
            lines.append("  state.regs[3] = 0u;")
            lines.append("  aw::write16(state.memory, state.regs[2], static_cast<std::uint16_t>(state.regs[3]));")
            lines.append("  state.regs[0] = aw::read8(state.memory, state.regs[2]);")
            lines.append("  state.regs[0] |= 0x3Fu;")
            lines.append("  aw::write8(state.memory, state.regs[2], static_cast<std::uint8_t>(state.regs[0]));")
            lines.append(f"  state.regs[0] = {hex32(dma_control)}u;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[3]));")
            lines.append(f"  state.regs[0] = {hex32(sound_bias)}u;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[3]));")
            lines.append(f"  state.regs[0] = {hex32(dma_source)}u;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[3]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: reset sound/dma control registers\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"0800F19E: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown engine register reset op {kind}")

    lines += [
        "}",
        "",
        "void block_0800F1B6(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0800F1B7u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0800F1B6\");",
    ]

    for op in thumb_engine_clear_after_register_reset_ops:
        kind = op[0]
        if kind == "thumb_engine_clear_after_register_reset":
            _, pc, sound_control, dma_control, target = op
            lines.append(f"  state.regs[1] = {hex32(sound_control)}u;")
            lines.append("  state.regs[0] = aw::read8(state.memory, state.regs[1]);")
            lines.append("  state.regs[0] |= 0xC0u;")
            lines.append("  aw::write8(state.memory, state.regs[1], static_cast<std::uint8_t>(state.regs[0]));")
            lines.append(f"  state.regs[1] = {hex32(dma_control)}u;")
            lines.append("  state.regs[0] = 0x1Fu;")
            lines.append("  aw::write16(state.memory, state.regs[1], static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: enable engine control shadows\");")
            lines.append("  state.regs[14] = 0x0800F1CBu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0800F1C6: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown engine clear continuation op {kind}")

    lines += [
        "}",
        "",
        "void block_0800F1CA(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0800F1CBu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0800F1CA\");",
    ]

    for op in thumb_engine_clear_return_ops:
        kind = op[0]
        if kind == "thumb_engine_clear_return":
            _, pc = op
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: pop {{r0}}\");")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::trace(state, \"0800F1CC: bx r0\");")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown engine clear return op {kind}")

    lines += [
        "}",
        "",
        "void block_080385D8(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080385D9u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080385D8\");",
    ]

    for op in thumb_system_init_after_engine_clear_ops:
        kind = op[0]
        if kind == "thumb_system_init_after_engine_clear":
            _, pc, target = op
            lines.append("  state.regs[0] = 0u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: movs r0, #0\");")
            lines.append("  state.regs[14] = 0x080385DFu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"080385DA: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown system init after engine clear op {kind}")

    lines += [
        "}",
        "",
        "void block_08038260(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08038261u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08038260\");",
    ]

    for op in thumb_state_store_first_ops:
        kind = op[0]
        if kind == "thumb_state_store_first":
            _, pc, address = op
            lines.append(f"  state.regs[1] = {hex32(address)}u;")
            lines.append("  aw::write32(state.memory, state.regs[1], state.regs[0]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: store state word\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"08038264: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown first state store op {kind}")

    lines += [
        "}",
        "",
        "void block_080385DE(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080385DFu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080385DE\");",
    ]

    for op in thumb_system_init_after_first_state_store_ops:
        kind = op[0]
        if kind == "thumb_system_init_after_first_state_store":
            _, pc, target = op
            lines.append("  state.regs[0] = 0u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: movs r0, #0\");")
            lines.append("  state.regs[14] = 0x080385E5u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"080385E0: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown system init after first state store op {kind}")

    lines += [
        "}",
        "",
        "void block_0803826C(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0803826Du;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0803826C\");",
    ]

    for op in thumb_state_store_second_ops:
        kind = op[0]
        if kind == "thumb_state_store_second":
            _, pc, address = op
            lines.append(f"  state.regs[1] = {hex32(address)}u;")
            lines.append("  aw::write32(state.memory, state.regs[1], state.regs[0]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: store state word\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"08038270: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown second state store op {kind}")

    lines += [
        "}",
        "",
        "void block_080385E4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080385E5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080385E4\");",
    ]

    for op in thumb_system_init_clear_globals_ops:
        kind = op[0]
        if kind == "thumb_system_init_clear_globals":
            _, pc, byte_address, word_address_1, word_address_2, word_address_3, word_address_4, halfword_address, target = op
            lines.append(f"  state.regs[0] = {hex32(byte_address)}u;")
            lines.append("  aw::write8(state.memory, state.regs[0], static_cast<std::uint8_t>(state.regs[4]));")
            for address in [word_address_1, word_address_2, word_address_3, word_address_4]:
                lines.append(f"  state.regs[0] = {hex32(address)}u;")
                lines.append("  aw::write32(state.memory, state.regs[0], state.regs[4]);")
            lines.append(f"  state.regs[0] = {hex32(halfword_address)}u;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[4]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: clear system globals\");")
            lines.append("  state.regs[14] = 0x08038601u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"080385FC: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown system init clear globals op {kind}")

    lines += [
        "}",
        "",
        "void block_080385BC(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080385BDu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080385BC\");",
    ]

    for op in thumb_system_init_local_reset_ops:
        kind = op[0]
        if kind == "thumb_system_init_local_reset":
            _, pc, address = op
            lines.append(f"  state.regs[1] = {hex32(address)}u;")
            lines.append("  state.regs[0] = 0u;")
            lines.append("  aw::write16(state.memory, state.regs[1], static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: reset system halfword\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"080385C2: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown system init local reset op {kind}")

    lines += [
        "}",
        "",
        "void block_08038600(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08038601u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08038600\");",
    ]

    for op in thumb_system_init_dma_copy_call_ops:
        kind = op[0]
        if kind == "thumb_system_init_dma_copy_call":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08038605u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown system init DMA copy call op {kind}")

    lines += [
        "}",
        "",
        "void block_0801B820(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801B821u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801B820\");",
    ]

    for op in thumb_dma_setup_ops:
        kind = op[0]
        if kind == "thumb_dma_setup":
            _, pc, source, destination, end_address, target = op
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[14]);")
            lines.append(f"  state.regs[3] = {hex32(source)}u;")
            lines.append(f"  state.regs[1] = {hex32(destination)}u;")
            lines.append(f"  state.regs[0] = {hex32(end_address)}u;")
            lines.append("  state.regs[0] -= state.regs[1];")
            lines.append("  if (static_cast<std::int32_t>(state.regs[0]) < 0) {")
            lines.append("    state.regs[0] += 3u;")
            lines.append("  }")
            lines.append("  state.regs[2] = (state.regs[0] << 9u) >> 11u;")
            lines.append("  state.regs[0] = 0x06000000u;")
            lines.append("  state.regs[2] |= state.regs[0];")
            lines.append("  state.regs[0] = state.regs[3];")
            lines.append(f"  aw::trace(state, \"{pc:08X}: prepare CpuSet copy\");")
            lines.append("  state.regs[14] = 0x0801B841u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0801B83C: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown DMA setup op {kind}")

    lines += [
        "}",
        "",
        "void block_080796C4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080796C5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080796C4\");",
    ]

    for op in thumb_bios_cpuset_boot_ops:
        kind = op[0]
        if kind == "thumb_bios_cpuset_boot":
            _, pc, source, destination, control, words = op
            lines.append(f"  if (state.regs[0] != {hex32(source)}u || state.regs[1] != {hex32(destination)}u ||")
            lines.append(f"      state.regs[2] != {hex32(control)}u) {{")
            lines.append("    throw std::runtime_error(\"unsupported CpuSet boot arguments\");")
            lines.append("  }")
            lines.append("  static constexpr std::uint32_t kWords[] = {")
            for index in range(0, len(words), 6):
                chunk = ", ".join(f"{hex32(word)}u" for word in words[index:index + 6])
                lines.append(f"      {chunk},")
            lines.append("  };")
            lines.append("  for (std::uint32_t index = 0; index < 0x00000212u; ++index) {")
            lines.append(f"    aw::write32(state.memory, {hex32(destination)}u + index * 4u, kWords[index]);")
            lines.append("  }")
            lines.append(f"  aw::trace(state, \"{pc:08X}: svc #0x0B CpuSet boot copy\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"080796C6: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown CpuSet boot op {kind}")

    lines += [
        "}",
        "",
        "void block_0801B840(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801B841u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801B840\");",
    ]

    for op in thumb_dma_setup_return_ops:
        kind = op[0]
        if kind == "thumb_dma_setup_return":
            _, pc = op
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: pop {{r0}}\");")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::trace(state, \"0801B842: bx r0\");")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown DMA setup return op {kind}")

    lines += [
        "}",
        "",
        "void block_08038604(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08038605u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08038604\");",
    ]

    for op in thumb_system_init_next_call_ops:
        kind = op[0]
        if kind == "thumb_system_init_next_call":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08038609u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown system init next call op {kind}")

    lines += [
        "}",
        "",
        "void block_08034C98(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08034C99u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08034C98\");",
    ]

    for op in thumb_next_initializer_ops:
        kind = op[0]
        if kind == "thumb_next_initializer":
            _, pc, first_store, pointer_address, pointer_value, second_store, target = op
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[14]);")
            lines.append(f"  state.regs[2] = {hex32(first_store)}u;")
            lines.append(f"  state.regs[0] = {hex32(pointer_address)}u;")
            lines.append(f"  state.regs[1] = {hex32(pointer_value)}u;")
            lines.append("  aw::write32(state.memory, state.regs[2], state.regs[1]);")
            lines.append(f"  state.regs[0] = {hex32(second_store)}u;")
            lines.append("  aw::write32(state.memory, state.regs[0], state.regs[1]);")
            lines.append("  state.regs[0] = 0u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: snapshot system pointers\");")
            lines.append("  state.regs[14] = 0x08034CADu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08034CA8: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown next initializer op {kind}")

    lines += [
        "}",
        "",
        "void block_08018AAC(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08018AADu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08018AAC\");",
    ]

    for op in thumb_byte_reset_helper_ops:
        kind = op[0]
        if kind == "thumb_byte_reset_helper":
            _, pc, sentinel_address, value_address = op
            lines.append(f"  state.regs[2] = {hex32(sentinel_address)}u;")
            lines.append("  state.regs[1] = 0x000000FFu;")
            lines.append("  aw::write8(state.memory, state.regs[2], static_cast<std::uint8_t>(state.regs[1]));")
            lines.append(f"  state.regs[1] = {hex32(value_address)}u;")
            lines.append("  aw::write8(state.memory, state.regs[1], static_cast<std::uint8_t>(state.regs[0]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: reset state bytes\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"08018AB6: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown byte reset helper op {kind}")

    lines += [
        "}",
        "",
        "void block_08034CAC(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08034CADu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08034CAC\");",
    ]

    for op in thumb_next_initializer_after_reset_ops:
        kind = op[0]
        if kind == "thumb_next_initializer_after_reset":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08034CB1u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown next initializer continuation op {kind}")

    lines += [
        "}",
        "",
        "void block_0801F114(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801F115u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801F114\");",
    ]

    for op in thumb_table_copy_helper_ops:
        kind = op[0]
        if kind == "thumb_table_copy_helper":
            _, pc, source, destination_pointer_address, destination, count, bytes_to_copy = op
            lines.append("  state.regs[13] -= 12u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[5]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 8u, state.regs[14]);")
            lines.append(f"  state.regs[5] = {hex32(source)}u;")
            lines.append(f"  state.regs[0] = {hex32(destination_pointer_address)}u;")
            lines.append(f"  state.regs[3] = {hex32(destination)}u;")
            lines.append("  state.regs[2] = 0u;")
            lines.append(f"  state.regs[4] = {hex32(count)}u;")
            lines.append("  static constexpr std::uint8_t kBytes[] = {")
            for index in range(0, len(bytes_to_copy), 16):
                chunk = ", ".join(f"0x{byte:02X}u" for byte in bytes_to_copy[index:index + 16])
                lines.append(f"      {chunk},")
            lines.append("  };")
            lines.append(f"  for (std::uint32_t index = 0; index <= {hex32(count)}u; ++index) {{")
            lines.append(f"    aw::write8(state.memory, {hex32(destination)}u + index, kBytes[index]);")
            lines.append("  }")
            lines.append(f"  aw::trace(state, \"{pc:08X}: copy boot table bytes\");")
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[5] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[13] += 8u;")
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::trace(state, \"0801F12E: pop {r4, r5}; pop {r0}; bx r0\");")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown table copy helper op {kind}")

    lines += [
        "}",
        "",
        "void block_08034CB0(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08034CB1u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08034CB0\");",
    ]

    for op in thumb_next_initializer_return_ops:
        kind = op[0]
        if kind == "thumb_next_initializer_return":
            _, pc = op
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: pop {{r0}}\");")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::trace(state, \"08034CB2: bx r0\");")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown next initializer return op {kind}")

    lines += [
        "}",
        "",
        "void block_08038608(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08038609u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08038608\");",
    ]

    for op in thumb_system_init_mode_call_ops:
        kind = op[0]
        if kind == "thumb_system_init_mode_call":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x0803860Du;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown system init mode call op {kind}")

    lines += [
        "}",
        "",
        "void block_08034C74(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08034C75u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08034C74\");",
    ]

    for op in thumb_mode_initializer_ops:
        kind = op[0]
        if kind == "thumb_mode_initializer":
            _, pc, base_address, target = op
            lines.append("  state.regs[13] -= 12u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[5]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 8u, state.regs[14]);")
            lines.append(f"  state.regs[4] = {hex32(base_address)}u;")
            lines.append("  state.regs[5] = 1u;")
            lines.append("  aw::write8(state.memory, state.regs[4] + 0x0Cu, static_cast<std::uint8_t>(state.regs[5]));")
            lines.append("  state.regs[0] = 3u;")
            lines.append("  aw::write8(state.memory, state.regs[4] + 0x01u, static_cast<std::uint8_t>(state.regs[0]));")
            lines.append("  aw::write8(state.memory, state.regs[4] + 0x02u, static_cast<std::uint8_t>(state.regs[5]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: initialize mode state prefix\");")
            lines.append("  state.regs[14] = 0x08034C87u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08034C82: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown mode initializer op {kind}")

    lines += [
        "}",
        "",
        "void block_08034BA8(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08034BA9u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08034BA8\");",
    ]

    for op in thumb_defaults_initializer_ops:
        kind = op[0]
        if kind == "thumb_defaults_initializer":
            _, pc, first_global, second_global, base_address, target = op
            lines.append("  state.regs[13] -= 20u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[5]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 8u, state.regs[6]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 12u, state.regs[7]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 16u, state.regs[14]);")
            lines.append("  state.regs[4] = 0u;")
            lines.append(f"  aw::write32(state.memory, {hex32(first_global)}u, state.regs[4]);")
            lines.append(f"  aw::write32(state.memory, {hex32(second_global)}u, state.regs[4]);")
            lines.append(f"  state.regs[6] = {hex32(base_address)}u;")
            lines.append("  state.regs[7] = 1u;")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x39u, 1u);")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x3Au, 1u);")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x3Bu, 1u);")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x3Cu, 1u);")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x34u, 1u);")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x35u, 2u);")
            lines.append("  state.regs[5] = 3u;")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x36u, 3u);")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x37u, 4u);")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x3Eu, 1u);")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x3Fu, 2u);")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x40u, 4u);")
            lines.append("  aw::write8(state.memory, state.regs[6] + 0x41u, 3u);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: initialize defaults prefix\");")
            lines.append("  state.regs[14] = 0x08034BFBu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08034BF6: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown defaults initializer op {kind}")

    lines += [
        "}",
        "",
        "void block_08024EC4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08024EC5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08024EC4\");",
    ]

    for op in thumb_order_helper_ops:
        kind = op[0]
        if kind == "thumb_order_helper":
            _, pc, base_address = op
            lines.append(f"  state.regs[2] = {hex32(base_address)}u;")
            lines.append("  aw::write8(state.memory, state.regs[2] + 0x43u, 0u);")
            lines.append("  aw::write8(state.memory, state.regs[2] + 0x44u, 1u);")
            lines.append("  aw::write8(state.memory, state.regs[2] + 0x45u, 2u);")
            lines.append("  aw::write8(state.memory, state.regs[2] + 0x46u, 3u);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: initialize order bytes\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"08024EE0: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown order helper op {kind}")

    lines += [
        "}",
        "",
        "void block_08034BFA(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08034BFBu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08034BFA\");",
    ]

    for op in thumb_defaults_after_order_ops:
        kind = op[0]
        if kind == "thumb_defaults_after_order":
            _, pc, unit_source_address, fallback_base = op
            lines.append("  const std::uint32_t base = state.regs[6];")
            lines.append("  aw::write8(state.memory, base + 0x30u, 0u);")
            lines.append("  aw::write8(state.memory, base + 0x31u, 0u);")
            lines.append("  aw::write32(state.memory, base + 0x28u, 0x000003E8u);")
            lines.append("  aw::write32(state.memory, base + 0x24u, 0u);")
            lines.append("  aw::write32(state.memory, base + 0x14u, 0u);")
            lines.append("  aw::write32(state.memory, base + 0x18u, 0u);")
            lines.append("  aw::write32(state.memory, base + 0x1Cu, 0u);")
            lines.append("  aw::write32(state.memory, base + 0x20u, 0u);")
            lines.append("  aw::write8(state.memory, base + 0x0Du, 0u);")
            lines.append("  aw::write8(state.memory, base + 0x2Fu, 0u);")
            lines.append("  aw::write8(state.memory, base + 0x2Cu, 0u);")
            lines.append("  aw::write8(state.memory, base + 0x2Du, 0u);")
            lines.append("  aw::write8(state.memory, base + 0x2Eu, 0u);")
            lines.append("  aw::write8(state.memory, base + 0x04u, 3u);")
            lines.append("  aw::write8(state.memory, base + 0x05u, 1u);")
            lines.append("  aw::write8(state.memory, base + 0x06u, 0u);")
            lines.append("  aw::write8(state.memory, base + 0x08u, 1u);")
            lines.append(f"  state.regs[0] = aw::read16(state.memory, {hex32(unit_source_address)}u + 0x1Au);")
            lines.append("  aw::write8(state.memory, base + 0x09u, static_cast<std::uint8_t>(state.regs[0]));")
            lines.append("  if (aw::read8(state.memory, base + 0x01u) >= 2u && aw::read8(state.memory, base + 0x01u) <= 3u) {")
            lines.append("    aw::write8(state.memory, base + 0x07u, 1u);")
            lines.append("  } else {")
            lines.append(f"    aw::write8(state.memory, {hex32(fallback_base)}u + 0x07u, 0u);")
            lines.append("  }")
            lines.append(f"  aw::trace(state, \"{pc:08X}: finish defaults initialization\");")
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[5] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[6] = aw::read32(state.memory, state.regs[13] + 8u);")
            lines.append("  state.regs[7] = aw::read32(state.memory, state.regs[13] + 12u);")
            lines.append("  state.regs[13] += 16u;")
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::trace(state, \"08034C5A: pop {r4, r5, r6, r7}; pop {r0}; bx r0\");")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown defaults continuation op {kind}")

    lines += [
        "}",
        "",
        "void block_08034C86(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08034C87u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08034C86\");",
    ]

    for op in thumb_mode_initializer_after_defaults_ops:
        kind = op[0]
        if kind == "thumb_mode_initializer_after_defaults":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08034C8Bu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown mode initializer continuation op {kind}")

    lines += [
        "}",
        "",
        "void block_08034C64(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08034C65u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08034C64\");",
    ]

    for op in thumb_mode_clear_helper_ops:
        kind = op[0]
        if kind == "thumb_mode_clear_helper":
            _, pc, base_address = op
            lines.append(f"  state.regs[0] = {hex32(base_address)}u + 0x32u;")
            lines.append("  state.regs[1] = 0u;")
            lines.append("  aw::write8(state.memory, state.regs[0], static_cast<std::uint8_t>(state.regs[1]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: clear mode byte\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"08034C6C: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown mode clear helper op {kind}")

    lines += [
        "}",
        "",
        "void block_08034C8A(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08034C8Bu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08034C8A\");",
    ]

    for op in thumb_mode_initializer_return_ops:
        kind = op[0]
        if kind == "thumb_mode_initializer_return":
            _, pc = op
            lines.append("  aw::write8(state.memory, state.regs[4] + 0x09u, static_cast<std::uint8_t>(state.regs[5]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: finish mode initializer\");")
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[5] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[13] += 8u;")
            lines.append("  state.regs[0] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append("  state.regs[15] = state.regs[0];")
            lines.append("  aw::trace(state, \"08034C8C: pop {r4, r5}; pop {r0}; bx r0\");")
            lines.append("  aw::stop_at(state, state.regs[0]);")
        else:
            raise ValueError(f"internal generator error: unknown mode initializer return op {kind}")

    lines += [
        "}",
        "",
        "void block_0803860C(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0803860Du;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0803860C\");",
    ]

    for op in thumb_system_init_unit_call_ops:
        kind = op[0]
        if kind == "thumb_system_init_unit_call":
            _, pc, target = op
            lines.append("  state.regs[14] = 0x08038611u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown system init unit call op {kind}")

    lines += [
        "}",
        "",
        "void block_080149E0(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080149E1u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080149E0\");",
    ]

    for op in thumb_unit_initializer_ops:
        kind = op[0]
        if kind == "thumb_unit_initializer":
            _, pc, global_address, target = op
            lines.append("  state.regs[13] -= 16u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[5]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 8u, state.regs[6]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 12u, state.regs[14]);")
            lines.append(f"  state.regs[0] = {hex32(global_address)}u;")
            lines.append("  state.regs[4] = 0u;")
            lines.append("  aw::write32(state.memory, state.regs[0], state.regs[4]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: clear unit initializer global\");")
            lines.append("  state.regs[14] = 0x080149EDu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"080149E8: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown unit initializer op {kind}")

    lines += [
        "}",
        "",
        "void block_0801A69C(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0801A69Du;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0801A69C\");",
    ]

    for op in thumb_global_table_reset_ops:
        kind = op[0]
        if kind == "thumb_global_table_reset":
            _, pc, table_base, header_base = op
            lines.append("  state.regs[13] -= 12u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[5]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 8u, state.regs[14]);")
            lines.append("  state.regs[2] = 0u;")
            lines.append(f"  state.regs[5] = {hex32(table_base)}u;")
            lines.append(f"  state.regs[4] = {hex32(header_base)}u;")
            lines.append("  for (std::uint32_t index = 0; index <= 0x80u; ++index) {")
            lines.append("    aw::write32(state.memory, state.regs[5] + index * 12u, state.regs[2]);")
            lines.append("  }")
            lines.append("  aw::write32(state.memory, state.regs[4] + 4u, 0u);")
            lines.append("  aw::write16(state.memory, state.regs[4], 0u);")
            lines.append("  aw::write32(state.memory, state.regs[5] + 4u, 0u);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: reset global table\");")
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[5] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[13] += 8u;")
            lines.append("  state.regs[1] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append("  state.regs[15] = state.regs[1];")
            lines.append("  aw::trace(state, \"0801A6CA: pop {r4, r5}; pop {r1}; bx r1\");")
            lines.append("  aw::stop_at(state, state.regs[1]);")
        else:
            raise ValueError(f"internal generator error: unknown global table reset op {kind}")

    lines += [
        "}",
        "",
        "void block_080149EC(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x080149EDu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x080149EC\");",
    ]

    for op in thumb_unit_initializer_after_reset_ops:
        kind = op[0]
        if kind == "thumb_unit_initializer_after_reset":
            _, pc, count_address, selected_address, active_base, target = op
            lines.append(f"  state.regs[1] = {hex32(count_address)}u;")
            lines.append("  state.regs[0] = 0x10u;")
            lines.append("  aw::write16(state.memory, state.regs[1], static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  state.regs[0] = {hex32(selected_address)}u;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[4]));")
            lines.append("  state.regs[5] = 0x100u;")
            lines.append(f"  state.regs[6] = {hex32(active_base)}u;")
            lines.append("  state.regs[0] = state.regs[4] + state.regs[6];")
            lines.append("  state.regs[1] = 0u;")
            lines.append("  aw::write8(state.memory, state.regs[0], static_cast<std::uint8_t>(state.regs[1]));")
            lines.append("  state.regs[0] = state.regs[4];")
            lines.append("  state.regs[1] = state.regs[5];")
            lines.append("  state.regs[2] = state.regs[5];")
            lines.append("  state.regs[3] = 0u;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: prepare first unit entry\");")
            lines.append("  state.regs[14] = 0x08014A11u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08014A0C: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown unit initializer continuation op {kind}")

    lines += [
        "}",
        "",
        "void block_08014C98(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08014C99u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08014C98\");",
    ]

    for op in thumb_unit_entry_setup_ops:
        kind = op[0]
        if kind == "thumb_unit_entry_setup":
            _, pc, table_base, target = op
            lines.append("  state.regs[13] -= 12u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[5]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 8u, state.regs[14]);")
            lines.append(f"  state.regs[5] = {hex32(table_base)}u;")
            lines.append("  state.regs[4] = (state.regs[0] << 4u) + state.regs[5];")
            lines.append("  aw::write16(state.memory, state.regs[4], static_cast<std::uint16_t>(state.regs[1]));")
            lines.append("  aw::write16(state.memory, state.regs[4] + 2u, static_cast<std::uint16_t>(state.regs[2]));")
            lines.append("  aw::write16(state.memory, state.regs[4] + 4u, static_cast<std::uint16_t>(state.regs[3]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: initialize unit entry\");")
            lines.append("  state.regs[14] = 0x08014CABu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08014CA6: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown unit entry setup op {kind}")

    lines += [
        "}",
        "",
        "void block_08014BF8(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08014BF9u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08014BF8\");",
    ]

    for op in thumb_unit_geometry_helper_ops:
        kind = op[0]
        if kind == "thumb_unit_geometry_helper":
            _, pc, table_base, target = op
            lines.append("  state.regs[13] -= 16u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[5]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 8u, state.regs[6]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 12u, state.regs[14]);")
            lines.append("  state.regs[6] = state.regs[9];")
            lines.append("  state.regs[5] = state.regs[8];")
            lines.append("  state.regs[13] -= 8u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[5]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[6]);")
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  state.regs[9] = state.regs[0];")
            lines.append(f"  state.regs[0] = {hex32(table_base)}u;")
            lines.append("  state.regs[1] = state.regs[9];")
            lines.append("  state.regs[4] = (state.regs[1] << 4u) + state.regs[0];")
            lines.append("  state.regs[2] = 4u;")
            lines.append("  state.regs[0] = static_cast<std::uint32_t>(static_cast<std::int16_t>(aw::read16(state.memory, state.regs[4] + state.regs[2])));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: prepare unit geometry math\");")
            lines.append("  state.regs[14] = 0x08014C15u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08014C10: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown unit geometry helper op {kind}")

    lines += [
        "}",
        "",
        "void block_0807AF30(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AF31u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AF30\");",
    ]

    for op in thumb_first_math_wrapper_ops:
        kind = op[0]
        if kind == "thumb_first_math_wrapper":
            _, pc, target = op
            lines.append("  state.regs[13] -= 4u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[14]);")
            lines.append("  state.regs[0] = (state.regs[0] + 0x5Au) & 0xFFFFFFFFu;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: add angle and call trig helper\");")
            lines.append("  state.regs[14] = 0x0807AF39u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0807AF34: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown first math wrapper op {kind}")

    lines += [
        "}",
        "",
        "void block_0807AED4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AED5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AED4\");",
    ]

    for op in thumb_trig_lookup_ops:
        kind = op[0]
        if kind == "thumb_trig_lookup":
            _, pc, table = op
            table_initializer = ", ".join(f"0x{value:04X}u" for value in table)
            lines.append(f"  static constexpr std::uint16_t kTrigTable[181] = {{{table_initializer}}};")
            lines.append("  std::int32_t angle = static_cast<std::int32_t>(state.regs[0]);")
            lines.append("  while (angle < 0) {")
            lines.append("    angle += 360;")
            lines.append("  }")
            lines.append("  while (angle > 0x167) {")
            lines.append("    angle -= 360;")
            lines.append("  }")
            lines.append("  const std::int32_t quadrant_source = angle;")
            lines.append("  if (angle > 0xB3) {")
            lines.append("    angle -= 0xB4;")
            lines.append("  }")
            lines.append("  if (angle > 0x5A) {")
            lines.append("    angle = 0xB4 - angle;")
            lines.append("  }")
            lines.append("  std::int32_t value = static_cast<std::int16_t>(kTrigTable[static_cast<std::uint32_t>(angle)]);")
            lines.append("  if (quadrant_source > 0xB3) {")
            lines.append("    value = -value;")
            lines.append("  }")
            lines.append("  state.regs[0] = static_cast<std::uint32_t>(static_cast<std::int16_t>(value));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: trig table lookup\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"0807AF2C: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown trig lookup op {kind}")

    lines += [
        "}",
        "",
        "void block_0807AF38(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AF39u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AF38\");",
    ]

    for op in thumb_first_math_return_ops:
        kind = op[0]
        if kind == "thumb_first_math_return":
            _, pc = op
            lines.append("  state.regs[0] = static_cast<std::uint32_t>(static_cast<std::int16_t>(state.regs[0] & 0xFFFFu));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: sign-extend math result\");")
            lines.append("  state.regs[15] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[13] += 4u;")
            lines.append("  aw::trace(state, \"0807AF3C: pop {pc}\");")
            lines.append("  aw::stop_at(state, state.regs[15]);")
        else:
            raise ValueError(f"internal generator error: unknown first math return op {kind}")

    lines += [
        "}",
        "",
        "void block_08014C14(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08014C15u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08014C14\");",
    ]

    for op in thumb_unit_geometry_after_first_math_ops:
        kind = op[0]
        if kind == "thumb_unit_geometry_after_first_math":
            _, pc, target = op
            lines.append("  state.regs[0] = static_cast<std::uint32_t>(static_cast<std::int32_t>(state.regs[0] << 16u) >> 14);")
            lines.append("  state.regs[2] = 0u;")
            lines.append("  state.regs[1] = static_cast<std::uint32_t>(static_cast<std::int16_t>(aw::read16(state.memory, state.regs[4] + state.regs[2])));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: scale first trig component\");")
            lines.append("  state.regs[14] = 0x08014C21u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08014C1C: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown unit geometry after first math op {kind}")

    lines += [
        "}",
        "",
        "void block_0807B488(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807B489u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807B488\");",
    ]

    for op in thumb_signed_divide_helper_ops:
        kind = op[0]
        if kind == "thumb_signed_divide_helper":
            _, pc = op
            lines.append("  const std::int32_t numerator = static_cast<std::int32_t>(state.regs[0]);")
            lines.append("  const std::int32_t denominator = static_cast<std::int32_t>(state.regs[1]);")
            lines.append("  if (denominator == 0) {")
            lines.append("    state.regs[0] = 0u;")
            lines.append("  } else {")
            lines.append("    const auto quotient = static_cast<std::int64_t>(numerator) / static_cast<std::int64_t>(denominator);")
            lines.append("    state.regs[0] = static_cast<std::uint32_t>(static_cast<std::int32_t>(quotient));")
            lines.append("  }")
            lines.append(f"  aw::trace(state, \"{pc:08X}: signed divide helper\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"0807B50E: mov pc, lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown signed divide helper op {kind}")

    lines += [
        "}",
        "",
        "void block_08014C20(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x08014C21u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x08014C20\");",
    ]

    for op in thumb_unit_geometry_after_first_divide_ops:
        kind = op[0]
        if kind == "thumb_unit_geometry_after_first_divide":
            _, pc, target = op
            lines.append("  state.regs[8] = state.regs[0];")
            lines.append("  state.regs[0] = static_cast<std::uint32_t>(static_cast<std::int16_t>(state.regs[0] & 0xFFFFu));")
            lines.append("  state.regs[8] = state.regs[0];")
            lines.append("  state.regs[1] = 4u;")
            lines.append("  state.regs[0] = static_cast<std::uint32_t>(static_cast<std::int16_t>(aw::read16(state.memory, state.regs[4] + state.regs[1])));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: preserve first component and prepare second trig\");")
            lines.append("  state.regs[14] = 0x08014C31u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"08014C2C: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown unit geometry after first divide op {kind}")

    lines += [
        "}",
        "",
        "void block_0807ACE8(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807ACE9u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807ACE8\");",
    ]

    emit_thumb_ops(lines, thumb_init_copy_ops)

    lines += [
        "}",
        "",
        "void block_0807ACF4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807ACF5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807ACF4\");",
    ]

    emit_thumb_ops(lines, thumb_copy_return_ops)

    lines += [
        "}",
        "",
        "void block_0807AD00(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AD01u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AD00\");",
    ]

    emit_thumb_ops(lines, thumb_helper_ops)

    lines += [
        "}",
        "",
        "void block_0807AD0A(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AD0Bu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AD0A\");",
    ]

    emit_thumb_ops(lines, thumb_continuation_ops)

    lines += [
        "}",
        "",
        "void block_0807AD0E(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AD0Fu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AD0E\");",
    ]

    emit_thumb_ops(lines, thumb_outer_return_ops)

    lines += [
        "}",
        "",
        "void block_0807AD10(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AD11u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AD10\");",
    ]

    emit_thumb_ops(lines, thumb_entry_ops)

    lines += [
        "}",
        "",
        "void block_0807AD16(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AD17u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AD16\");",
    ]

    emit_thumb_ops(lines, thumb_entry_second_call_ops)

    lines += [
        "}",
        "",
        "void block_0807AE60(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AE61u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AE60\");",
    ]

    for op in thumb_hardware_ops:
        kind = op[0]
        if kind == "thumb_hw_init_path":
            _, pc, shadow_address, ie_address, ime_address = op
            lines.append("  if (state.regs[0] != 0u || state.regs[1] != 0u) {")
            lines.append("    throw std::runtime_error(\"block_0807AE60 only supports initial r0=0/r1=0 path\");")
            lines.append("  }")
            lines.append("  state.regs[2] = state.regs[1];")
            lines.append(f"  aw::trace(state, \"{pc:08X}: adds r2, r1, #0\");")
            lines.append(f"  state.regs[0] = {hex32(shadow_address)}u;")
            lines.append("  aw::write32(state.memory, state.regs[0], state.regs[2]);")
            lines.append(
                f"  aw::trace(state, \"0807AE84: ldr/str interrupt shadow {hex32(shadow_address)}\");"
            )
            lines.append("  state.regs[1] = state.regs[0];")
            lines.append(f"  state.regs[0] = {hex32(ie_address)}u;")
            lines.append("  state.regs[1] = aw::read32(state.memory, state.regs[1]);")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[1]));")
            lines.append(f"  aw::trace(state, \"0807AEA4: strh IE {hex32(ie_address)}\");")
            lines.append("  state.regs[2] = 0x00010000u & state.regs[1];")
            lines.append(f"  state.regs[0] = {hex32(ime_address)}u;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[2]));")
            lines.append(f"  aw::trace(state, \"0807AEC8: strh IME {hex32(ime_address)}\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"0807AECC: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown hardware op {kind}")

    lines += [
        "}",
        "",
        "void block_0807AE14(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AE15u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AE14\");",
    ]

    for op in thumb_display_setup_ops:
        kind = op[0]
        if kind == "thumb_display_setup_prefix":
            _, pc, target = op
            lines.append("  state.regs[13] -= 8u;")
            lines.append("  aw::write32(state.memory, state.regs[13], state.regs[4]);")
            lines.append("  aw::write32(state.memory, state.regs[13] + 4u, state.regs[14]);")
            lines.append("  state.regs[0] = 0;")
            lines.append("  state.regs[1] = 0;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: push {{r4, lr}}; movs r0/r1, #0\");")
            lines.append("  state.regs[14] = 0x0807AE1Fu;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0807AE1A: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown display setup op {kind}")

    lines += [
        "}",
        "",
        "void block_0807AE1E(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AE1Fu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AE1E\");",
    ]

    for op in thumb_display_setup_continuation_ops:
        kind = op[0]
        if kind == "thumb_display_setup_continuation":
            _, pc, source, destination, target = op
            lines.append(f"  state.regs[0] = {hex32(source)}u;")
            lines.append(f"  state.regs[1] = {hex32(destination)}u;")
            lines.append("  state.regs[2] = 0x0000001Eu;")
            lines.append(f"  aw::trace(state, \"{pc:08X}: ldr r0/r1; movs r2, #0x1E\");")
            lines.append("  state.regs[14] = 0x0807AE29u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0807AE24: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown display continuation op {kind}")

    lines += [
        "}",
        "",
        "void block_0807AE28(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AE29u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AE28\");",
    ]

    for op in thumb_display_second_dma_ops:
        kind = op[0]
        if kind == "thumb_display_second_dma":
            _, pc, source, scratch_pointer, target = op
            lines.append(f"  state.regs[0] = {hex32(source)}u;")
            lines.append(f"  state.regs[4] = {hex32(scratch_pointer)}u;")
            lines.append("  state.regs[2] = 0x00000100u;")
            lines.append("  state.regs[1] = state.regs[4];")
            lines.append(f"  aw::trace(state, \"{pc:08X}: ldr r0/r4; movs/lsls r2, #0x100; adds r1, r4, #0\");")
            lines.append("  state.regs[14] = 0x0807AE37u;")
            lines.append(f"  state.regs[15] = {hex32(target)}u;")
            lines.append(f"  aw::trace(state, \"0807AE32: bl {hex32(target)}\");")
            lines.append(f"  aw::stop_at(state, {hex32(target)}u);")
        else:
            raise ValueError(f"internal generator error: unknown display second DMA op {kind}")

    lines += [
        "}",
        "",
        "void block_0807AE36(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AE37u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AE36\");",
    ]

    for op in thumb_display_setup_tail_ops:
        kind = op[0]
        if kind == "thumb_display_setup_tail":
            _, pc, pointer_address = op
            lines.append(f"  state.regs[0] = {hex32(pointer_address)}u;")
            lines.append("  aw::write32(state.memory, state.regs[0], state.regs[4]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: ldr r0; str r4, [r0]\");")
            lines.append("  state.regs[4] = aw::read32(state.memory, state.regs[13]);")
            lines.append("  state.regs[15] = aw::read32(state.memory, state.regs[13] + 4u);")
            lines.append("  state.regs[13] += 8u;")
            lines.append("  aw::trace(state, \"0807AE3A: pop {r4, pc}\");")
            lines.append("  aw::stop_at(state, state.regs[15]);")
        else:
            raise ValueError(f"internal generator error: unknown display setup tail op {kind}")

    lines += [
        "}",
        "",
        "void block_0807AE50(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AE51u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AE50\");",
    ]

    for op in thumb_callback_register_ops:
        kind = op[0]
        if kind == "thumb_callback_register":
            _, pc, callback_table = op
            lines.append(f"  state.regs[2] = {hex32(callback_table)}u;")
            lines.append("  state.regs[0] = (state.regs[0] << 2u) + state.regs[2];")
            lines.append("  aw::write32(state.memory, state.regs[0], state.regs[1]);")
            lines.append(f"  aw::trace(state, \"{pc:08X}: register callback\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"0807AE58: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown callback register op {kind}")

    lines += [
        "}",
        "",
        "void block_0807AFF4(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807AFF5u;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807AFF4\");",
    ]

    for op in thumb_struct_init_ops:
        kind = op[0]
        if kind == "thumb_struct_init":
            lines.append("  state.regs[3] = state.regs[2];")
            lines.append("  state.regs[2] = 0;")
            lines.append("  aw::write8(state.memory, state.regs[0], static_cast<std::uint8_t>(state.regs[2]));")
            lines.append("  aw::write32(state.memory, state.regs[0] + 4u, state.regs[2]);")
            lines.append("  aw::write8(state.memory, state.regs[0] + 1u, static_cast<std::uint8_t>(state.regs[3]));")
            lines.append("  aw::write8(state.memory, state.regs[0] + 2u, static_cast<std::uint8_t>(state.regs[2]));")
            lines.append("  aw::write32(state.memory, state.regs[0] + 12u, state.regs[1]);")
            lines.append("  aw::write32(state.memory, state.regs[0] + 8u, state.regs[1]);")
            lines.append("  aw::trace(state, \"0807AFF4: initialize list header\");")
            lines.append("  if (static_cast<std::int32_t>(state.regs[3]) > 0) {")
            lines.append("    state.regs[0] = 0;")
            lines.append("    state.regs[2] = state.regs[3];")
            lines.append("    while (state.regs[2] != 0) {")
            lines.append("      aw::write32(state.memory, state.regs[1], state.regs[0]);")
            lines.append("      state.regs[1] += 12u;")
            lines.append("      state.regs[2] -= 1u;")
            lines.append("    }")
            lines.append("  }")
            lines.append("  aw::trace(state, \"0807B016: bx lr\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown struct init op {kind}")

    lines += [
        "}",
        "",
        "void block_0807B2DC(aw::CpuState& state) {",
        "  state.thumb = true;",
        "  state.stop_target = 0;",
        "  state.regs[15] = 0x0807B2DDu;",
        "  aw::trace(state, \"Executing generated Thumb block 0x0807B2DC\");",
    ]

    for op in thumb_dma_helper_ops:
        kind = op[0]
        if kind == "thumb_dma_helper":
            _, pc, source_register, destination_register, control_register = op
            lines.append("  state.regs[2] = (state.regs[2] << 16u) >> 16u;")
            lines.append(f"  state.regs[3] = {hex32(source_register)}u;")
            lines.append("  aw::write16(state.memory, state.regs[3], static_cast<std::uint16_t>(state.regs[0]));")
            lines.append("  state.regs[3] += 2u;")
            lines.append("  state.regs[0] = static_cast<std::uint32_t>(static_cast<std::int32_t>(state.regs[0]) >> 16);")
            lines.append("  aw::write16(state.memory, state.regs[3], static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  state.regs[0] = {hex32(destination_register)}u;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[1]));")
            lines.append("  state.regs[0] += 2u;")
            lines.append("  state.regs[1] = static_cast<std::uint32_t>(static_cast<std::int32_t>(state.regs[1]) >> 16);")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[1]));")
            lines.append("  state.regs[0] += 2u;")
            lines.append("  aw::write16(state.memory, state.regs[0], static_cast<std::uint16_t>(state.regs[2]));")
            lines.append(f"  state.regs[1] = {hex32(control_register)}u;")
            lines.append("  state.regs[2] = 0x00008000u;")
            lines.append("  state.regs[0] = state.regs[2];")
            lines.append("  aw::write16(state.memory, state.regs[1], static_cast<std::uint16_t>(state.regs[0]));")
            lines.append(f"  aw::trace(state, \"{pc:08X}: write DMA halfword registers\");")
            lines.append("  state.regs[15] = state.regs[14];")
            lines.append("  aw::trace(state, \"0807B302: bx lr\");")
            lines.append("  aw::stop_at(state, state.regs[14]);")
        else:
            raise ValueError(f"internal generator error: unknown DMA helper op {kind}")

    lines += [
        "}",
        "",
        "void dispatch_one(aw::CpuState& state) {",
        "  if (state.stop_target == 0x0800F171u) {",
        "    block_0800F170(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0800F1B1u) {",
        "    block_0800F1B0(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0800F1B7u) {",
        "    block_0800F1B6(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0800F1CBu) {",
        "    block_0800F1CA(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08038261u) {",
        "    block_08038260(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0803826Du) {",
        "    block_0803826C(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801B821u) {",
        "    block_0801B820(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08010445u) {",
        "    block_08010444(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080104B1u) {",
        "    block_080104B0(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080104D5u) {",
        "    block_080104D4(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08010545u) {",
        "    block_08010544(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801055Bu) {",
        "    block_0801055A(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801055Fu) {",
        "    block_0801055E(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08010563u) {",
        "    block_08010562(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08010575u) {",
        "    block_08010574(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080106A5u) {",
        "    block_080106A4(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08010969u) {",
        "    block_08010968(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801096Fu) {",
        "    block_0801096E(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08010975u) {",
        "    block_08010974(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801097Bu) {",
        "    block_0801097A(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801097Fu) {",
        "    block_0801097E(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08010A79u) {",
        "    block_08010A78(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08012CF1u) {",
        "    block_08012CF0(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801A769u) {",
        "    block_0801A768(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801A785u) {",
        "    block_0801A784(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801A78Bu) {",
        "    block_0801A78A(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801AFC9u) {",
        "    block_0801AFC8(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08018AADu) {",
        "    block_08018AAC(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080149E1u) {",
        "    block_080149E0(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080149EDu) {",
        "    block_080149EC(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08014C99u) {",
        "    block_08014C98(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08014BF9u) {",
        "    block_08014BF8(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AF31u) {",
        "    block_0807AF30(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AED5u) {",
        "    block_0807AED4(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AF39u) {",
        "    block_0807AF38(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08014C15u) {",
        "    block_08014C14(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807B489u) {",
        "    block_0807B488(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08014C21u) {",
        "    block_08014C20(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801A69Du) {",
        "    block_0801A69C(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801F115u) {",
        "    block_0801F114(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08024EC5u) {",
        "    block_08024EC4(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801B2B9u) {",
        "    block_0801B2B8(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801AC1Du) {",
        "    block_0801AC1C(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801AD39u) {",
        "    block_0801AD38(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08015D1Du) {",
        "    block_08015D1C(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08015FB5u) {",
        "    block_08015FB4(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08015FC1u) {",
        "    block_08015FC0(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08015FCBu) {",
        "    block_08015FCA(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0803F87Du) {",
        "    block_0803F87C(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0803F887u) {",
        "    block_0803F886(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0803F891u) {",
        "    block_0803F890(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0803F899u) {",
        "    block_0803F898(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0803F8B1u) {",
        "    block_0803F8B0(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AD11u) {",
        "    block_0807AD10(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AD17u) {",
        "    block_0807AD16(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080386E5u) {",
        "    block_080386E4(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080385CDu) {",
        "    block_080385CC(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08034C99u) {",
        "    block_08034C98(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08034C75u) {",
        "    block_08034C74(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08034BA9u) {",
        "    block_08034BA8(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08034BFBu) {",
        "    block_08034BFA(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08034C87u) {",
        "    block_08034C86(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08034C65u) {",
        "    block_08034C64(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08034C8Bu) {",
        "    block_08034C8A(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08034CADu) {",
        "    block_08034CAC(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08034CB1u) {",
        "    block_08034CB0(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080385D9u) {",
        "    block_080385D8(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080385DFu) {",
        "    block_080385DE(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080385E5u) {",
        "    block_080385E4(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080385BDu) {",
        "    block_080385BC(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08038601u) {",
        "    block_08038600(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08038605u) {",
        "    block_08038604(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08038609u) {",
        "    block_08038608(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0803860Du) {",
        "    block_0803860C(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080386B5u) {",
        "    block_080386B4(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0803872Bu) {",
        "    block_0803872A(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08038735u) {",
        "    block_08038734(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08038751u) {",
        "    block_08038750(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08038755u) {",
        "    block_08038754(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08038759u) {",
        "    block_08038758(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0803875Fu) {",
        "    block_0803875E(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08038763u) {",
        "    block_08038762(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x08038767u) {",
        "    block_08038766(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0803876Fu) {",
        "    block_0803876E(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080387FDu) {",
        "    block_080387FC(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AE15u) {",
        "    block_0807AE14(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AE1Fu) {",
        "    block_0807AE1E(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AE29u) {",
        "    block_0807AE28(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AE37u) {",
        "    block_0807AE36(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AE51u) {",
        "    block_0807AE50(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x080796C5u) {",
        "    block_080796C4(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0801B841u) {",
        "    block_0801B840(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807B2DDu) {",
        "    block_0807B2DC(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AD01u) {",
        "    block_0807AD00(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AE61u) {",
        "    block_0807AE60(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AD0Bu) {",
        "    block_0807AD0A(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AD0Fu) {",
        "    block_0807AD0E(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807ACE9u) {",
        "    block_0807ACE8(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807ACF5u) {",
        "    block_0807ACF4(state);",
        "    return;",
        "  }",
        "  if (state.stop_target == 0x0807AFF5u) {",
        "    block_0807AFF4(state);",
        "    return;",
        "  }",
        "  throw std::runtime_error(\"no generated block for stop target\");",
        "}",
        "",
        "}  // namespace aw::generated",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def emit_manifest(path, arm_ops, thumb_display_zero_helper_ops, thumb_display_short_zero_helper_ops, thumb_display_flag_reset_helper_ops, thumb_register_reset_ops, thumb_register_reset_after_zero_ops, thumb_register_reset_after_second_zero_ops, thumb_register_reset_return_ops, thumb_register_sync_ops, thumb_second_register_sync_ops, thumb_seed_store_ops, thumb_display_reset_wrapper_ops, thumb_display_reset_wrapper_return_ops, thumb_input_reset_wrapper_ops, thumb_input_reset_after_register_sync_ops, thumb_input_reset_tail_ops, thumb_alloc_init_ops, thumb_global_init_ops, thumb_global_init_after_save_ops, thumb_global_init_return_ops, thumb_save_probe_ops, thumb_table_init_ops, thumb_object_state_check_ops, thumb_state_query_ops, thumb_object_copy_ops, thumb_object_setup_wrapper_ops, thumb_object_setup_continuation_ops, thumb_object_setup_return_ops, thumb_slot_init_ops, thumb_slot_loop_continuation_ops, thumb_slot_init_return_ops, thumb_slot_helper_ops, thumb_slot_helper_after_query_ops, thumb_large_boot_ops, thumb_large_boot_after_display_ops, thumb_large_boot_alloc_result_ops, thumb_large_boot_after_global_init_ops, thumb_large_boot_after_object_setup_ops, thumb_large_boot_after_slot_init_ops, thumb_large_boot_after_seed_ops, thumb_large_boot_after_display_reset_ops, thumb_large_boot_after_input_reset_ops, thumb_large_boot_default_state_ops, thumb_large_boot_save_path_ops, thumb_refresh_init_wrapper_ops, thumb_system_init_ops, thumb_engine_clear_ops, thumb_engine_register_reset_ops, thumb_engine_clear_after_register_reset_ops, thumb_engine_clear_return_ops, thumb_system_init_after_engine_clear_ops, thumb_state_store_first_ops, thumb_system_init_after_first_state_store_ops, thumb_state_store_second_ops, thumb_system_init_clear_globals_ops, thumb_system_init_local_reset_ops, thumb_system_init_dma_copy_call_ops, thumb_dma_setup_ops, thumb_bios_cpuset_boot_ops, thumb_dma_setup_return_ops, thumb_system_init_next_call_ops, thumb_next_initializer_ops, thumb_byte_reset_helper_ops, thumb_next_initializer_after_reset_ops, thumb_table_copy_helper_ops, thumb_next_initializer_return_ops, thumb_system_init_mode_call_ops, thumb_mode_initializer_ops, thumb_defaults_initializer_ops, thumb_order_helper_ops, thumb_defaults_after_order_ops, thumb_mode_initializer_after_defaults_ops, thumb_mode_clear_helper_ops, thumb_mode_initializer_return_ops, thumb_system_init_unit_call_ops, thumb_unit_initializer_ops, thumb_global_table_reset_ops, thumb_unit_initializer_after_reset_ops, thumb_unit_entry_setup_ops, thumb_unit_geometry_helper_ops, thumb_callback_register_ops, thumb_display_setup_ops, thumb_display_setup_continuation_ops, thumb_display_second_dma_ops, thumb_display_setup_tail_ops, thumb_dma_helper_ops, thumb_entry_ops, thumb_entry_second_call_ops, thumb_helper_ops, thumb_continuation_ops, thumb_outer_return_ops, thumb_init_copy_ops, thumb_copy_return_ops, thumb_hardware_ops, thumb_struct_init_ops, thumb_first_math_wrapper_ops, thumb_trig_lookup_ops, thumb_first_math_return_ops, thumb_unit_geometry_after_first_math_ops, thumb_signed_divide_helper_ops, thumb_unit_geometry_after_first_divide_ops):
    stop_target = None
    for op in arm_ops:
        if op[0] == "bx":
            stop_target = f"register:{reg_name(op[2])}"
            break
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "\n".join([
            "entry=0x080000C0",
            "thumb_display_zero_helper=0x08010444",
            "thumb_display_short_zero_helper=0x080104B0",
            "thumb_display_flag_reset_helper=0x080104D4",
            "thumb_register_reset=0x08010544",
            "thumb_register_reset_after_zero=0x0801055A",
            "thumb_register_reset_after_second_zero=0x0801055E",
            "thumb_register_reset_return=0x08010562",
            "thumb_register_sync=0x08010574",
            "thumb_second_register_sync=0x080106A4",
            "thumb_seed_store=0x08010A78",
            "thumb_display_reset_wrapper=0x08010968",
            "thumb_display_reset_wrapper_return=0x0801096E",
            "thumb_input_reset_wrapper=0x08010974",
            "thumb_input_reset_after_register_sync=0x0801097A",
            "thumb_input_reset_tail=0x0801097E",
            "thumb_alloc_init=0x08012CF0",
            "thumb_global_init=0x0801A768",
            "thumb_global_init_after_save=0x0801A784",
            "thumb_global_init_return=0x0801A78A",
            "thumb_save_probe=0x0801AFC8",
            "thumb_table_init=0x0801B2B8",
            "thumb_object_state_check=0x0801AC1C",
            "thumb_state_query=0x0801AD38",
            "thumb_object_copy=0x08015D1C",
            "thumb_object_setup_wrapper=0x08015FB4",
            "thumb_object_setup_continuation=0x08015FC0",
            "thumb_object_setup_return=0x08015FCA",
            "thumb_slot_init=0x0803F87C",
            "thumb_slot_loop_continuation=0x0803F886",
            "thumb_slot_init_return=0x0803F890",
            "thumb_slot_helper=0x0803F898",
            "thumb_slot_helper_after_query=0x0803F8B0",
            "thumb_large_boot=0x080386E4",
            "thumb_large_boot_after_display=0x0803872A",
            "thumb_large_boot_alloc_result=0x08038734",
            "thumb_large_boot_after_global_init=0x08038750",
            "thumb_large_boot_after_object_setup=0x08038754",
            "thumb_large_boot_after_slot_init=0x08038758",
            "thumb_large_boot_after_seed=0x0803875E",
            "thumb_large_boot_after_display_reset=0x08038762",
            "thumb_large_boot_after_input_reset=0x08038766",
            "thumb_large_boot_default_state=0x0803876E",
            "thumb_large_boot_save_path=0x080387FC",
            "thumb_refresh_init_wrapper=0x080386B4",
            "thumb_system_init=0x080385CC",
            "thumb_engine_clear=0x0800F1B0",
            "thumb_engine_register_reset=0x0800F170",
            "thumb_engine_clear_after_register_reset=0x0800F1B6",
            "thumb_engine_clear_return=0x0800F1CA",
            "thumb_system_init_after_engine_clear=0x080385D8",
            "thumb_state_store_first=0x08038260",
            "thumb_system_init_after_first_state_store=0x080385DE",
            "thumb_state_store_second=0x0803826C",
            "thumb_system_init_clear_globals=0x080385E4",
            "thumb_system_init_local_reset=0x080385BC",
            "thumb_system_init_dma_copy_call=0x08038600",
            "thumb_dma_setup=0x0801B820",
            "thumb_bios_cpuset_boot=0x080796C4",
            "thumb_dma_setup_return=0x0801B840",
            "thumb_system_init_next_call=0x08038604",
            "thumb_next_initializer=0x08034C98",
            "thumb_byte_reset_helper=0x08018AAC",
            "thumb_next_initializer_after_reset=0x08034CAC",
            "thumb_table_copy_helper=0x0801F114",
            "thumb_next_initializer_return=0x08034CB0",
            "thumb_system_init_mode_call=0x08038608",
            "thumb_mode_initializer=0x08034C74",
            "thumb_defaults_initializer=0x08034BA8",
            "thumb_order_helper=0x08024EC4",
            "thumb_defaults_after_order=0x08034BFA",
            "thumb_mode_initializer_after_defaults=0x08034C86",
            "thumb_mode_clear_helper=0x08034C64",
            "thumb_mode_initializer_return=0x08034C8A",
            "thumb_system_init_unit_call=0x0803860C",
            "thumb_unit_initializer=0x080149E0",
            "thumb_global_table_reset=0x0801A69C",
            "thumb_unit_initializer_after_reset=0x080149EC",
            "thumb_unit_entry_setup=0x08014C98",
            "thumb_unit_geometry_helper=0x08014BF8",
            "thumb_first_math_wrapper=0x0807AF30",
            "thumb_trig_lookup=0x0807AED4",
            "thumb_first_math_return=0x0807AF38",
            "thumb_unit_geometry_after_first_math=0x08014C14",
            "thumb_signed_divide_helper=0x0807B488",
            "thumb_unit_geometry_after_first_divide=0x08014C20",
            "thumb_callback_register=0x0807AE50",
            "thumb_display_setup=0x0807AE14",
            "thumb_display_setup_continuation=0x0807AE1E",
            "thumb_display_second_dma=0x0807AE28",
            "thumb_display_setup_tail=0x0807AE36",
            "thumb_dma_helper=0x0807B2DC",
            "thumb_helper=0x0807AD00",
            "thumb_helper_continuation=0x0807AD0A",
            "thumb_outer_return=0x0807AD0E",
            "thumb_init_copy=0x0807ACE8",
            "thumb_copy_return=0x0807ACF4",
            "thumb_entry=0x0807AD10",
            "thumb_entry_second_call=0x0807AD16",
            "thumb_hardware_helper=0x0807AE60",
            "thumb_struct_init=0x0807AFF4",
            f"arm_instruction_count={len(arm_ops)}",
            f"thumb_display_zero_helper_instruction_count={len(thumb_display_zero_helper_ops)}",
            f"thumb_display_short_zero_helper_instruction_count={len(thumb_display_short_zero_helper_ops)}",
            f"thumb_display_flag_reset_helper_instruction_count={len(thumb_display_flag_reset_helper_ops)}",
            f"thumb_register_reset_instruction_count={len(thumb_register_reset_ops)}",
            f"thumb_register_reset_after_zero_instruction_count={len(thumb_register_reset_after_zero_ops)}",
            f"thumb_register_reset_after_second_zero_instruction_count={len(thumb_register_reset_after_second_zero_ops)}",
            f"thumb_register_reset_return_instruction_count={len(thumb_register_reset_return_ops)}",
            f"thumb_register_sync_instruction_count={len(thumb_register_sync_ops)}",
            f"thumb_second_register_sync_instruction_count={len(thumb_second_register_sync_ops)}",
            f"thumb_seed_store_instruction_count={len(thumb_seed_store_ops)}",
            f"thumb_display_reset_wrapper_instruction_count={len(thumb_display_reset_wrapper_ops)}",
            f"thumb_display_reset_wrapper_return_instruction_count={len(thumb_display_reset_wrapper_return_ops)}",
            f"thumb_input_reset_wrapper_instruction_count={len(thumb_input_reset_wrapper_ops)}",
            f"thumb_input_reset_after_register_sync_instruction_count={len(thumb_input_reset_after_register_sync_ops)}",
            f"thumb_input_reset_tail_instruction_count={len(thumb_input_reset_tail_ops)}",
            f"thumb_alloc_init_instruction_count={len(thumb_alloc_init_ops)}",
            f"thumb_global_init_instruction_count={len(thumb_global_init_ops)}",
            f"thumb_global_init_after_save_instruction_count={len(thumb_global_init_after_save_ops)}",
            f"thumb_global_init_return_instruction_count={len(thumb_global_init_return_ops)}",
            f"thumb_save_probe_instruction_count={len(thumb_save_probe_ops)}",
            f"thumb_table_init_instruction_count={len(thumb_table_init_ops)}",
            f"thumb_object_state_check_instruction_count={len(thumb_object_state_check_ops)}",
            f"thumb_state_query_instruction_count={len(thumb_state_query_ops)}",
            f"thumb_object_copy_instruction_count={len(thumb_object_copy_ops)}",
            f"thumb_object_setup_wrapper_instruction_count={len(thumb_object_setup_wrapper_ops)}",
            f"thumb_object_setup_continuation_instruction_count={len(thumb_object_setup_continuation_ops)}",
            f"thumb_object_setup_return_instruction_count={len(thumb_object_setup_return_ops)}",
            f"thumb_slot_init_instruction_count={len(thumb_slot_init_ops)}",
            f"thumb_slot_loop_continuation_instruction_count={len(thumb_slot_loop_continuation_ops)}",
            f"thumb_slot_init_return_instruction_count={len(thumb_slot_init_return_ops)}",
            f"thumb_slot_helper_instruction_count={len(thumb_slot_helper_ops)}",
            f"thumb_slot_helper_after_query_instruction_count={len(thumb_slot_helper_after_query_ops)}",
            f"thumb_large_boot_instruction_count={len(thumb_large_boot_ops)}",
            f"thumb_large_boot_after_display_instruction_count={len(thumb_large_boot_after_display_ops)}",
            f"thumb_large_boot_alloc_result_instruction_count={len(thumb_large_boot_alloc_result_ops)}",
            f"thumb_large_boot_after_global_init_instruction_count={len(thumb_large_boot_after_global_init_ops)}",
            f"thumb_large_boot_after_object_setup_instruction_count={len(thumb_large_boot_after_object_setup_ops)}",
            f"thumb_large_boot_after_slot_init_instruction_count={len(thumb_large_boot_after_slot_init_ops)}",
            f"thumb_large_boot_after_seed_instruction_count={len(thumb_large_boot_after_seed_ops)}",
            f"thumb_large_boot_after_display_reset_instruction_count={len(thumb_large_boot_after_display_reset_ops)}",
            f"thumb_large_boot_after_input_reset_instruction_count={len(thumb_large_boot_after_input_reset_ops)}",
            f"thumb_large_boot_default_state_instruction_count={len(thumb_large_boot_default_state_ops)}",
            f"thumb_large_boot_save_path_instruction_count={len(thumb_large_boot_save_path_ops)}",
            f"thumb_refresh_init_wrapper_instruction_count={len(thumb_refresh_init_wrapper_ops)}",
            f"thumb_system_init_instruction_count={len(thumb_system_init_ops)}",
            f"thumb_engine_clear_instruction_count={len(thumb_engine_clear_ops)}",
            f"thumb_engine_register_reset_instruction_count={len(thumb_engine_register_reset_ops)}",
            f"thumb_engine_clear_after_register_reset_instruction_count={len(thumb_engine_clear_after_register_reset_ops)}",
            f"thumb_engine_clear_return_instruction_count={len(thumb_engine_clear_return_ops)}",
            f"thumb_system_init_after_engine_clear_instruction_count={len(thumb_system_init_after_engine_clear_ops)}",
            f"thumb_state_store_first_instruction_count={len(thumb_state_store_first_ops)}",
            f"thumb_system_init_after_first_state_store_instruction_count={len(thumb_system_init_after_first_state_store_ops)}",
            f"thumb_state_store_second_instruction_count={len(thumb_state_store_second_ops)}",
            f"thumb_system_init_clear_globals_instruction_count={len(thumb_system_init_clear_globals_ops)}",
            f"thumb_system_init_local_reset_instruction_count={len(thumb_system_init_local_reset_ops)}",
            f"thumb_system_init_dma_copy_call_instruction_count={len(thumb_system_init_dma_copy_call_ops)}",
            f"thumb_dma_setup_instruction_count={len(thumb_dma_setup_ops)}",
            f"thumb_bios_cpuset_boot_instruction_count={len(thumb_bios_cpuset_boot_ops)}",
            f"thumb_dma_setup_return_instruction_count={len(thumb_dma_setup_return_ops)}",
            f"thumb_system_init_next_call_instruction_count={len(thumb_system_init_next_call_ops)}",
            f"thumb_next_initializer_instruction_count={len(thumb_next_initializer_ops)}",
            f"thumb_byte_reset_helper_instruction_count={len(thumb_byte_reset_helper_ops)}",
            f"thumb_next_initializer_after_reset_instruction_count={len(thumb_next_initializer_after_reset_ops)}",
            f"thumb_table_copy_helper_instruction_count={len(thumb_table_copy_helper_ops)}",
            f"thumb_next_initializer_return_instruction_count={len(thumb_next_initializer_return_ops)}",
            f"thumb_system_init_mode_call_instruction_count={len(thumb_system_init_mode_call_ops)}",
            f"thumb_mode_initializer_instruction_count={len(thumb_mode_initializer_ops)}",
            f"thumb_defaults_initializer_instruction_count={len(thumb_defaults_initializer_ops)}",
            f"thumb_order_helper_instruction_count={len(thumb_order_helper_ops)}",
            f"thumb_defaults_after_order_instruction_count={len(thumb_defaults_after_order_ops)}",
            f"thumb_mode_initializer_after_defaults_instruction_count={len(thumb_mode_initializer_after_defaults_ops)}",
            f"thumb_mode_clear_helper_instruction_count={len(thumb_mode_clear_helper_ops)}",
            f"thumb_mode_initializer_return_instruction_count={len(thumb_mode_initializer_return_ops)}",
            f"thumb_system_init_unit_call_instruction_count={len(thumb_system_init_unit_call_ops)}",
            f"thumb_unit_initializer_instruction_count={len(thumb_unit_initializer_ops)}",
            f"thumb_global_table_reset_instruction_count={len(thumb_global_table_reset_ops)}",
            f"thumb_unit_initializer_after_reset_instruction_count={len(thumb_unit_initializer_after_reset_ops)}",
            f"thumb_unit_entry_setup_instruction_count={len(thumb_unit_entry_setup_ops)}",
            f"thumb_unit_geometry_helper_instruction_count={len(thumb_unit_geometry_helper_ops)}",
            f"thumb_first_math_wrapper_instruction_count={len(thumb_first_math_wrapper_ops)}",
            f"thumb_trig_lookup_instruction_count={len(thumb_trig_lookup_ops)}",
            f"thumb_first_math_return_instruction_count={len(thumb_first_math_return_ops)}",
            f"thumb_unit_geometry_after_first_math_instruction_count={len(thumb_unit_geometry_after_first_math_ops)}",
            f"thumb_signed_divide_helper_instruction_count={len(thumb_signed_divide_helper_ops)}",
            f"thumb_unit_geometry_after_first_divide_instruction_count={len(thumb_unit_geometry_after_first_divide_ops)}",
            f"thumb_callback_register_instruction_count={len(thumb_callback_register_ops)}",
            f"thumb_display_setup_instruction_count={len(thumb_display_setup_ops)}",
            f"thumb_display_setup_continuation_instruction_count={len(thumb_display_setup_continuation_ops)}",
            f"thumb_display_second_dma_instruction_count={len(thumb_display_second_dma_ops)}",
            f"thumb_display_setup_tail_instruction_count={len(thumb_display_setup_tail_ops)}",
            f"thumb_dma_helper_instruction_count={len(thumb_dma_helper_ops)}",
            f"thumb_entry_instruction_count={len(thumb_entry_ops)}",
            f"thumb_entry_second_call_instruction_count={len(thumb_entry_second_call_ops)}",
            f"thumb_helper_instruction_count={len(thumb_helper_ops)}",
            f"thumb_helper_continuation_instruction_count={len(thumb_continuation_ops)}",
            f"thumb_outer_return_instruction_count={len(thumb_outer_return_ops)}",
            f"thumb_init_copy_instruction_count={len(thumb_init_copy_ops)}",
            f"thumb_copy_return_instruction_count={len(thumb_copy_return_ops)}",
            f"thumb_hardware_instruction_count={len(thumb_hardware_ops)}",
            f"thumb_struct_init_instruction_count={len(thumb_struct_init_ops)}",
            f"stop_target={stop_target}",
            "",
        ]),
        encoding="utf-8",
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", required=True)
    parser.add_argument("--header", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--manifest", required=True)
    args = parser.parse_args()

    data = Path(args.rom).read_bytes()
    arm_ops = decode_entry_block(data)
    thumb_display_zero_helper_ops = decode_thumb_display_zero_helper_block(data)
    thumb_display_short_zero_helper_ops = decode_thumb_display_short_zero_helper_block(data)
    thumb_display_flag_reset_helper_ops = decode_thumb_display_flag_reset_helper_block(data)
    thumb_register_reset_ops = decode_thumb_register_reset_block(data)
    thumb_register_reset_after_zero_ops = decode_thumb_register_reset_after_zero_block(data)
    thumb_register_reset_after_second_zero_ops = decode_thumb_register_reset_after_second_zero_block(data)
    thumb_register_reset_return_ops = decode_thumb_register_reset_return_block(data)
    thumb_register_sync_ops = decode_thumb_register_sync_block(data)
    thumb_second_register_sync_ops = decode_thumb_second_register_sync_block(data)
    thumb_seed_store_ops = decode_thumb_seed_store_block(data)
    thumb_display_reset_wrapper_ops = decode_thumb_display_reset_wrapper_block(data)
    thumb_display_reset_wrapper_return_ops = decode_thumb_display_reset_wrapper_return_block(data)
    thumb_input_reset_wrapper_ops = decode_thumb_input_reset_wrapper_block(data)
    thumb_input_reset_after_register_sync_ops = decode_thumb_input_reset_after_register_sync_block(data)
    thumb_input_reset_tail_ops = decode_thumb_input_reset_tail_block(data)
    thumb_alloc_init_ops = decode_thumb_alloc_init_block(data)
    thumb_global_init_ops = decode_thumb_global_init_block(data)
    thumb_global_init_after_save_ops = decode_thumb_global_init_after_save_block(data)
    thumb_global_init_return_ops = decode_thumb_global_init_return_block(data)
    thumb_save_probe_ops = decode_thumb_save_probe_block(data)
    thumb_table_init_ops = decode_thumb_table_init_block(data)
    thumb_object_state_check_ops = decode_thumb_object_state_check_block(data)
    thumb_state_query_ops = decode_thumb_state_query_block(data)
    thumb_object_copy_ops = decode_thumb_object_copy_block(data)
    thumb_object_setup_wrapper_ops = decode_thumb_object_setup_wrapper_block(data)
    thumb_object_setup_continuation_ops = decode_thumb_object_setup_continuation_block(data)
    thumb_object_setup_return_ops = decode_thumb_object_setup_return_block(data)
    thumb_slot_init_ops = decode_thumb_slot_init_block(data)
    thumb_slot_loop_continuation_ops = decode_thumb_slot_loop_continuation_block(data)
    thumb_slot_init_return_ops = decode_thumb_slot_init_return_block(data)
    thumb_slot_helper_ops = decode_thumb_slot_helper_block(data)
    thumb_slot_helper_after_query_ops = decode_thumb_slot_helper_after_query_block(data)
    thumb_large_boot_ops = decode_thumb_large_boot_block(data)
    thumb_large_boot_after_display_ops = decode_thumb_large_boot_after_display_block(data)
    thumb_large_boot_alloc_result_ops = decode_thumb_large_boot_alloc_result_block(data)
    thumb_large_boot_after_global_init_ops = decode_thumb_large_boot_after_global_init_block(data)
    thumb_large_boot_after_object_setup_ops = decode_thumb_large_boot_after_object_setup_block(data)
    thumb_large_boot_after_slot_init_ops = decode_thumb_large_boot_after_slot_init_block(data)
    thumb_large_boot_after_seed_ops = decode_thumb_large_boot_after_seed_block(data)
    thumb_large_boot_after_display_reset_ops = decode_thumb_large_boot_after_display_reset_block(data)
    thumb_large_boot_after_input_reset_ops = decode_thumb_large_boot_after_input_reset_block(data)
    thumb_large_boot_default_state_ops = decode_thumb_large_boot_default_state_block(data)
    thumb_large_boot_save_path_ops = decode_thumb_large_boot_save_path_block(data)
    thumb_refresh_init_wrapper_ops = decode_thumb_refresh_init_wrapper_block(data)
    thumb_system_init_ops = decode_thumb_system_init_block(data)
    thumb_engine_clear_ops = decode_thumb_engine_clear_block(data)
    thumb_engine_register_reset_ops = decode_thumb_engine_register_reset_block(data)
    thumb_engine_clear_after_register_reset_ops = decode_thumb_engine_clear_after_register_reset_block(data)
    thumb_engine_clear_return_ops = decode_thumb_engine_clear_return_block(data)
    thumb_system_init_after_engine_clear_ops = decode_thumb_system_init_after_engine_clear_block(data)
    thumb_state_store_first_ops = decode_thumb_state_store_first_block(data)
    thumb_system_init_after_first_state_store_ops = decode_thumb_system_init_after_first_state_store_block(data)
    thumb_state_store_second_ops = decode_thumb_state_store_second_block(data)
    thumb_system_init_clear_globals_ops = decode_thumb_system_init_clear_globals_block(data)
    thumb_system_init_local_reset_ops = decode_thumb_system_init_local_reset_block(data)
    thumb_system_init_dma_copy_call_ops = decode_thumb_system_init_dma_copy_call_block(data)
    thumb_dma_setup_ops = decode_thumb_dma_setup_block(data)
    thumb_bios_cpuset_boot_ops = decode_thumb_bios_cpuset_boot_block(data)
    thumb_dma_setup_return_ops = decode_thumb_dma_setup_return_block(data)
    thumb_system_init_next_call_ops = decode_thumb_system_init_next_call_block(data)
    thumb_next_initializer_ops = decode_thumb_next_initializer_block(data)
    thumb_byte_reset_helper_ops = decode_thumb_byte_reset_helper_block(data)
    thumb_next_initializer_after_reset_ops = decode_thumb_next_initializer_after_reset_block(data)
    thumb_table_copy_helper_ops = decode_thumb_table_copy_helper_block(data)
    thumb_next_initializer_return_ops = decode_thumb_next_initializer_return_block(data)
    thumb_system_init_mode_call_ops = decode_thumb_system_init_mode_call_block(data)
    thumb_mode_initializer_ops = decode_thumb_mode_initializer_block(data)
    thumb_defaults_initializer_ops = decode_thumb_defaults_initializer_block(data)
    thumb_order_helper_ops = decode_thumb_order_helper_block(data)
    thumb_defaults_after_order_ops = decode_thumb_defaults_after_order_block(data)
    thumb_mode_initializer_after_defaults_ops = decode_thumb_mode_initializer_after_defaults_block(data)
    thumb_mode_clear_helper_ops = decode_thumb_mode_clear_helper_block(data)
    thumb_mode_initializer_return_ops = decode_thumb_mode_initializer_return_block(data)
    thumb_system_init_unit_call_ops = decode_thumb_system_init_unit_call_block(data)
    thumb_unit_initializer_ops = decode_thumb_unit_initializer_block(data)
    thumb_global_table_reset_ops = decode_thumb_global_table_reset_block(data)
    thumb_unit_initializer_after_reset_ops = decode_thumb_unit_initializer_after_reset_block(data)
    thumb_unit_entry_setup_ops = decode_thumb_unit_entry_setup_block(data)
    thumb_unit_geometry_helper_ops = decode_thumb_unit_geometry_helper_block(data)
    thumb_first_math_wrapper_ops = decode_thumb_first_math_wrapper_block(data)
    thumb_trig_lookup_ops = decode_thumb_trig_lookup_block(data)
    thumb_first_math_return_ops = decode_thumb_first_math_return_block(data)
    thumb_unit_geometry_after_first_math_ops = decode_thumb_unit_geometry_after_first_math_block(data)
    thumb_signed_divide_helper_ops = decode_thumb_signed_divide_helper_block(data)
    thumb_unit_geometry_after_first_divide_ops = decode_thumb_unit_geometry_after_first_divide_block(data)
    thumb_callback_register_ops = decode_thumb_callback_register_block(data)
    thumb_display_setup_ops = decode_thumb_display_setup_block(data)
    thumb_display_setup_continuation_ops = decode_thumb_display_setup_continuation_block(data)
    thumb_display_second_dma_ops = decode_thumb_display_second_dma_block(data)
    thumb_display_setup_tail_ops = decode_thumb_display_setup_tail_block(data)
    thumb_dma_helper_ops = decode_thumb_dma_helper_block(data)
    thumb_entry_ops = decode_thumb_entry_block(data)
    thumb_entry_second_call_ops = decode_thumb_entry_second_call_block(data)
    thumb_helper_ops = decode_thumb_helper_block(data)
    thumb_continuation_ops = decode_thumb_helper_continuation_block(data)
    thumb_outer_return_ops = decode_thumb_outer_return_block(data)
    thumb_init_copy_ops = decode_thumb_init_copy_block(data)
    thumb_copy_return_ops = decode_thumb_copy_return_block(data)
    thumb_hardware_ops = decode_thumb_hardware_helper_block(data)
    thumb_struct_init_ops = decode_thumb_struct_init_block(data)
    emit_header(Path(args.header))
    emit_source(Path(args.source), arm_ops, thumb_display_zero_helper_ops, thumb_display_short_zero_helper_ops, thumb_display_flag_reset_helper_ops, thumb_register_reset_ops, thumb_register_reset_after_zero_ops, thumb_register_reset_after_second_zero_ops, thumb_register_reset_return_ops, thumb_register_sync_ops, thumb_second_register_sync_ops, thumb_seed_store_ops, thumb_display_reset_wrapper_ops, thumb_display_reset_wrapper_return_ops, thumb_input_reset_wrapper_ops, thumb_input_reset_after_register_sync_ops, thumb_input_reset_tail_ops, thumb_alloc_init_ops, thumb_global_init_ops, thumb_global_init_after_save_ops, thumb_global_init_return_ops, thumb_save_probe_ops, thumb_table_init_ops, thumb_object_state_check_ops, thumb_state_query_ops, thumb_object_copy_ops, thumb_object_setup_wrapper_ops, thumb_object_setup_continuation_ops, thumb_object_setup_return_ops, thumb_slot_init_ops, thumb_slot_loop_continuation_ops, thumb_slot_init_return_ops, thumb_slot_helper_ops, thumb_slot_helper_after_query_ops, thumb_large_boot_ops, thumb_large_boot_after_display_ops, thumb_large_boot_alloc_result_ops, thumb_large_boot_after_global_init_ops, thumb_large_boot_after_object_setup_ops, thumb_large_boot_after_slot_init_ops, thumb_large_boot_after_seed_ops, thumb_large_boot_after_display_reset_ops, thumb_large_boot_after_input_reset_ops, thumb_large_boot_default_state_ops, thumb_large_boot_save_path_ops, thumb_refresh_init_wrapper_ops, thumb_system_init_ops, thumb_engine_clear_ops, thumb_engine_register_reset_ops, thumb_engine_clear_after_register_reset_ops, thumb_engine_clear_return_ops, thumb_system_init_after_engine_clear_ops, thumb_state_store_first_ops, thumb_system_init_after_first_state_store_ops, thumb_state_store_second_ops, thumb_system_init_clear_globals_ops, thumb_system_init_local_reset_ops, thumb_system_init_dma_copy_call_ops, thumb_dma_setup_ops, thumb_bios_cpuset_boot_ops, thumb_dma_setup_return_ops, thumb_system_init_next_call_ops, thumb_next_initializer_ops, thumb_byte_reset_helper_ops, thumb_next_initializer_after_reset_ops, thumb_table_copy_helper_ops, thumb_next_initializer_return_ops, thumb_system_init_mode_call_ops, thumb_mode_initializer_ops, thumb_defaults_initializer_ops, thumb_order_helper_ops, thumb_defaults_after_order_ops, thumb_mode_initializer_after_defaults_ops, thumb_mode_clear_helper_ops, thumb_mode_initializer_return_ops, thumb_system_init_unit_call_ops, thumb_unit_initializer_ops, thumb_global_table_reset_ops, thumb_unit_initializer_after_reset_ops, thumb_unit_entry_setup_ops, thumb_unit_geometry_helper_ops, thumb_callback_register_ops, thumb_display_setup_ops, thumb_display_setup_continuation_ops, thumb_display_second_dma_ops, thumb_display_setup_tail_ops, thumb_dma_helper_ops, thumb_entry_ops, thumb_entry_second_call_ops, thumb_helper_ops, thumb_continuation_ops, thumb_outer_return_ops, thumb_init_copy_ops, thumb_copy_return_ops, thumb_hardware_ops, thumb_struct_init_ops, thumb_first_math_wrapper_ops, thumb_trig_lookup_ops, thumb_first_math_return_ops, thumb_unit_geometry_after_first_math_ops, thumb_signed_divide_helper_ops, thumb_unit_geometry_after_first_divide_ops)
    emit_manifest(Path(args.manifest), arm_ops, thumb_display_zero_helper_ops, thumb_display_short_zero_helper_ops, thumb_display_flag_reset_helper_ops, thumb_register_reset_ops, thumb_register_reset_after_zero_ops, thumb_register_reset_after_second_zero_ops, thumb_register_reset_return_ops, thumb_register_sync_ops, thumb_second_register_sync_ops, thumb_seed_store_ops, thumb_display_reset_wrapper_ops, thumb_display_reset_wrapper_return_ops, thumb_input_reset_wrapper_ops, thumb_input_reset_after_register_sync_ops, thumb_input_reset_tail_ops, thumb_alloc_init_ops, thumb_global_init_ops, thumb_global_init_after_save_ops, thumb_global_init_return_ops, thumb_save_probe_ops, thumb_table_init_ops, thumb_object_state_check_ops, thumb_state_query_ops, thumb_object_copy_ops, thumb_object_setup_wrapper_ops, thumb_object_setup_continuation_ops, thumb_object_setup_return_ops, thumb_slot_init_ops, thumb_slot_loop_continuation_ops, thumb_slot_init_return_ops, thumb_slot_helper_ops, thumb_slot_helper_after_query_ops, thumb_large_boot_ops, thumb_large_boot_after_display_ops, thumb_large_boot_alloc_result_ops, thumb_large_boot_after_global_init_ops, thumb_large_boot_after_object_setup_ops, thumb_large_boot_after_slot_init_ops, thumb_large_boot_after_seed_ops, thumb_large_boot_after_display_reset_ops, thumb_large_boot_after_input_reset_ops, thumb_large_boot_default_state_ops, thumb_large_boot_save_path_ops, thumb_refresh_init_wrapper_ops, thumb_system_init_ops, thumb_engine_clear_ops, thumb_engine_register_reset_ops, thumb_engine_clear_after_register_reset_ops, thumb_engine_clear_return_ops, thumb_system_init_after_engine_clear_ops, thumb_state_store_first_ops, thumb_system_init_after_first_state_store_ops, thumb_state_store_second_ops, thumb_system_init_clear_globals_ops, thumb_system_init_local_reset_ops, thumb_system_init_dma_copy_call_ops, thumb_dma_setup_ops, thumb_bios_cpuset_boot_ops, thumb_dma_setup_return_ops, thumb_system_init_next_call_ops, thumb_next_initializer_ops, thumb_byte_reset_helper_ops, thumb_next_initializer_after_reset_ops, thumb_table_copy_helper_ops, thumb_next_initializer_return_ops, thumb_system_init_mode_call_ops, thumb_mode_initializer_ops, thumb_defaults_initializer_ops, thumb_order_helper_ops, thumb_defaults_after_order_ops, thumb_mode_initializer_after_defaults_ops, thumb_mode_clear_helper_ops, thumb_mode_initializer_return_ops, thumb_system_init_unit_call_ops, thumb_unit_initializer_ops, thumb_global_table_reset_ops, thumb_unit_initializer_after_reset_ops, thumb_unit_entry_setup_ops, thumb_unit_geometry_helper_ops, thumb_callback_register_ops, thumb_display_setup_ops, thumb_display_setup_continuation_ops, thumb_display_second_dma_ops, thumb_display_setup_tail_ops, thumb_dma_helper_ops, thumb_entry_ops, thumb_entry_second_call_ops, thumb_helper_ops, thumb_continuation_ops, thumb_outer_return_ops, thumb_init_copy_ops, thumb_copy_return_ops, thumb_hardware_ops, thumb_struct_init_ops, thumb_first_math_wrapper_ops, thumb_trig_lookup_ops, thumb_first_math_return_ops, thumb_unit_geometry_after_first_math_ops, thumb_signed_divide_helper_ops, thumb_unit_geometry_after_first_divide_ops)


if __name__ == "__main__":
    main()




