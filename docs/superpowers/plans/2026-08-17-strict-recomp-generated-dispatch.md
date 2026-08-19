# Strict Recomp Generated Dispatch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Execute the first ROM-derived ARM basic block as generated native C++ and stop cleanly at the unresolved Thumb handoff target.

**Architecture:** Add a small generator that follows the reset branch from `0x08000000` to `0x080000C0`, decodes the first ARM basic block, and emits C++ into the build tree. Add a CPU state and dispatch runtime that links the generated function into `advance-wars-native.exe`, executes it, and reports the next unresolved target.

**Tech Stack:** C++20, CMake, Ninja, Python 3 from the local venv, PowerShell, CTest.

**Spec:** `docs/superpowers/specs/2026-08-17-native-gba-recomp-design.md`

## Global Constraints

- Strict recomp only: no emulator core and no playable shell shortcut.
- Target ROM is `rom/Advance Wars (USA) (Rev 1).gba`.
- Expected SHA-1 is `15053499D5B3F49128A941D7F2D84876F5424D0C`.
- Generated C++ must be deterministic and live under the build directory.
- `advance-wars-native.exe --trace` must execute generated native code and stop with `Stopped at unresolved target 0x0807AD11`.

---

### Task 1: CPU State Runtime

**Files:**
- Create: `runtime/include/aw/cpu_state.hpp`
- Create: `runtime/src/cpu_state.cpp`
- Create: `runtime/tests/cpu_state_tests.cpp`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Produces: `aw::CpuState` with `std::array<std::uint32_t, 16> regs`, `std::uint32_t cpsr`, `bool trace_enabled`, `std::vector<std::string> trace_lines`, `std::uint32_t stop_target`
- Produces: `void aw::trace(CpuState& state, std::string line)`
- Produces: `void aw::stop_at(CpuState& state, std::uint32_t target)`

### Task 2: Generated Block Script

**Files:**
- Create: `scripts/generate-blocks.py`
- Create: `runtime/tests/generated_block_tests.cpp`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Consumes: ROM path and output paths from CMake.
- Produces: `aw/generated_blocks.hpp`
- Produces: `aw_generated_blocks.cpp`
- Produces: `generated_manifest.txt`
- Produces: `void aw::generated::block_080000C0(aw::CpuState& state)`

### Task 3: Native Dispatch CLI

**Files:**
- Modify: `runtime/src/main.cpp`
- Modify: `README.md`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Consumes: `aw::generated::block_080000C0(aw::CpuState&)`
- Produces: `advance-wars-native --trace`
- Produces output containing `Executing generated block 0x080000C0` and `Stopped at unresolved target 0x0807AD11`

### Self-Review

- Spec coverage: this implements generated native code, dispatch state, and clean unresolved-target stopping.
- Placeholder scan: no `TBD`, `TODO`, or unresolved task names.
- Type consistency: `CpuState`, `trace`, `stop_at`, and `block_080000C0` signatures are consistent across tasks.
