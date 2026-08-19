#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>

#include "aw/cpu_interpreter.hpp"
#include "aw/cpu_state.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

void tests_thumb_add_and_mov() {
  aw::CpuState state;
  state.thumb = true;
  state.regs[15] = 0x03000000;

  // MOV r0, #42 -> 0x202A
  aw::write16(state.memory, 0x03000000, 0x202A);
  // ADD r1, r0, #2 -> 0x1C81
  aw::write16(state.memory, 0x03000002, 0x1C81);

  aw::step_interpreter(state);
  require_equal(state.regs[0], 42u, "MOV r0, #42");

  aw::step_interpreter(state);
  require_equal(state.regs[1], 44u, "ADD r1, r0, #2");
}

void tests_bios_swi_cpuset() {
  aw::CpuState state;
  state.regs[0] = 0x03000000; // src
  state.regs[1] = 0x03000100; // dst
  state.regs[2] = 4 | (1u << 26); // count 4, 32-bit copy

  aw::write32(state.memory, 0x03000000, 0xDEADBEEF);
  aw::write32(state.memory, 0x03000004, 0xCAFEBABE);
  aw::write32(state.memory, 0x03000008, 0x12345678);
  aw::write32(state.memory, 0x0300000C, 0x98765432);

  aw::execute_swi(state, 0x09);

  require_equal(aw::read32(state.memory, 0x03000100), 0xDEADBEEFu, "cpuset word 0");
  require_equal(aw::read32(state.memory, 0x03000104), 0xCAFEBABEu, "cpuset word 1");
  require_equal(aw::read32(state.memory, 0x03000108), 0x12345678u, "cpuset word 2");
  require_equal(aw::read32(state.memory, 0x0300010C), 0x98765432u, "cpuset word 3");
}

void tests_bios_swi_div() {
  aw::CpuState state;
  state.regs[0] = 100; // num
  state.regs[1] = 7;   // denom

  aw::execute_swi(state, 0x06);

  require_equal(state.regs[0], 14u, "div quotient");
  require_equal(state.regs[1], 2u, "div remainder");
  require_equal(state.regs[2], 14u, "div abs quotient");
}

}  // namespace

int main() {
  try {
    tests_thumb_add_and_mov();
    tests_bios_swi_cpuset();
    tests_bios_swi_div();
    std::cout << "interpreter_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
