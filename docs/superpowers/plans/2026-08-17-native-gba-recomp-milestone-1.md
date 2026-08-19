# Native GBA Recomp Milestone 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first native Windows executable around the Advance Wars Rev 1 ROM, verifying metadata and decoding the reset-entry branch before stopping cleanly.

**Architecture:** Add a small CMake C++ runtime with focused ROM-loading, metadata, and ARM decode units. The executable consumes the ROM path, validates the expected Rev 1 header/hash, decodes the reset vector branch, prints a structured summary, and exits with a non-error milestone diagnostic.

**Tech Stack:** C++20, CMake, Ninja, Windows clang++, PowerShell, CTest.

**Spec:** `docs/superpowers/specs/2026-08-17-native-gba-recomp-design.md`

## Global Constraints

- Target ROM is `rom/Advance Wars (USA) (Rev 1).gba`.
- Expected SHA-1 is `15053499D5B3F49128A941D7F2D84876F5424D0C`.
- First milestone must produce a native executable named `advance-wars-native`.
- First milestone stops after verified metadata and decoded reset-entry summary.
- Do not commit ROMs.

---

### Task 1: ROM Metadata Library

**Files:**
- Create: `runtime/include/aw/rom.hpp`
- Create: `runtime/src/rom.cpp`
- Create: `runtime/tests/rom_metadata_tests.cpp`
- Create: `CMakeLists.txt`
- Create: `runtime/CMakeLists.txt`

**Interfaces:**
- Produces: `aw::RomImage aw::load_rom_file(const std::filesystem::path& path)`
- Produces: `aw::RomHeader aw::parse_header(std::span<const std::uint8_t> bytes)`
- Produces: `std::string aw::sha1_hex(std::span<const std::uint8_t> bytes)`
- Produces: `bool aw::is_expected_advance_wars_rev1(const RomImage& rom)`

- [ ] **Step 1: Write the failing test**

```cpp
static void parses_advance_wars_rev1_header() {
  const auto rom = aw::load_rom_file(kRomPath);
  const auto header = aw::parse_header(rom.bytes);
  require_equal(header.title, "ADVANCEWARS", "title");
  require_equal(header.game_code, "AWRE", "game code");
  require_equal(header.maker_code, "01", "maker code");
  require_equal(header.version, 1, "version");
  require_equal(header.fixed_value, 0x96, "fixed value");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/native && ctest --test-dir build/native --output-on-failure`

Expected: configure or compile fails because `aw::load_rom_file` and `aw::parse_header` do not exist.

- [ ] **Step 3: Write minimal implementation**

Implement binary file loading, fixed GBA header offsets, and uppercase SHA-1 formatting in `runtime/src/rom.cpp`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/native && ctest --test-dir build/native --output-on-failure`

Expected: `rom_metadata_tests` passes.

### Task 2: Reset Entry Decoder

**Files:**
- Create: `runtime/include/aw/arm_decode.hpp`
- Create: `runtime/src/arm_decode.cpp`
- Create: `runtime/tests/arm_decode_tests.cpp`
- Modify: `runtime/CMakeLists.txt`

**Interfaces:**
- Consumes: `aw::RomImage`
- Produces: `aw::ArmBranch aw::decode_arm_branch(std::uint32_t instruction, std::uint32_t pc_address)`
- Produces: `std::uint32_t aw::read_le32(std::span<const std::uint8_t> bytes, std::size_t offset)`

- [ ] **Step 1: Write the failing test**

```cpp
static void decodes_reset_vector_to_rom_entry() {
  const auto rom = aw::load_rom_file(kRomPath);
  const auto instruction = aw::read_le32(rom.bytes, 0);
  const auto branch = aw::decode_arm_branch(instruction, 0x08000000);
  require_equal(branch.condition, 0xE, "condition");
  require_equal(branch.link, false, "link flag");
  require_equal(branch.target, 0x080000C0u, "branch target");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build/native && ctest --test-dir build/native -R arm_decode_tests --output-on-failure`

Expected: compile fails because `aw::decode_arm_branch` does not exist.

- [ ] **Step 3: Write minimal implementation**

Implement little-endian word reads and ARM B/BL immediate decoding with `target = pc_address + 8 + sign_extend(imm24 << 2)`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/native && ctest --test-dir build/native --output-on-failure`

Expected: both tests pass.

### Task 3: Native Executable

**Files:**
- Create: `runtime/src/main.cpp`
- Modify: `runtime/CMakeLists.txt`
- Create: `scripts/inspect-rom.ps1`
- Modify: `README.md`

**Interfaces:**
- Consumes: `aw::load_rom_file`, `aw::parse_header`, `aw::sha1_hex`, `aw::decode_arm_branch`
- Produces: executable target `advance-wars-native`

- [ ] **Step 1: Write the failing executable smoke test**

Add a CTest entry that runs:

```cmake
add_test(
  NAME advance_wars_native_smoke
  COMMAND advance-wars-native ${PROJECT_SOURCE_DIR}/rom/Advance\ Wars\ (USA)\ (Rev\ 1).gba
)
```

Expected output includes:

```text
Advance Wars native recomp milestone 1
ROM: ADVANCEWARS AWRE01 Rev 1
SHA1: 15053499D5B3F49128A941D7F2D84876F5424D0C
Reset branch: 0x08000000 -> 0x080000C0
Next milestone required: generated block dispatch
```

- [ ] **Step 2: Run smoke test to verify it fails**

Run: `cmake --build build/native && ctest --test-dir build/native -R advance_wars_native_smoke --output-on-failure`

Expected: compile or CTest fails because `advance-wars-native` does not exist.

- [ ] **Step 3: Write minimal implementation**

Implement CLI parsing, ROM validation, summary printing, and clean exit code `0`.

- [ ] **Step 4: Run smoke test to verify it passes**

Run: `cmake --build build/native && ctest --test-dir build/native --output-on-failure`

Expected: all tests pass, including the native exe smoke test.

### Self-Review

- Spec coverage: ROM validation, native exe, reset-entry decode, and clean stop are covered.
- Placeholder scan: no `TBD`, `TODO`, or unresolved function names.
- Type consistency: the metadata and decode interfaces are consistent across tasks.
