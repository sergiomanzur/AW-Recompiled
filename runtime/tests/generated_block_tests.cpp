#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "aw/cpu_state.hpp"
#include "aw/generated_blocks.hpp"
#include "aw/memory.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

void executes_entry_block_until_thumb_handoff() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);

  require_equal(state.regs[0], 0x0000001Fu, "r0");
  require_equal(state.regs[1], 0x0807AD11u, "r1");
  require_equal(state.regs[13], 0x03007C00u, "sp");
  require_equal(state.regs[14], 0x080000E4u, "lr");
  require_equal(state.stop_target, 0x0807AD11u, "stop target");
  require_equal(state.trace_lines.front(), std::string("Executing generated block 0x080000C0"), "first trace");
}

void dispatches_thumb_handoff_until_first_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  aw::generated::dispatch_one(state);

  require_equal(state.thumb, true, "thumb mode");
  require_equal(state.regs[13], 0x03007BFCu, "sp after thumb push");
  require_equal(aw::read32(state.memory, 0x03007BFC), 0x080000E4u, "pushed lr");
  require_equal(state.regs[14], 0x0807AD17u, "thumb bl lr");
  require_equal(state.stop_target, 0x0807AD01u, "thumb call target");
}

void dispatches_called_thumb_helper_until_first_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);

  require_equal(state.regs[0], 0x00000000u, "r0");
  require_equal(state.regs[1], 0x00000000u, "r1");
  require_equal(state.regs[13], 0x03007BF8u, "sp after helper push");
  require_equal(aw::read32(state.memory, 0x03007BF8), 0x0807AD17u, "helper pushed lr");
  require_equal(state.regs[14], 0x0807AD0Bu, "helper thumb bl lr");
  require_equal(state.stop_target, 0x0807AE61u, "helper call target");
}

void dispatches_hardware_state_helper_until_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);

  require_equal(aw::read32(state.memory, 0x03000710), 0x00000000u, "interrupt shadow");
  require_equal(aw::read16(state.memory, 0x04000200), std::uint16_t{0x0000}, "io 0x04000200");
  require_equal(aw::read16(state.memory, 0x04000208), std::uint16_t{0x0000}, "io 0x04000208");
  require_equal(state.stop_target, 0x0807AD0Bu, "return target");
}

void dispatches_helper_continuation_until_next_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);

  require_equal(state.regs[14], 0x0807AD0Fu, "continuation thumb bl lr");
  require_equal(state.stop_target, 0x0807ACE9u, "continuation call target");
}

void dispatches_next_helper_until_copy_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);

  require_equal(state.regs[0], 0x03006560u, "copy source");
  require_equal(state.regs[1], 0x03006570u, "copy destination");
  require_equal(state.regs[2], 0x00000010u, "copy size");
  require_equal(state.regs[13], 0x03007BF4u, "sp after next helper push");
  require_equal(aw::read32(state.memory, 0x03007BF4), 0x0807AD0Fu, "next helper pushed lr");
  require_equal(state.regs[14], 0x0807ACF5u, "next helper thumb bl lr");
  require_equal(state.stop_target, 0x0807AFF5u, "copy call target");
}

void dispatches_structure_init_until_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);

  require_equal(aw::read8(state.memory, 0x03006560), std::uint8_t{0}, "struct flags");
  require_equal(aw::read8(state.memory, 0x03006561), std::uint8_t{0x10}, "struct count");
  require_equal(aw::read8(state.memory, 0x03006562), std::uint8_t{0}, "struct mode");
  require_equal(aw::read32(state.memory, 0x03006564), 0x00000000u, "struct cursor");
  require_equal(aw::read32(state.memory, 0x03006568), 0x03006570u, "struct head");
  require_equal(aw::read32(state.memory, 0x0300656C), 0x03006570u, "struct base");
  for (std::uint32_t i = 0; i < 16; ++i) {
    require_equal(aw::read32(state.memory, 0x03006570 + i * 12), 0x00000000u, "cleared slot");
  }
  require_equal(state.stop_target, 0x0807ACF5u, "structure init return");
}

void dispatches_copy_helper_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);

  require_equal(state.regs[13], 0x03007BF8u, "sp after copy helper return");
  require_equal(state.stop_target, 0x0807AD0Fu, "copy helper return target");
}

void dispatches_outer_helper_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);
  aw::generated::dispatch_one(state);

  require_equal(state.regs[13], 0x03007BFCu, "sp after outer helper return");
  require_equal(state.stop_target, 0x0807AD17u, "outer helper return target");
}

void dispatches_entry_second_call_site() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 9; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x0807AD1Bu, "second call lr");
  require_equal(state.stop_target, 0x080386E5u, "second call target");
}

void dispatches_large_boot_routine_until_first_nested_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 10; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BE4u, "large routine stack frame");
  require_equal(aw::read32(state.memory, 0x03007BF4), 0x00000000u, "large routine pushed r4");
  require_equal(aw::read32(state.memory, 0x03007BF8), 0x0807AD1Bu, "large routine pushed lr");
  require_equal(aw::read32(state.memory, 0x03007BEC), 0x00000000u, "dma source local");
  require_equal(aw::read16(state.memory, 0x03007BF0), std::uint16_t{0x0000}, "dma halfword local");
  require_equal(aw::read32(state.memory, 0x040000D4), 0x03007BF0u, "dma source register");
  require_equal(aw::read32(state.memory, 0x040000D8), 0x02000000u, "dma dest register");
  require_equal(aw::read32(state.memory, 0x040000DC), 0x04000204u, "dma control register");
  require_equal(aw::read16(state.memory, 0x04000204), std::uint16_t{0x45B4}, "io control write");
  require_equal(aw::read16(state.memory, 0x03007BE8), std::uint16_t{0x0000}, "key input local");
  require_equal(state.regs[14], 0x0803872Bu, "large routine bl lr");
  require_equal(state.stop_target, 0x0807AE15u, "large routine nested call target");
}

void dispatches_display_setup_wrapper_until_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 12; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BDCu, "display wrapper stack");
  require_equal(aw::read32(state.memory, 0x03007BDC), 0x00000000u, "display wrapper pushed r4");
  require_equal(aw::read32(state.memory, 0x03007BE0), 0x0803872Bu, "display wrapper pushed lr");
  require_equal(state.regs[14], 0x0807AE1Fu, "display wrapper helper lr");
  require_equal(state.stop_target, 0x0807AE1Fu, "display wrapper return target");
}

void dispatches_display_setup_continuation_until_data_init_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 13; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x0827D36Cu, "display setup source");
  require_equal(state.regs[1], 0x03006630u, "display setup destination");
  require_equal(state.regs[2], 0x0000001Eu, "display setup count");
  require_equal(state.regs[14], 0x0807AE29u, "display setup continuation lr");
  require_equal(state.stop_target, 0x0807B2DDu, "display setup data init target");
}

void dispatches_dma_helper_until_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 14; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read16(state.memory, 0x040000D4), std::uint16_t{0xD36C}, "dma source low");
  require_equal(aw::read16(state.memory, 0x040000D6), std::uint16_t{0x0827}, "dma source high");
  require_equal(aw::read16(state.memory, 0x040000D8), std::uint16_t{0x6630}, "dma destination low");
  require_equal(aw::read16(state.memory, 0x040000DA), std::uint16_t{0x0300}, "dma destination high");
  require_equal(aw::read16(state.memory, 0x040000DC), std::uint16_t{0x001E}, "dma count");
  require_equal(aw::read16(state.memory, 0x040000DE), std::uint16_t{0x8000}, "dma control");
  require_equal(state.stop_target, 0x0807AE29u, "dma helper return target");
}

void dispatches_second_display_dma_setup_until_data_init_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 15; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x0801BBCCu, "second display source");
  require_equal(state.regs[4], 0x03000718u, "second display scratch pointer");
  require_equal(state.regs[1], 0x03000718u, "second display destination");
  require_equal(state.regs[2], 0x00000100u, "second display count");
  require_equal(state.regs[14], 0x0807AE37u, "second display dma lr");
  require_equal(state.stop_target, 0x0807B2DDu, "second display data init target");
}

void dispatches_display_setup_tail_until_large_boot_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 17; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read32(state.memory, 0x03007FFCu), 0x03000718u, "display scratch pointer store");
  require_equal(state.regs[4], 0x00000000u, "restored r4");
  require_equal(state.regs[13], 0x03007BE4u, "sp after display setup return");
  require_equal(state.stop_target, 0x0803872Bu, "large boot return target");
}

void dispatches_large_boot_after_display_until_allocator_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 18; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x02008000u, "allocator arena base");
  require_equal(state.regs[1], 0x00008000u, "allocator arena size");
  require_equal(state.regs[14], 0x08038735u, "allocator call lr");
  require_equal(state.stop_target, 0x08012CF1u, "allocator init target");
}

void dispatches_allocator_init_until_large_boot_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 19; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read32(state.memory, 0x02008000u), 0x00000000u, "allocator next block");
  require_equal(aw::read32(state.memory, 0x02008004u), 0x00007FF0u, "allocator free size");
  require_equal(aw::read32(state.memory, 0x02008008u), 0x00000000u, "allocator used flag");
  require_equal(aw::read32(state.memory, 0x0200800Cu), 0x00008000u, "allocator arena total");
  require_equal(aw::read32(state.memory, 0x03000050u), 0x02008000u, "allocator global pointer");
  require_equal(state.regs[0], 0x00000000u, "allocator init result");
  require_equal(state.regs[13], 0x03007BE4u, "sp after allocator init");
  require_equal(state.stop_target, 0x08038735u, "allocator init return target");
}

void dispatches_large_boot_allocator_result_until_init_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 20; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x08015CA9u, "init call first callback");
  require_equal(state.regs[1], 0x08015B79u, "init call second callback");
  require_equal(state.regs[2], 0x0202D000u, "init call data pointer");
  require_equal(state.regs[3], 0x00000002u, "init call mode");
  require_equal(aw::read32(state.memory, 0x03007BE4u), 0x03003314u, "init call stack argument");
  require_equal(state.regs[14], 0x08038751u, "init call lr");
  require_equal(state.stop_target, 0x0801A769u, "post allocator init target");
}

void dispatches_global_init_until_first_nested_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 21; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read32(state.memory, 0x02012A54u), 0x08015CA9u, "global init first callback");
  require_equal(aw::read32(state.memory, 0x02012A58u), 0x08015B79u, "global init second callback");
  require_equal(aw::read32(state.memory, 0x02012A5Cu), 0x0202D000u, "global init data pointer");
  require_equal(aw::read8(state.memory, 0x02012A60u), std::uint8_t{0x02}, "global init mode");
  require_equal(aw::read32(state.memory, 0x02012A64u), 0x03003314u, "global init stack argument");
  require_equal(state.regs[13], 0x03007BD8u, "sp after global init push");
  require_equal(state.regs[14], 0x0801A785u, "global init first nested lr");
  require_equal(state.stop_target, 0x0801AFC9u, "global init first nested target");
}

void dispatches_save_probe_until_global_init_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 22; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read8(state.memory, 0x02012B3Cu), std::uint8_t{0x01}, "save probe normalized flag");
  require_equal(state.regs[13], 0x03007BD8u, "sp after save probe");
  require_equal(state.stop_target, 0x0801A785u, "save probe return target");
}

void dispatches_global_init_after_save_probe_until_second_nested_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 23; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00000000u, "second global init argument");
  require_equal(state.regs[14], 0x0801A78Bu, "second global init lr");
  require_equal(state.stop_target, 0x0801B2B9u, "second global init target");
}

void dispatches_table_init_until_global_init_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 24; ++i) {
    aw::generated::dispatch_one(state);
  }

  for (std::uint32_t address = 0x02012AF8u; address <= 0x02012B34u; address += 4u) {
    require_equal(aw::read32(state.memory, address), 0x00000000u, "cleared global table word");
  }
  require_equal(aw::read32(state.memory, 0x02012B38u), 0x00000000u, "global table selected index");
  require_equal(state.regs[13], 0x03007BD8u, "sp after table init");
  require_equal(state.stop_target, 0x0801A78Bu, "table init return target");
}

void dispatches_global_init_final_return_until_large_boot() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 25; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x08038751u, "global init return branch register");
  require_equal(state.regs[13], 0x03007BE4u, "sp after global init return");
  require_equal(state.stop_target, 0x08038751u, "large boot after global init target");
}

void dispatches_large_boot_after_global_init_until_object_setup_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 26; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08038755u, "object setup call lr");
  require_equal(state.stop_target, 0x08015FB5u, "object setup target");
}

void dispatches_object_setup_wrapper_until_state_check_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 27; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00000000u, "state check mode");
  require_equal(state.regs[1], 0x0202D000u, "state check object base");
  require_equal(state.regs[4], 0x0202D000u, "object setup saved base");
  require_equal(state.regs[13], 0x03007BDCu, "sp after object setup push");
  require_equal(state.regs[14], 0x08015FC1u, "state check call lr");
  require_equal(state.stop_target, 0x0801AC1Du, "state check target");
}

void dispatches_object_state_check_until_wrapper_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 28; ++i) {
    aw::generated::dispatch_one(state);
  }

  for (std::uint32_t index = 0; index < 16; ++index) {
    require_equal(aw::read8(state.memory, 0x02012A68u + index), std::uint8_t{0}, "object state visible flags");
  }
  require_equal(state.regs[0], 0x00000000u, "object state check result");
  require_equal(state.regs[4], 0x0202D000u, "object setup preserved base after state check");
  require_equal(state.regs[13], 0x03007BDCu, "sp after object state check");
  require_equal(state.stop_target, 0x08015FC1u, "object setup state check return");
}

void dispatches_object_setup_continuation_until_copy_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 29; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x0202D000u, "object copy source base");
  require_equal(state.regs[14], 0x08015FCBu, "object copy call lr");
  require_equal(state.regs[13], 0x03007BDCu, "sp before object copy call");
  require_equal(state.stop_target, 0x08015D1Du, "object copy target");
}

void dispatches_object_copy_until_wrapper_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 30; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00000518u, "object copy return value");
  require_equal(state.regs[13], 0x03007BDCu, "sp after object copy");
  require_equal(state.stop_target, 0x08015FCBu, "object copy return target");
}

void dispatches_object_setup_return_until_large_boot() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 31; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x08038755u, "object setup return branch register");
  require_equal(state.regs[13], 0x03007BE4u, "sp after object setup");
  require_equal(state.stop_target, 0x08038755u, "large boot after object setup target");
}

void dispatches_large_boot_after_object_setup_until_slot_init_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 32; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08038759u, "slot init call lr");
  require_equal(state.stop_target, 0x0803F87Du, "slot init target");
}

void dispatches_slot_init_until_first_slot_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 33; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00000000u, "slot index argument");
  require_equal(state.regs[4], 0x00000000u, "slot loop index");
  require_equal(state.regs[13], 0x03007BDCu, "sp after slot init push");
  require_equal(state.regs[14], 0x0803F887u, "slot helper lr");
  require_equal(state.stop_target, 0x0803F899u, "slot helper target");
}

void dispatches_first_slot_helper_until_state_query() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 34; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00000005u, "slot state query id");
  require_equal(state.regs[4], 0x00000000u, "slot helper index");
  require_equal(state.regs[5], 0x00000005u, "slot helper state id");
  require_equal(state.regs[6], 0x0202D000u, "slot helper object base");
  require_equal(state.regs[13], 0x03007BCCu, "sp after slot helper push");
  require_equal(state.regs[14], 0x0803F8B1u, "slot state query lr");
  require_equal(state.stop_target, 0x0801AD39u, "slot state query target");
}

void dispatches_slot_state_query_until_slot_helper_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 35; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00000001u, "slot state query unavailable result");
  require_equal(state.regs[4], 0x00000000u, "slot helper index after query");
  require_equal(state.regs[5], 0x00000005u, "slot helper state id after query");
  require_equal(state.regs[6], 0x0202D000u, "slot helper object base after query");
  require_equal(state.regs[13], 0x03007BCCu, "sp after slot state query");
  require_equal(state.stop_target, 0x0803F8B1u, "slot state query return target");
}

void dispatches_first_slot_helper_disabled_return_until_slot_loop() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 36; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read8(state.memory, 0x0202D015u), std::uint8_t{0xFF}, "first slot disabled marker");
  require_equal(state.regs[0], 0x00000000u, "first slot helper result");
  require_equal(state.regs[4], 0x00000000u, "restored slot loop index");
  require_equal(state.regs[13], 0x03007BDCu, "sp after first slot helper");
  require_equal(state.stop_target, 0x0803F887u, "first slot helper return target");
}

void dispatches_slot_loop_until_second_slot_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 37; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00000001u, "second slot index argument");
  require_equal(state.regs[4], 0x00000001u, "second slot loop index");
  require_equal(state.regs[13], 0x03007BDCu, "sp before second slot helper");
  require_equal(state.regs[14], 0x0803F887u, "second slot helper lr");
  require_equal(state.stop_target, 0x0803F899u, "second slot helper target");
}

void dispatches_fourth_slot_loop_until_direct_disabled_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 50; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read8(state.memory, 0x0202D015u), std::uint8_t{0xFF}, "slot 0 disabled marker");
  require_equal(aw::read8(state.memory, 0x0202D035u), std::uint8_t{0xFF}, "slot 1 disabled marker");
  require_equal(aw::read8(state.memory, 0x0202D055u), std::uint8_t{0xFF}, "slot 2 disabled marker");
  require_equal(aw::read8(state.memory, 0x0202D075u), std::uint8_t{0xFF}, "slot 3 disabled marker");
  require_equal(aw::read8(state.memory, 0x0202D095u), std::uint8_t{0xFF}, "slot 4 disabled marker");
  require_equal(state.regs[0], 0x00000000u, "direct disabled slot result");
  require_equal(state.regs[4], 0x00000004u, "restored fourth slot loop index");
  require_equal(state.regs[13], 0x03007BDCu, "sp after fourth slot helper");
  require_equal(state.stop_target, 0x0803F887u, "fourth slot helper return target");
}

void dispatches_slot_init_until_large_boot_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 66; ++i) {
    aw::generated::dispatch_one(state);
  }

  for (std::uint32_t slot = 0; slot < 12; ++slot) {
    require_equal(aw::read8(state.memory, 0x0202D000u + slot * 0x20u + 0x15u),
                  std::uint8_t{0xFF}, "slot disabled marker");
  }
  require_equal(state.regs[0], 0x08038759u, "slot init return branch register");
  require_equal(state.regs[13], 0x03007BE4u, "sp after slot init");
  require_equal(state.stop_target, 0x08038759u, "large boot after slot init target");
}

void dispatches_large_boot_after_slot_init_until_seed_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 67; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x0A6B99CDu, "seed literal argument");
  require_equal(state.regs[14], 0x0803875Fu, "seed call lr");
  require_equal(state.stop_target, 0x08010A79u, "seed helper target");
}

void dispatches_seed_store_until_large_boot_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 68; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read32(state.memory, 0x03001D30u), 0x0A6B99CDu, "stored seed literal");
  require_equal(state.regs[1], 0x03001D30u, "seed global address");
  require_equal(state.regs[13], 0x03007BE4u, "sp after seed helper");
  require_equal(state.stop_target, 0x0803875Fu, "large boot after seed target");
}

void dispatches_large_boot_after_seed_until_display_reset_wrapper() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 69; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08038763u, "display reset wrapper lr");
  require_equal(state.stop_target, 0x08010969u, "display reset wrapper target");
}

void dispatches_display_reset_wrapper_until_register_reset_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 70; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BE0u, "sp after display reset wrapper push");
  require_equal(aw::read32(state.memory, 0x03007BE0u), 0x08038763u, "display reset pushed lr");
  require_equal(state.regs[14], 0x0801096Fu, "register reset call lr");
  require_equal(state.stop_target, 0x08010545u, "register reset target");
}

void dispatches_register_reset_until_first_nested_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 71; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read16(state.memory, 0x0300337Cu), std::uint16_t{0x0080}, "display reset flag");
  require_equal(aw::read16(state.memory, 0x03001EE8u), std::uint16_t{0x0000}, "display reset global 1");
  require_equal(aw::read16(state.memory, 0x03001D2Cu), std::uint16_t{0x0000}, "display reset global 2");
  require_equal(state.regs[13], 0x03007BDCu, "sp after register reset push");
  require_equal(aw::read32(state.memory, 0x03007BDCu), 0x0801096Fu, "register reset pushed lr");
  require_equal(state.regs[14], 0x0801055Bu, "first nested display reset lr");
  require_equal(state.stop_target, 0x08010445u, "first nested display reset target");
}

void dispatches_display_zero_helper_until_register_reset_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 71; ++i) {
    aw::generated::dispatch_one(state);
  }

  const std::uint32_t addresses[] = {
      0x03001D48u, 0x03000DBCu, 0x03002F40u, 0x030031F0u, 0x03003350u,
      0x03000DA0u, 0x03001E50u, 0x03001E44u, 0x03003374u, 0x03002F78u,
      0x03001D3Cu, 0x03003364u, 0x03002F2Cu,
  };
  for (const auto address : addresses) {
    aw::write16(state.memory, address, 0xBEEFu);
  }

  aw::generated::dispatch_one(state);

  for (const auto address : addresses) {
    require_equal(aw::read16(state.memory, address), std::uint16_t{0x0000}, "zeroed display global");
  }
  require_equal(state.regs[13], 0x03007BDCu, "sp after display zero helper");
  require_equal(state.stop_target, 0x0801055Bu, "register reset after zero helper target");
}

void dispatches_register_reset_after_zero_until_second_zero_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 73; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x0801055Fu, "second zero helper lr");
  require_equal(state.stop_target, 0x080104B1u, "second zero helper target");
}

void dispatches_display_short_zero_helper_until_register_reset_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 73; ++i) {
    aw::generated::dispatch_one(state);
  }

  const std::uint32_t addresses[] = {
      0x03003388u, 0x03001E60u, 0x03002F30u, 0x03001E40u,
  };
  for (const auto address : addresses) {
    aw::write16(state.memory, address, 0xCAFEu);
  }

  aw::generated::dispatch_one(state);

  for (const auto address : addresses) {
    require_equal(aw::read16(state.memory, address), std::uint16_t{0x0000}, "zeroed short display global");
  }
  require_equal(state.stop_target, 0x0801055Fu, "register reset after short zero helper target");
}

void dispatches_register_reset_after_second_zero_until_flag_reset_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 75; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08010563u, "flag reset helper lr");
  require_equal(state.stop_target, 0x080104D5u, "flag reset helper target");
}

void dispatches_display_flag_reset_until_register_reset_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 75; ++i) {
    aw::generated::dispatch_one(state);
  }

  aw::write8(state.memory, 0x0300337Du, 0xFFu);
  const std::uint32_t byte_addresses[] = {
      0x03002F48u, 0x03002F58u, 0x030031D0u, 0x03002F4Cu,
      0x03002F74u, 0x03002314u, 0x03002F3Cu, 0x03001EECu,
  };
  const std::uint32_t halfword_addresses[] = {
      0x03003354u, 0x03003384u,
  };
  for (const auto address : byte_addresses) {
    aw::write8(state.memory, address, 0xA5u);
  }
  for (const auto address : halfword_addresses) {
    aw::write16(state.memory, address, 0xCAFEu);
  }

  aw::generated::dispatch_one(state);

  require_equal(aw::read8(state.memory, 0x0300337Du), std::uint8_t{0x1F}, "masked display flag byte");
  for (const auto address : byte_addresses) {
    require_equal(aw::read8(state.memory, address), std::uint8_t{0x00}, "zeroed flag byte global");
  }
  for (const auto address : halfword_addresses) {
    require_equal(aw::read16(state.memory, address), std::uint16_t{0x0000}, "zeroed flag halfword global");
  }
  require_equal(state.stop_target, 0x08010563u, "register reset after flag helper target");
}

void dispatches_register_reset_return_until_wrapper_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 77; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x0801096Fu, "register reset return address");
  require_equal(state.regs[13], 0x03007BE0u, "sp after register reset return");
  require_equal(state.stop_target, 0x0801096Fu, "display reset wrapper return target");
}

void dispatches_display_reset_wrapper_return_until_large_boot() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 78; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x08038763u, "display reset wrapper return address");
  require_equal(state.regs[13], 0x03007BE4u, "sp after display reset wrapper return");
  require_equal(state.stop_target, 0x08038763u, "large boot after display reset target");
}

void dispatches_large_boot_after_display_reset_until_input_reset_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 79; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08038767u, "input reset call lr");
  require_equal(state.stop_target, 0x08010975u, "input reset target");
}

void dispatches_input_reset_wrapper_until_register_sync_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 80; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BE0u, "sp after input reset wrapper push");
  require_equal(aw::read32(state.memory, 0x03007BE0u), 0x08038767u, "input reset pushed lr");
  require_equal(state.regs[14], 0x0801097Bu, "register sync call lr");
  require_equal(state.stop_target, 0x08010575u, "register sync target");
}

void dispatches_register_sync_until_input_reset_wrapper_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 80; ++i) {
    aw::generated::dispatch_one(state);
  }

  const std::pair<std::uint32_t, std::uint32_t> halfword_copies[] = {
      {0x0300337Cu, 0x04000000u}, {0x03001EE8u, 0x04000004u},
      {0x03001D48u, 0x04000010u}, {0x03000DBCu, 0x04000012u},
      {0x03002F40u, 0x04000014u}, {0x030031F0u, 0x04000016u},
      {0x03003350u, 0x04000018u}, {0x03000DA0u, 0x0400001Au},
      {0x03001E50u, 0x0400001Cu}, {0x03001E44u, 0x0400001Eu},
      {0x03003374u, 0x0400004Cu}, {0x03002F78u, 0x04000008u},
      {0x03001D3Cu, 0x0400000Au}, {0x03003364u, 0x0400000Cu},
      {0x03002F2Cu, 0x0400000Eu}, {0x03003388u, 0x04000050u},
      {0x03002F30u, 0x04000054u},
  };
  std::uint16_t value = 0x1100u;
  for (const auto& copy : halfword_copies) {
    aw::write16(state.memory, copy.first, value++);
  }
  aw::write16(state.memory, 0x03001E60u, 0x0034u);
  aw::write16(state.memory, 0x03001E40u, 0x0012u);

  for (std::uint32_t i = 0; i < 4; ++i) {
    aw::write32(state.memory, 0x03002300u + i * 4u, 0xA0000000u + i);
    aw::write32(state.memory, 0x030032D0u + i * 4u, 0xB0000000u + i);
  }

  aw::generated::dispatch_one(state);

  for (const auto& copy : halfword_copies) {
    require_equal(aw::read16(state.memory, copy.second), aw::read16(state.memory, copy.first),
                  "synced IO halfword");
  }
  require_equal(aw::read16(state.memory, 0x04000052u), std::uint16_t{0x1234}, "combined blend alpha");
  for (std::uint32_t i = 0; i < 4; ++i) {
    require_equal(aw::read32(state.memory, 0x04000020u + i * 4u), 0xA0000000u + i,
                  "synced first IO word group");
    require_equal(aw::read32(state.memory, 0x04000030u + i * 4u), 0xB0000000u + i,
                  "synced second IO word group");
  }
  require_equal(state.stop_target, 0x0801097Bu, "input reset after register sync target");
}

void dispatches_input_reset_after_register_sync_until_second_sync_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 82; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x0801097Fu, "second register sync call lr");
  require_equal(state.stop_target, 0x080106A5u, "second register sync target");
}

void dispatches_second_register_sync_until_input_reset_wrapper_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 82; ++i) {
    aw::generated::dispatch_one(state);
  }

  const std::pair<std::uint32_t, std::uint32_t> first_halfword_copies[] = {
      {0x03003234u, 0x0300337Cu}, {0x03001D28u, 0x03001EE8u},
      {0x030022F0u, 0x03001D48u}, {0x03002318u, 0x03000DBCu},
      {0x03002F34u, 0x03002F40u}, {0x0300336Cu, 0x030031F0u},
      {0x03003340u, 0x03003350u}, {0x03000DB8u, 0x03000DA0u},
      {0x030031E8u, 0x03001E50u}, {0x03002F28u, 0x03001E44u},
      {0x030031ECu, 0x03003374u}, {0x030032F0u, 0x03003354u},
      {0x03001E4Cu, 0x03003384u},
  };
  const std::pair<std::uint32_t, std::uint32_t> byte_copies[] = {
      {0x03001D34u, 0x03002F48u}, {0x03002F7Cu, 0x030031D0u},
      {0x030031BCu, 0x03002F58u}, {0x03003230u, 0x03002F4Cu},
      {0x0300335Cu, 0x03002F74u}, {0x030031C0u, 0x03002F3Cu},
      {0x03001ED4u, 0x03002314u}, {0x03003344u, 0x03001EECu},
  };
  const std::pair<std::uint32_t, std::uint32_t> second_halfword_copies[] = {
      {0x030031C8u, 0x03002F78u}, {0x03002F64u, 0x03001D3Cu},
      {0x03003348u, 0x03003364u}, {0x0300338Cu, 0x03002F2Cu},
      {0x03001E54u, 0x03003388u}, {0x03003368u, 0x03001E60u},
      {0x03001D40u, 0x03002F30u}, {0x03001D10u, 0x03001E40u},
      {0x03000DB0u, 0x03002F50u}, {0x03001D20u, 0x03001D1Cu},
      {0x030032E4u, 0x03003380u}, {0x03000DC4u, 0x030032E8u},
      {0x03003238u, 0x03001D24u}, {0x03001E58u, 0x03001E6Cu},
      {0x030031B0u, 0x03001E48u}, {0x03000DACu, 0x03001EE4u},
      {0x03001D4Cu, 0x0300334Cu}, {0x03002F54u, 0x03002F6Cu},
      {0x03003370u, 0x03001E64u}, {0x03002F5Cu, 0x03001D14u},
      {0x03003228u, 0x030022F4u}, {0x03001E5Cu, 0x030022FCu},
      {0x03002F70u, 0x03000DB4u},
  };

  std::uint16_t halfword_value = 0x2100u;
  for (const auto& copy : first_halfword_copies) {
    aw::write16(state.memory, copy.second, halfword_value++);
    aw::write16(state.memory, copy.first, 0);
  }
  for (const auto& copy : second_halfword_copies) {
    aw::write16(state.memory, copy.second, halfword_value++);
    aw::write16(state.memory, copy.first, 0);
  }
  std::uint8_t byte_value = 0x40u;
  for (const auto& copy : byte_copies) {
    aw::write8(state.memory, copy.second, byte_value++);
    aw::write8(state.memory, copy.first, 0);
  }

  aw::generated::dispatch_one(state);

  for (const auto& copy : first_halfword_copies) {
    require_equal(aw::read16(state.memory, copy.first), aw::read16(state.memory, copy.second),
                  "synced first shadow halfword");
  }
  for (const auto& copy : byte_copies) {
    require_equal(aw::read8(state.memory, copy.first), aw::read8(state.memory, copy.second),
                  "synced shadow byte");
  }
  for (const auto& copy : second_halfword_copies) {
    require_equal(aw::read16(state.memory, copy.first), aw::read16(state.memory, copy.second),
                  "synced second shadow halfword");
  }
  require_equal(state.stop_target, 0x0801097Fu, "input reset after second register sync target");
}

void dispatches_input_reset_tail_until_large_boot_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 83; ++i) {
    aw::generated::dispatch_one(state);
  }

  const std::pair<std::uint32_t, std::uint32_t> byte_copies[] = {
      {0x04000040u, 0x03002F58u}, {0x04000041u, 0x03002F48u},
      {0x04000044u, 0x03002F4Cu}, {0x04000045u, 0x030031D0u},
      {0x04000042u, 0x03002314u}, {0x04000043u, 0x03002F74u},
      {0x04000046u, 0x03001EECu}, {0x04000047u, 0x03002F3Cu},
  };
  const std::pair<std::uint32_t, std::uint32_t> halfword_copies[] = {
      {0x04000048u, 0x03003354u}, {0x0400004Au, 0x03003384u},
  };
  std::uint8_t byte_value = 0x50u;
  for (const auto& copy : byte_copies) {
    aw::write8(state.memory, copy.second, byte_value++);
    aw::write8(state.memory, copy.first, 0);
  }
  std::uint16_t halfword_value = 0x3300u;
  for (const auto& copy : halfword_copies) {
    aw::write16(state.memory, copy.second, halfword_value++);
    aw::write16(state.memory, copy.first, 0);
  }

  aw::generated::dispatch_one(state);

  for (const auto& copy : byte_copies) {
    require_equal(aw::read8(state.memory, copy.first), aw::read8(state.memory, copy.second),
                  "synced input tail byte");
  }
  for (const auto& copy : halfword_copies) {
    require_equal(aw::read16(state.memory, copy.first), aw::read16(state.memory, copy.second),
                  "synced input tail halfword");
  }
  require_equal(state.regs[0], 0x08038767u, "input reset wrapper return address");
  require_equal(state.regs[13], 0x03007BE4u, "sp after input reset wrapper return");
  require_equal(state.stop_target, 0x08038767u, "large boot after input reset target");
}

void dispatches_large_boot_after_input_reset_until_callback_register_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 85; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00000000u, "callback slot");
  require_equal(state.regs[1], 0x08038291u, "callback target");
  require_equal(state.regs[14], 0x0803876Fu, "callback register lr");
  require_equal(state.stop_target, 0x0807AE51u, "callback register target");
}

void dispatches_callback_register_until_large_boot_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 86; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read32(state.memory, 0x03006630u), 0x08038291u, "registered callback slot zero");
  require_equal(state.stop_target, 0x0803876Fu, "large boot after callback register target");
}

void dispatches_large_boot_default_state_writes_until_save_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 87; ++i) {
    aw::generated::dispatch_one(state);
  }

  constexpr std::uint32_t state_base = 0x0201AB34u;
  require_equal(aw::read8(state.memory, state_base + 0x85u), std::uint8_t{0x00},
                "boot state byte zero");
  require_equal(aw::read8(state.memory, state_base + 0xEDu), std::uint8_t{0x03},
                "boot state byte three");
  require_equal(aw::read8(state.memory, state_base + 0x155u), std::uint8_t{0x08},
                "boot state byte eight");
  require_equal(aw::read8(state.memory, state_base + 0x1BDu), std::uint8_t{0x06},
                "boot state byte six");
  require_equal(state.stop_target, 0x080387FDu, "large boot default save target");
}

void dispatches_large_boot_save_path_until_refresh_wrapper_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 88; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08038801u, "refresh wrapper lr");
  require_equal(state.stop_target, 0x080386B5u, "refresh wrapper target");
}

void dispatches_refresh_wrapper_until_system_init_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 89; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BE0u, "refresh wrapper stack frame");
  require_equal(aw::read32(state.memory, 0x03007BE0u), 0x08038801u, "refresh wrapper pushed lr");
  require_equal(state.regs[14], 0x080386BBu, "system init lr");
  require_equal(state.stop_target, 0x080385CDu, "system init target");
}

void dispatches_system_init_until_engine_clear_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 90; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BD8u, "system init stack frame");
  require_equal(aw::read32(state.memory, 0x03007BD8u), 0x00000000u, "system init pushed r4");
  require_equal(aw::read32(state.memory, 0x03007BDCu), 0x080386BBu, "system init pushed lr");
  require_equal(aw::read32(state.memory, 0x03004440u), 0x00000000u, "system init global zero");
  require_equal(state.regs[4], 0x00000000u, "system init r4 zero");
  require_equal(state.regs[14], 0x080385D9u, "engine clear lr");
  require_equal(state.stop_target, 0x0800F1B1u, "engine clear target");
}

void dispatches_engine_clear_until_register_reset_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 91; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BD4u, "engine clear stack frame");
  require_equal(aw::read32(state.memory, 0x03007BD4u), 0x080385D9u, "engine clear pushed lr");
  require_equal(state.regs[14], 0x0800F1B7u, "engine register reset lr");
  require_equal(state.stop_target, 0x0800F171u, "engine register reset target");
}

void dispatches_engine_register_reset_until_engine_clear_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 92; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read16(state.memory, 0x03003388u), std::uint16_t{0x003F},
                "sound control shadow reset flags");
  require_equal(aw::read16(state.memory, 0x03001E60u), std::uint16_t{0x0000},
                "dma control shadow reset");
  require_equal(aw::read16(state.memory, 0x03002F30u), std::uint16_t{0x0000},
                "sound bias shadow reset");
  require_equal(aw::read16(state.memory, 0x03001E40u), std::uint16_t{0x0000},
                "dma source shadow reset");
  require_equal(state.stop_target, 0x0800F1B7u, "engine clear continuation target");
}

void dispatches_engine_clear_continuation_until_display_reset_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 93; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read16(state.memory, 0x03003388u), std::uint16_t{0x00FF},
                "sound control shadow enabled flags");
  require_equal(aw::read16(state.memory, 0x03001E40u), std::uint16_t{0x001F},
                "dma control shadow enabled");
  require_equal(state.regs[14], 0x0800F1CBu, "engine clear display reset lr");
  require_equal(state.stop_target, 0x08010975u, "engine clear display reset target");
}

void dispatches_engine_clear_return_until_system_init_continuation() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 99; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BD8u, "engine clear stack released");
  require_equal(state.regs[0], 0x080385D9u, "engine clear popped return");
  require_equal(state.stop_target, 0x080385D9u, "system init continuation target");
}

void dispatches_system_init_continuation_until_first_state_store_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 100; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00000000u, "first state store value");
  require_equal(state.regs[14], 0x080385DFu, "first state store lr");
  require_equal(state.stop_target, 0x08038261u, "first state store target");
}

void dispatches_first_state_store_until_second_state_store_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 102; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read32(state.memory, 0x03004478u), 0x00000000u, "first state store global");
  require_equal(state.regs[0], 0x00000000u, "second state store value");
  require_equal(state.regs[14], 0x080385E5u, "second state store lr");
  require_equal(state.stop_target, 0x0803826Du, "second state store target");
}

void dispatches_second_state_store_until_local_reset_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 104; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read32(state.memory, 0x03004460u), 0x00000000u, "second state store global");
  require_equal(aw::read8(state.memory, 0x03004434u), std::uint8_t{0x00}, "system init byte clear");
  require_equal(aw::read32(state.memory, 0x03004358u), 0x00000000u, "system init word clear 1");
  require_equal(aw::read32(state.memory, 0x030035E0u), 0x00000000u, "system init word clear 2");
  require_equal(aw::read32(state.memory, 0x030043D4u), 0x00000000u, "system init word clear 3");
  require_equal(aw::read32(state.memory, 0x030047E0u), 0x00000000u, "system init word clear 4");
  require_equal(aw::read16(state.memory, 0x030036B0u), std::uint16_t{0x0000},
                "system init halfword clear");
  require_equal(state.regs[14], 0x08038601u, "local reset lr");
  require_equal(state.stop_target, 0x080385BDu, "local reset target");
}

void dispatches_local_reset_until_system_init_dma_copy_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 105; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read16(state.memory, 0x030047E4u), std::uint16_t{0x0000},
                "local reset halfword");
  require_equal(state.stop_target, 0x08038601u, "system init dma copy target");
}

void dispatches_system_init_dma_copy_until_dma_setup_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 106; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08038605u, "dma setup lr");
  require_equal(state.stop_target, 0x0801B821u, "dma setup target");
}

void dispatches_dma_setup_until_bios_cpuset_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 107; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BD4u, "dma setup stack frame");
  require_equal(aw::read32(state.memory, 0x03007BD4u), 0x08038605u, "dma setup pushed lr");
  require_equal(state.regs[0], 0x083F752Cu, "bios cpuset source");
  require_equal(state.regs[1], 0x0300666Cu, "bios cpuset destination");
  require_equal(state.regs[2], 0x06000212u, "bios cpuset control");
  require_equal(state.regs[14], 0x0801B841u, "bios cpuset lr");
  require_equal(state.stop_target, 0x080796C5u, "bios cpuset target");
}

void dispatches_bios_cpuset_until_dma_setup_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 108; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read32(state.memory, 0x0300666Cu), 0x464FB5F0u, "cpuset first word");
  require_equal(aw::read32(state.memory, 0x03006670u), 0xB4C04646u, "cpuset second word");
  require_equal(aw::read32(state.memory, 0x03006EB0u), 0x03000154u, "cpuset last word");
  require_equal(state.stop_target, 0x0801B841u, "dma setup return target");
}

void dispatches_dma_setup_return_until_next_system_init_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 109; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BD8u, "dma setup stack released");
  require_equal(state.regs[0], 0x08038605u, "dma setup popped return");
  require_equal(state.stop_target, 0x08038605u, "next system init call target");
}

void dispatches_next_system_init_call_until_next_initializer() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 110; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08038609u, "next initializer lr");
  require_equal(state.stop_target, 0x08034C99u, "next initializer target");
}

void dispatches_next_initializer_until_byte_reset_helper_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 111; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BD4u, "next initializer stack frame");
  require_equal(aw::read32(state.memory, 0x03007BD4u), 0x08038609u, "next initializer pushed lr");
  require_equal(aw::read32(state.memory, 0x030035F0u), 0x02015340u, "first pointer snapshot");
  require_equal(aw::read32(state.memory, 0x030041D8u), 0x02015340u, "second pointer snapshot");
  require_equal(state.regs[0], 0x00000000u, "byte reset argument");
  require_equal(state.regs[14], 0x08034CADu, "byte reset helper lr");
  require_equal(state.stop_target, 0x08018AADu, "byte reset helper target");
}

void dispatches_byte_reset_helper_until_next_initializer_continuation() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 112; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read8(state.memory, 0x03004280u), std::uint8_t{0xFF}, "byte reset sentinel");
  require_equal(aw::read8(state.memory, 0x03003580u), std::uint8_t{0x00}, "byte reset value");
  require_equal(state.stop_target, 0x08034CADu, "next initializer continuation target");
}

void dispatches_next_initializer_continuation_until_table_copy_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 113; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08034CB1u, "table copy lr");
  require_equal(state.stop_target, 0x0801F115u, "table copy target");
}

void dispatches_table_copy_until_next_initializer_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 114; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read32(state.memory, 0x0201AD3Cu), 0x01030100u, "table copy first word");
  require_equal(aw::read32(state.memory, 0x0201AD40u), 0x04040404u, "table copy second word");
  require_equal(aw::read8(state.memory, 0x0201B13Bu), std::uint8_t{0x00}, "table copy last byte");
  require_equal(state.regs[13], 0x03007BD4u, "table copy stack restored");
  require_equal(state.stop_target, 0x08034CB1u, "next initializer return target");
}

void dispatches_next_initializer_return_until_system_init_sequence() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 115; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BD8u, "next initializer wrapper stack released");
  require_equal(state.regs[0], 0x08038609u, "next initializer popped return");
  require_equal(state.stop_target, 0x08038609u, "system init sequence target");
}

void dispatches_system_init_sequence_until_mode_initializer_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 116; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x0803860Du, "mode initializer lr");
  require_equal(state.stop_target, 0x08034C75u, "mode initializer target");
}

void dispatches_mode_initializer_until_defaults_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 117; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BCCu, "mode initializer stack frame");
  require_equal(aw::read32(state.memory, 0x03007BD4u), 0x0803860Du, "mode initializer pushed lr");
  require_equal(aw::read8(state.memory, 0x0300431Cu), std::uint8_t{0x01}, "mode initializer active flag");
  require_equal(aw::read8(state.memory, 0x03004311u), std::uint8_t{0x03}, "mode initializer mode");
  require_equal(aw::read8(state.memory, 0x03004312u), std::uint8_t{0x01}, "mode initializer submode");
  require_equal(state.regs[14], 0x08034C87u, "defaults initializer lr");
  require_equal(state.stop_target, 0x08034BA9u, "defaults initializer target");
}

void dispatches_defaults_initializer_until_order_helper_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 118; ++i) {
    aw::generated::dispatch_one(state);
  }

  constexpr std::uint32_t base = 0x03004310u;
  require_equal(state.regs[13], 0x03007BB8u, "defaults initializer stack frame");
  require_equal(aw::read32(state.memory, 0x03007BC8u), 0x08034C87u, "defaults initializer pushed lr");
  require_equal(aw::read32(state.memory, 0x0300447Cu), 0x00000000u, "defaults first global");
  require_equal(aw::read32(state.memory, 0x03004890u), 0x00000000u, "defaults second global");
  require_equal(aw::read8(state.memory, base + 0x34u), std::uint8_t{0x01}, "defaults byte 34");
  require_equal(aw::read8(state.memory, base + 0x35u), std::uint8_t{0x02}, "defaults byte 35");
  require_equal(aw::read8(state.memory, base + 0x36u), std::uint8_t{0x03}, "defaults byte 36");
  require_equal(aw::read8(state.memory, base + 0x37u), std::uint8_t{0x04}, "defaults byte 37");
  require_equal(aw::read8(state.memory, base + 0x39u), std::uint8_t{0x01}, "defaults byte 39");
  require_equal(aw::read8(state.memory, base + 0x3Cu), std::uint8_t{0x01}, "defaults byte 3c");
  require_equal(aw::read8(state.memory, base + 0x3Eu), std::uint8_t{0x01}, "defaults byte 3e");
  require_equal(aw::read8(state.memory, base + 0x3Fu), std::uint8_t{0x02}, "defaults byte 3f");
  require_equal(aw::read8(state.memory, base + 0x40u), std::uint8_t{0x04}, "defaults byte 40");
  require_equal(aw::read8(state.memory, base + 0x41u), std::uint8_t{0x03}, "defaults byte 41");
  require_equal(state.regs[14], 0x08034BFBu, "order helper lr");
  require_equal(state.stop_target, 0x08024EC5u, "order helper target");
}

void dispatches_order_helper_until_defaults_continuation() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 119; ++i) {
    aw::generated::dispatch_one(state);
  }

  constexpr std::uint32_t base = 0x03004310u;
  require_equal(aw::read8(state.memory, base + 0x43u), std::uint8_t{0x00}, "order helper byte 43");
  require_equal(aw::read8(state.memory, base + 0x44u), std::uint8_t{0x01}, "order helper byte 44");
  require_equal(aw::read8(state.memory, base + 0x45u), std::uint8_t{0x02}, "order helper byte 45");
  require_equal(aw::read8(state.memory, base + 0x46u), std::uint8_t{0x03}, "order helper byte 46");
  require_equal(state.stop_target, 0x08034BFBu, "defaults continuation target");
}

void dispatches_defaults_continuation_until_mode_initializer_continuation() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 120; ++i) {
    aw::generated::dispatch_one(state);
  }

  constexpr std::uint32_t base = 0x03004310u;
  require_equal(aw::read8(state.memory, base + 0x30u), std::uint8_t{0x00}, "defaults byte 30");
  require_equal(aw::read8(state.memory, base + 0x31u), std::uint8_t{0x00}, "defaults byte 31");
  require_equal(aw::read32(state.memory, base + 0x28u), 0x000003E8u, "defaults timer");
  require_equal(aw::read32(state.memory, base + 0x14u), 0x00000000u, "defaults word 14");
  require_equal(aw::read8(state.memory, base + 0x04u), std::uint8_t{0x03}, "defaults final byte 04");
  require_equal(aw::read8(state.memory, base + 0x05u), std::uint8_t{0x01}, "defaults final byte 05");
  require_equal(aw::read8(state.memory, base + 0x07u), std::uint8_t{0x01}, "defaults branch byte 07");
  require_equal(state.regs[13], 0x03007BCCu, "defaults stack restored");
  require_equal(state.stop_target, 0x08034C87u, "mode initializer continuation target");
}

void dispatches_mode_initializer_continuation_until_clear_helper_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 121; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08034C8Bu, "mode clear helper lr");
  require_equal(state.stop_target, 0x08034C65u, "mode clear helper target");
}

void dispatches_mode_clear_helper_until_mode_initializer_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 122; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read8(state.memory, 0x03004342u), std::uint8_t{0x00}, "mode clear helper byte");
  require_equal(state.stop_target, 0x08034C8Bu, "mode initializer return target");
}

void dispatches_mode_initializer_return_until_system_init_next_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 123; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read8(state.memory, 0x03004319u), std::uint8_t{0x01}, "mode initializer final byte");
  require_equal(state.regs[13], 0x03007BD8u, "mode initializer stack restored");
  require_equal(state.regs[0], 0x0803860Du, "mode initializer popped return");
  require_equal(state.stop_target, 0x0803860Du, "system init next call target");
}

void dispatches_system_init_next_call_until_unit_initializer() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 124; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[14], 0x08038611u, "unit initializer lr");
  require_equal(state.stop_target, 0x080149E1u, "unit initializer target");
}

void dispatches_unit_initializer_until_global_table_reset_call() {
  aw::CpuState state;
  state.trace_enabled = true;
  aw::write32(state.memory, 0x03002F20u, 0xDEADBEEFu);

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 125; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BC8u, "unit initializer stack frame");
  require_equal(aw::read32(state.memory, 0x03007BD4u), 0x08038611u, "unit initializer pushed lr");
  require_equal(aw::read32(state.memory, 0x03002F20u), 0x00000000u, "unit initializer cleared global");
  require_equal(state.regs[14], 0x080149EDu, "global table reset lr");
  require_equal(state.stop_target, 0x0801A69Du, "global table reset target");
}

void dispatches_global_table_reset_until_unit_initializer_continuation() {
  aw::CpuState state;
  state.trace_enabled = true;
  aw::write32(state.memory, 0x02012448u, 0xDEADBEEFu);
  aw::write32(state.memory, 0x02012A48u, 0xCAFEBABEu);
  aw::write32(state.memory, 0x03001EDCu, 0xFEEDFACEu);
  aw::write16(state.memory, 0x03001ED8u, 0xBEEFu);

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 126; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read32(state.memory, 0x02012448u), 0x00000000u, "global table reset first entry");
  require_equal(aw::read32(state.memory, 0x02012A48u), 0x00000000u, "global table reset last entry");
  require_equal(aw::read32(state.memory, 0x03001EDCu), 0x00000000u, "global table reset word");
  require_equal(aw::read16(state.memory, 0x03001ED8u), static_cast<std::uint16_t>(0), "global table reset halfword");
  require_equal(state.regs[13], 0x03007BC8u, "global table reset stack restored");
  require_equal(state.stop_target, 0x080149EDu, "unit initializer continuation target");
}

void dispatches_unit_initializer_continuation_until_first_entry_setup() {
  aw::CpuState state;
  state.trace_enabled = true;
  aw::write16(state.memory, 0x03002F60u, 0xBEEFu);
  aw::write16(state.memory, 0x03001D38u, 0xFACEu);
  aw::write8(state.memory, 0x03002F00u, std::uint8_t{0x7F});

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 127; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(aw::read16(state.memory, 0x03002F60u), static_cast<std::uint16_t>(0x0010), "unit init count");
  require_equal(aw::read16(state.memory, 0x03001D38u), static_cast<std::uint16_t>(0), "unit init selected index");
  require_equal(aw::read8(state.memory, 0x03002F00u), std::uint8_t{0x00}, "unit init first active byte");
  require_equal(state.regs[0], 0x00000000u, "first entry index");
  require_equal(state.regs[1], 0x00000100u, "first entry x");
  require_equal(state.regs[2], 0x00000100u, "first entry y");
  require_equal(state.regs[3], 0x00000000u, "first entry kind");
  require_equal(state.regs[14], 0x08014A11u, "first entry setup lr");
  require_equal(state.stop_target, 0x08014C99u, "first entry setup target");
}

void dispatches_first_entry_setup_until_geometry_helper() {
  aw::CpuState state;
  state.trace_enabled = true;
  aw::write16(state.memory, 0x02011D24u, 0xBEEFu);
  aw::write16(state.memory, 0x02011D26u, 0xFACEu);
  aw::write16(state.memory, 0x02011D28u, 0xCAFEu);

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 128; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BBCu, "first entry setup stack frame");
  require_equal(aw::read32(state.memory, 0x03007BC4u), 0x08014A11u, "first entry setup pushed lr");
  require_equal(aw::read16(state.memory, 0x02011D24u), static_cast<std::uint16_t>(0x0100), "first entry stored x");
  require_equal(aw::read16(state.memory, 0x02011D26u), static_cast<std::uint16_t>(0x0100), "first entry stored y");
  require_equal(aw::read16(state.memory, 0x02011D28u), static_cast<std::uint16_t>(0), "first entry stored kind");
  require_equal(state.regs[14], 0x08014CABu, "geometry helper lr");
  require_equal(state.stop_target, 0x08014BF9u, "geometry helper target");
}

void dispatches_geometry_helper_until_first_math_call() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 129; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BA0u, "geometry helper stack frame");
  require_equal(aw::read32(state.memory, 0x03007BB8u), 0x08014CABu, "geometry helper pushed lr");
  require_equal(state.regs[4], 0x02011D24u, "geometry helper entry pointer");
  require_equal(state.regs[9], 0x00000000u, "geometry helper sb index");
  require_equal(state.regs[0], 0x00000000u, "first math argument");
  require_equal(state.regs[14], 0x08014C15u, "first math lr");
  require_equal(state.stop_target, 0x0807AF31u, "first math target");
}

void dispatches_first_math_wrapper_until_trig_lookup() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 130; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007B9Cu, "math wrapper stack frame");
  require_equal(aw::read32(state.memory, 0x03007B9Cu), 0x08014C15u, "math wrapper pushed lr");
  require_equal(state.regs[0], 0x0000005Au, "trig lookup angle");
  require_equal(state.regs[14], 0x0807AF39u, "trig lookup lr");
  require_equal(state.stop_target, 0x0807AED5u, "trig lookup target");
}

void dispatches_trig_lookup_until_first_math_return() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 131; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00004000u, "sin 90 fixed point");
  require_equal(state.stop_target, 0x0807AF39u, "first math return target");
}

void dispatches_first_math_return_until_geometry_continuation() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 132; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BA0u, "math wrapper stack restored");
  require_equal(state.regs[0], 0x00004000u, "math wrapper result");
  require_equal(state.stop_target, 0x08014C15u, "geometry continuation target");
}

void dispatches_geometry_continuation_until_signed_divide() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 133; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BA0u, "geometry continuation stack");
  require_equal(state.regs[0], 0x00010000u, "scaled first trig result");
  require_equal(state.regs[1], 0x00000100u, "first divide denominator");
  require_equal(state.regs[2], 0x00000000u, "first entry x offset");
  require_equal(state.regs[14], 0x08014C21u, "first divide lr");
  require_equal(state.stop_target, 0x0807B489u, "signed divide target");
}

void dispatches_signed_divide_until_geometry_after_first_divide() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 134; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[13], 0x03007BA0u, "signed divide stack restored");
  require_equal(state.regs[0], 0x00000100u, "first signed divide quotient");
  require_equal(state.stop_target, 0x08014C21u, "geometry after first divide target");
}

void dispatches_geometry_after_first_divide_until_second_trig_lookup() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 135; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[8], 0x00000100u, "first geometry component");
  require_equal(state.regs[0], 0x00000000u, "second trig angle");
  require_equal(state.regs[14], 0x08014C31u, "second trig lr");
  require_equal(state.stop_target, 0x0807AED5u, "second trig lookup target");
}

void dispatches_second_trig_lookup_until_geometry_continuation() {
  aw::CpuState state;
  state.trace_enabled = true;

  aw::generated::block_080000C0(state);
  for (int i = 0; i < 136; ++i) {
    aw::generated::dispatch_one(state);
  }

  require_equal(state.regs[0], 0x00000000u, "sin zero fixed point");
  require_equal(state.stop_target, 0x08014C31u, "geometry second trig continuation target");
}

}  // namespace

int main() {
  try {
    executes_entry_block_until_thumb_handoff();
    dispatches_thumb_handoff_until_first_call();
    dispatches_called_thumb_helper_until_first_call();
    dispatches_hardware_state_helper_until_return();
    dispatches_helper_continuation_until_next_call();
    dispatches_next_helper_until_copy_call();
    dispatches_structure_init_until_return();
    dispatches_copy_helper_return();
    dispatches_outer_helper_return();
    dispatches_entry_second_call_site();
    dispatches_large_boot_routine_until_first_nested_call();
    dispatches_display_setup_wrapper_until_return();
    dispatches_display_setup_continuation_until_data_init_call();
    dispatches_dma_helper_until_return();
    dispatches_second_display_dma_setup_until_data_init_call();
    dispatches_display_setup_tail_until_large_boot_return();
    dispatches_large_boot_after_display_until_allocator_call();
    dispatches_allocator_init_until_large_boot_return();
    dispatches_large_boot_allocator_result_until_init_call();
    dispatches_global_init_until_first_nested_call();
    dispatches_save_probe_until_global_init_return();
    dispatches_global_init_after_save_probe_until_second_nested_call();
    dispatches_table_init_until_global_init_return();
    dispatches_global_init_final_return_until_large_boot();
    dispatches_large_boot_after_global_init_until_object_setup_call();
    dispatches_object_setup_wrapper_until_state_check_call();
    dispatches_object_state_check_until_wrapper_return();
    dispatches_object_setup_continuation_until_copy_call();
    dispatches_object_copy_until_wrapper_return();
    dispatches_object_setup_return_until_large_boot();
    dispatches_large_boot_after_object_setup_until_slot_init_call();
    dispatches_slot_init_until_first_slot_call();
    dispatches_first_slot_helper_until_state_query();
    dispatches_slot_state_query_until_slot_helper_return();
    dispatches_first_slot_helper_disabled_return_until_slot_loop();
    dispatches_slot_loop_until_second_slot_call();
    dispatches_fourth_slot_loop_until_direct_disabled_return();
    dispatches_slot_init_until_large_boot_return();
    dispatches_large_boot_after_slot_init_until_seed_call();
    dispatches_seed_store_until_large_boot_return();
    dispatches_large_boot_after_seed_until_display_reset_wrapper();
    dispatches_display_reset_wrapper_until_register_reset_call();
    dispatches_register_reset_until_first_nested_call();
    dispatches_display_zero_helper_until_register_reset_return();
    dispatches_register_reset_after_zero_until_second_zero_call();
    dispatches_display_short_zero_helper_until_register_reset_return();
    dispatches_register_reset_after_second_zero_until_flag_reset_call();
    dispatches_display_flag_reset_until_register_reset_return();
    dispatches_register_reset_return_until_wrapper_return();
    dispatches_display_reset_wrapper_return_until_large_boot();
    dispatches_large_boot_after_display_reset_until_input_reset_call();
    dispatches_input_reset_wrapper_until_register_sync_call();
    dispatches_register_sync_until_input_reset_wrapper_return();
    dispatches_input_reset_after_register_sync_until_second_sync_call();
    dispatches_second_register_sync_until_input_reset_wrapper_return();
    dispatches_input_reset_tail_until_large_boot_return();
    dispatches_large_boot_after_input_reset_until_callback_register_call();
    dispatches_callback_register_until_large_boot_return();
    dispatches_large_boot_default_state_writes_until_save_call();
    dispatches_large_boot_save_path_until_refresh_wrapper_call();
    dispatches_refresh_wrapper_until_system_init_call();
    dispatches_system_init_until_engine_clear_call();
    dispatches_engine_clear_until_register_reset_call();
    dispatches_engine_register_reset_until_engine_clear_return();
    dispatches_engine_clear_continuation_until_display_reset_call();
    dispatches_engine_clear_return_until_system_init_continuation();
    dispatches_system_init_continuation_until_first_state_store_call();
    dispatches_first_state_store_until_second_state_store_call();
    dispatches_second_state_store_until_local_reset_call();
    dispatches_local_reset_until_system_init_dma_copy_call();
    dispatches_system_init_dma_copy_until_dma_setup_call();
    dispatches_dma_setup_until_bios_cpuset_call();
    dispatches_bios_cpuset_until_dma_setup_return();
    dispatches_dma_setup_return_until_next_system_init_call();
    dispatches_next_system_init_call_until_next_initializer();
    dispatches_next_initializer_until_byte_reset_helper_call();
    dispatches_byte_reset_helper_until_next_initializer_continuation();
    dispatches_next_initializer_continuation_until_table_copy_call();
    dispatches_table_copy_until_next_initializer_return();
    dispatches_next_initializer_return_until_system_init_sequence();
    dispatches_system_init_sequence_until_mode_initializer_call();
    dispatches_mode_initializer_until_defaults_call();
    dispatches_defaults_initializer_until_order_helper_call();
    dispatches_order_helper_until_defaults_continuation();
    dispatches_defaults_continuation_until_mode_initializer_continuation();
    dispatches_mode_initializer_continuation_until_clear_helper_call();
    dispatches_mode_clear_helper_until_mode_initializer_return();
    dispatches_mode_initializer_return_until_system_init_next_call();
    dispatches_system_init_next_call_until_unit_initializer();
    dispatches_unit_initializer_until_global_table_reset_call();
    dispatches_global_table_reset_until_unit_initializer_continuation();
    dispatches_unit_initializer_continuation_until_first_entry_setup();
    dispatches_first_entry_setup_until_geometry_helper();
    dispatches_geometry_helper_until_first_math_call();
    dispatches_first_math_wrapper_until_trig_lookup();
    dispatches_trig_lookup_until_first_math_return();
    dispatches_first_math_return_until_geometry_continuation();
    dispatches_geometry_continuation_until_signed_divide();
    dispatches_signed_divide_until_geometry_after_first_divide();
    dispatches_geometry_after_first_divide_until_second_trig_lookup();
    dispatches_second_trig_lookup_until_geometry_continuation();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }

  return 0;
}
