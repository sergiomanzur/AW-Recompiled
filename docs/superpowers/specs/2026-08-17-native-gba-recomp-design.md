# Native GBA Recomp Design

Date: 2026-08-17

## ROM

- File: `rom/Advance Wars (USA) (Rev 1).gba`
- Size: 4,194,304 bytes
- Title: `ADVANCEWARS`
- Game code: `AWRE`
- Maker: `01`
- Revision: `1`
- SHA-1: `15053499D5B3F49128A941D7F2D84876F5424D0C`
- MD5: `04775A93461D24CF1A7E3346D244E516`
- SHA-256: `4DD4BD22441F29B22CA5AF554F30BF0EB7D2B1A5DAFF0E2CD071A43E11383305`

## Goal

Build a native Windows executable that runs Advance Wars from translated native code, with a GBA hardware runtime underneath it. The exe should become the permanent host for future feature work.

## Non-Goals

- The deliverable is not a normal GBA ROM rebuild.
- The deliverable is not an emulator frontend as the final architecture.
- The first implementation pass does not need to reproduce every subsystem perfectly.

## Recommended Architecture

Use a hybrid static recompilation runtime:

1. ROM analysis pipeline
   - Validate the ROM hash and header.
   - Discover ARM and Thumb entry points.
   - Emit a manifest of known code ranges, data ranges, interrupt vectors, and unresolved branches.

2. Native generated-code project
   - Generate C++ translation units from decoded ARM/Thumb basic blocks.
   - Compile those generated blocks into a Windows executable.
   - Dispatch between blocks through a runtime jump table.

3. GBA runtime
   - Model CPU-visible memory regions: BIOS stubs, EWRAM, IWRAM, IO, palette RAM, VRAM, OAM, ROM, SRAM.
   - Implement enough SWI, IRQ, DMA, timers, input, and PPU/audio surfaces to boot and render progressively.
   - Keep mGBA or another emulator as an oracle for traces and screenshots, not as the shipped runtime.

4. Incremental fallback
   - Unknown translated targets stop with structured diagnostics first.
   - Later, an interpreter fallback can be added for unresolved edge cases if needed.

## First Native Milestone

After approval, implement the smallest useful exe:

1. Add a CMake-based C++ host under `runtime/`.
2. Add `scripts/inspect-rom.ps1` or equivalent ROM metadata verification.
3. Add an initial translator scaffold that reads the ROM and decodes the reset entry basic block.
4. Build `advance-wars-native.exe`.
5. Running the exe should print verified ROM metadata and a decoded-entry summary, then stop cleanly with an explicit `next milestone required` diagnostic.

This is intentionally small. It proves we can build a native exe around the ROM and toolchain before filling in CPU translation and GBA hardware behavior.

## Next Milestones

1. Decode reachable ARM/Thumb functions and emit generated C++ block stubs.
2. Implement block dispatch and CPU register state.
3. Execute boot code until the first unimplemented memory or SWI operation.
4. Add memory map and BIOS SWI stubs needed by boot.
5. Add PPU/windowing using SDL2 or a similar native library.
6. Iterate against emulator traces until menus render.

## Key Risks

- The Rev 1 ROM differs from existing Advance Wars references that target the original USA revision.
- Code/data separation will need iterative correction.
- GBA hardware behavior is the long pole, especially rendering, DMA timing, IRQs, and audio.
- Generated-code volume can become large, so the project needs deterministic generation and incremental rebuilds.

## Open Decision

The strict recomp path above is slower but creates the right foundation for feature work. A faster native exe could embed an emulator core first, but that would be a compatibility shell instead of a recomp target.

Approval requested: proceed with the strict recomp runtime milestone above.
