# GBA Recomp Toolchain

## Core Build Tools

- `agbcc`: historical GCC-derived compiler used by many GBA matching decompilation projects.
- ARM binutils/devkitARM: provides `arm-none-eabi-as`, `arm-none-eabi-ld`, `arm-none-eabi-objcopy`, and `arm-none-eabi-objdump`.
- `gba-tools`: provides `gbafix` and related ROM utilities.

## Reverse-Engineering Tools

- `luvdis`: configurable GBA disassembler for bootstrapping assembly output and function maps.
- Ghidra: static analysis and type/function investigation.
- mGBA: emulator/debugger for runtime behavior checks.

## Local Bootstrap

Use PowerShell on this machine:

```powershell
.\scripts\bootstrap-tools.ps1
.\scripts\check-tools.ps1
```

The bootstrap intentionally keeps fetched source repositories under ignored paths in `tools/src/`.

## Docker Bootstrap

Docker Desktop is installed on this machine, but the Linux engine must be running before this command works:

```powershell
.\scripts\bootstrap-tools.ps1 -BuildDocker
.\scripts\build-agbcc.ps1
.\scripts\build-gba-tools.ps1
```

The image includes Python tooling, ARM binutils, Make, GCC, CMake, Ninja, autotools, and multilib support for building `agbcc` and `gba-tools`. The `build-agbcc.ps1` wrapper installs compiler files to `tools/agbcc/`; `build-gba-tools.ps1` installs ROM utilities to `tools/gba-tools/`.
