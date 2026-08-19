# GBA Toolchain Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a reproducible first-pass Game Boy Advance recompilation toolchain for this empty Advance Wars workspace.

**Architecture:** Keep project-owned setup in scripts and manifests, while storing downloaded compiler/tool sources in ignored directories. Prefer a Docker/Linux path for historical compiler builds and a local Python venv for disassembly helpers.

**Tech Stack:** PowerShell, Bash, Python venv, Docker, ARM binutils/devkitARM-style tools, `agbcc`, `gba-tools`, `luvdis`.

**Spec:** User request: "Let's begin working on a new recomp. This time it's a GBA game. Please bring in all the tools we need to recomp a gba game first."

## Global Constraints

- Do not commit copyrighted ROM data.
- Keep fetched third-party source drops and generated binaries ignored by Git.
- Prefer reproducible project-local setup over silent global machine installs.

---

### Task 1: Toolchain Scaffolding

**Files:**
- Create: `.gitignore`
- Create: `README.md`
- Create: `requirements-tools.txt`
- Create: `tools/manifest.json`
- Create: `tools/Dockerfile.gba`
- Create: `scripts/bootstrap-tools.ps1`
- Create: `scripts/bootstrap-tools.sh`
- Create: `scripts/check-tools.ps1`
- Create: `scripts/build-agbcc.ps1`
- Create: `scripts/build-gba-tools.ps1`
- Create: `config/luvdis-functions.example.cfg`
- Create: `docs/toolchain.md`

**Interfaces:**
- Produces: `scripts/bootstrap-tools.ps1` to install Python helpers and fetch GBA tool sources.
- Produces: `scripts/check-tools.ps1` to report host and local tool availability.

- [x] **Step 1: Add ignore rules for local tools, ROMs, and build output**

- [x] **Step 2: Add project README and toolchain notes**

- [x] **Step 3: Add Python requirements for disassembly helpers**

- [x] **Step 4: Add tool manifest documenting each dependency and role**

- [x] **Step 5: Add PowerShell and Bash bootstrap scripts**

- [x] **Step 6: Add PowerShell verification script**

- [x] **Step 7: Add Dockerfile for Linux-based GBA toolchain**

- [x] **Step 8: Add Docker wrapper for building and installing agbcc**

- [x] **Step 9: Add Docker wrapper for building and installing gba-tools**
