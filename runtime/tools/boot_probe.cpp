// aw-boot-probe — headless boot + RAM mining harness for Advance Wars.
//
// The cursor miner needs a savestate taken in-game, and the sidebar/undo
// features need verified game-state addresses (menu-open flag, menu cursor,
// day counter). Getting those requires actually reaching the map, which this
// tool does by scripting the front-end: Advance Wars' menus default to the
// sensible option, so a steady Start/A cadence walks through title, campaign
// select and mission dialogue into the first mission's map.
//
// Modes:
//   boot <rom> [--out state.ss] [--max-frames N]
//       Spam Start/A until the map cursor (IWRAM 0x030036A4/0x030036A6,
//       mined by aw-cursor-miner) responds to the D-pad, then save the
//       savestate. The responsiveness probe presses Right/Down and checks
//       the cursor coordinate actually moved, then steps back.
//   menuprobe <rom> <state.ss>
//       From the map state: press Start (in-game menu), mine bytes that
//       (a) changed when the menu opened, (b) reverted when it closed, and
//       (c) track Up/Down by +/-1 (the menu cursor). Then A,A from the map
//       to try opening a unit action menu and repeats the same mining.
//   turnprobe <rom> <state.ss> <menu_cursor_abs> <index>
//       Press Start, drive the mined menu-cursor byte to `index`, confirm,
//       wait for the next player turn, and report every byte that changed
//       by exactly +1 across the turn change (day-counter candidates),
//       filtered to bytes that were stable beforehand.
//
// This tool never writes game memory; it only reads and sends input.

#include "aw/hardware.hpp"
#include "aw/mgba_adapter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using aw::kKeyA;
using aw::kKeyB;
using aw::kKeyDown;
using aw::kKeyLeft;
using aw::kKeyRight;
using aw::kKeyStart;
using aw::kKeyUp;

namespace {

constexpr std::uint32_t kCursorXAddr = 0x030036A4;
constexpr std::uint32_t kCursorYAddr = 0x030036A6;

struct RegionView {
  const char* label;
  std::uint32_t base_addr;
  std::vector<std::uint8_t> bytes;
};

struct CoreHandle {
  struct mCore* core = nullptr;
  std::vector<std::uint32_t> video;

  explicit CoreHandle(const std::string& rom) {
    video.resize(240 * 160, 0);
    core = aw_mgba_create(rom.c_str(), video.data(), 240);
  }
  ~CoreHandle() {
    if (core != nullptr) aw_mgba_destroy(core);
  }
  bool ok() const { return core != nullptr; }
};

void run_frames(struct mCore* core, int n, std::uint16_t keys = 0) {
  for (int i = 0; i < n; ++i) aw_mgba_run_frame(core, keys);
}

// Minimal 24-bit BMP writer so the harness can show where the boot script
// actually is. The runtime's video buffer is 0xAABBGGRR after mGBA's native
// format, matching Win32 DIBs.
void dump_bmp(const std::vector<std::uint32_t>& video, const std::string& path) {
  constexpr int w = 240, h = 160;
  const int row = w * 3;
  const int data = row * h;
  const int size = 54 + data;
  std::vector<std::uint8_t> bmp(size, 0);
  bmp[0] = 'B'; bmp[1] = 'M';
  bmp[2] = size & 0xFF; bmp[3] = (size >> 8) & 0xFF; bmp[4] = (size >> 16) & 0xFF;
  bmp[10] = 54;
  bmp[14] = 40;
  bmp[18] = w & 0xFF; bmp[19] = (w >> 8) & 0xFF;
  bmp[22] = h & 0xFF; bmp[23] = (h >> 8) & 0xFF;
  bmp[26] = 1; bmp[28] = 24;
  bmp[34] = data & 0xFF; bmp[35] = (data >> 8) & 0xFF; bmp[36] = (data >> 16) & 0xFF;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const std::uint32_t px = video[static_cast<std::size_t>(y) * w + x];
      std::uint8_t* dst = &bmp[54 + (h - 1 - y) * row + x * 3];
      dst[0] = px & 0xFF;         // blue
      dst[1] = (px >> 8) & 0xFF;  // green
      dst[2] = (px >> 16) & 0xFF; // red
    }
  }
  FILE* f = std::fopen(path.c_str(), "wb");
  if (f != nullptr) {
    std::fwrite(bmp.data(), 1, bmp.size(), f);
    std::fclose(f);
  }
}

void press(struct mCore* core, std::uint16_t key, int hold = 2, int settle = 6) {
  run_frames(core, hold, key);
  run_frames(core, settle, 0);
}

std::uint8_t cur_x(struct mCore* core) { return aw_mgba_read8(core, kCursorXAddr); }
std::uint8_t cur_y(struct mCore* core) { return aw_mgba_read8(core, kCursorYAddr); }

// True when the map cursor coordinate answers to the D-pad within a few
// frames. `restore_key` undoes the probe press so probing leaves the cursor
// where it was.
bool cursor_responsive(struct mCore* core, bool vertical) {
  const std::uint8_t before = vertical ? cur_y(core) : cur_x(core);
  const std::uint16_t probe = vertical ? kKeyDown : kKeyRight;
  const std::uint16_t restore = vertical ? kKeyUp : kKeyLeft;
  run_frames(core, 3, probe);
  run_frames(core, 4, 0);
  const std::uint8_t after = vertical ? cur_y(core) : cur_x(core);
  if (after == before) {
    // Maybe pinned at an edge: try the other direction before giving up.
    const std::uint16_t alt = vertical ? kKeyUp : kKeyLeft;
    const std::uint16_t alt_restore = vertical ? kKeyDown : kKeyRight;
    run_frames(core, 3, alt);
    run_frames(core, 4, 0);
    const std::uint8_t alt_after = vertical ? cur_y(core) : cur_x(core);
    if (alt_after == after) return false;
    run_frames(core, 3, alt_restore);
    run_frames(core, 4, 0);
    return true;
  }
  run_frames(core, 3, restore);
  run_frames(core, 4, 0);
  return true;
}

void capture_regions(struct mCore* core, RegionView& iwram, RegionView& ewram) {
  for (RegionView* region : {&iwram, &ewram}) {
    std::size_t size = 0;
    void* ptr = aw_mgba_memory_block(core, region->label, &size);
    region->bytes.assign(size, 0);
    if (ptr != nullptr && size > 0) {
      std::memcpy(region->bytes.data(), ptr, size);
    }
  }
}

struct ByteDelta {
  std::uint32_t addr;
  int before;
  int after;
  int delta() const { return after - before; }
};

// Bytes that changed by exactly `delta` between two captures of one region.
std::vector<ByteDelta> diff_exact(const RegionView& before, const RegionView& after, int delta) {
  std::vector<ByteDelta> out;
  const std::size_t n = std::min(before.bytes.size(), after.bytes.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (before.bytes[i] == after.bytes[i]) continue;
    if (static_cast<int>(after.bytes[i]) - static_cast<int>(before.bytes[i]) == delta) {
      out.push_back({before.base_addr + static_cast<std::uint32_t>(i),
                     before.bytes[i], after.bytes[i]});
    }
  }
  return out;
}

// Every byte that differs between two captures.
std::vector<ByteDelta> diff_any(const RegionView& before, const RegionView& after) {
  std::vector<ByteDelta> out;
  const std::size_t n = std::min(before.bytes.size(), after.bytes.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (before.bytes[i] != after.bytes[i]) {
      out.push_back({before.base_addr + static_cast<std::uint32_t>(i),
                     before.bytes[i], after.bytes[i]});
    }
  }
  return out;
}

void print_deltas(const char* what, const std::vector<ByteDelta>& deltas, std::size_t limit = 24) {
  std::printf("  %s: %zu byte(s)", what, deltas.size());
  if (deltas.empty()) {
    std::printf("\n");
    return;
  }
  std::printf(" (first %zu):\n", std::min(limit, deltas.size()));
  for (std::size_t i = 0; i < deltas.size() && i < limit; ++i) {
    std::printf("    0x%08X (decimal %u): %d -> %d\n", deltas[i].addr, deltas[i].addr,
                deltas[i].before, deltas[i].after);
  }
}

// ---------------------------------------------------------------------------
// boot
// ---------------------------------------------------------------------------
int mode_boot(const std::string& rom, const std::string& out, int max_frames) {
  CoreHandle handle(rom);
  if (!handle.ok()) {
    std::fprintf(stderr, "core creation failed\n");
    return 1;
  }
  struct mCore* core = handle.core;

  // Alternate Start and A presses (~2 Hz each) with idle gaps; every ~1.5 s
  // of in-game time, probe whether the map cursor is live yet.
  int frame = 0;
  int probes = 0;
  std::string dump_dir = "bootdump";
  std::filesystem::create_directories(dump_dir);
  while (frame < max_frames) {
    // Responsiveness probe: prefer horizontal, fall back to vertical.
    ++probes;
    if (probes % 10 == 1) {
      char name[64];
      std::snprintf(name, sizeof(name), "%s/frame_%06d.bmp", dump_dir.c_str(), frame);
      dump_bmp(handle.video, name);
      std::printf("boot: probe %d at frame %d, cursor bytes (%u, %u)\n",
                  probes, frame, cur_x(core), cur_y(core));
    }
    if (cursor_responsive(core, false) || cursor_responsive(core, true)) {
      std::printf("boot: map cursor responsive after %d frames (%d probes)\n", frame, probes);
      std::printf("boot: cursor at tile (%u, %u)\n", cur_x(core), cur_y(core));
      if (!out.empty()) {
        if (aw_mgba_save_state(core, out.c_str())) {
          std::printf("boot: saved %s\n", out.c_str());
        } else {
          std::fprintf(stderr, "boot: save state failed\n");
          return 1;
        }
      }
      return 0;
    }
    // Name entry needs more than A: cycle the letter grid with wrap-around
    // (Right/Down cycle through cells), then confirm. The press count grows
    // each attempt so the landing cell varies; one attempt lands on END.
    // Elsewhere the extra d-pad taps are harmless.
    if (probes % 10 == 0) {
      const int rights = 30 + probes;
      for (int i = 0; i < rights; ++i) press(core, kKeyRight, 2, 1);
      for (int i = 0; i < 40; ++i) press(core, kKeyDown, 2, 1);
      press(core, kKeyA);
      char name[64];
      std::snprintf(name, sizeof(name), "%s/escape_%06d.bmp", dump_dir.c_str(), frame);
      dump_bmp(handle.video, name);
    } else if (probes % 15 == 0) {
      press(core, kKeyB);
      run_frames(core, 6, 0);
    }
    // Stuck-screen triage: save a savestate of wherever we are early on, so
    // the offline cursor miner can work out how to drive this screen.
    if (probes == 80) {
      if (aw_mgba_save_state(core, "state_stuck.ss")) {
        std::printf("boot: saved state_stuck.ss for offline mining\n");
      }
    }
    // 90 frames of front-end mashing between probes.
    for (int i = 0; i < 6 && frame < max_frames; ++i) {
      const std::uint16_t key = (i % 2 == 0) ? kKeyStart : kKeyA;
      run_frames(core, 4, key);
      run_frames(core, 11, 0);
      frame += 15;
    }
  }
  std::fprintf(stderr, "boot: cursor never became responsive in %d frames\n", max_frames);
  return 1;
}

// ---------------------------------------------------------------------------
// Shared menu mining: with a menu open, find bytes that track Up/Down.
// ---------------------------------------------------------------------------
struct MenuMine {
  std::vector<ByteDelta> toggle_bytes;   // changed on open, reverted on close
  std::vector<ByteDelta> up_down_bytes;  // track the highlighted item
};

MenuMine mine_menu(struct mCore* core, bool use_start_menu) {
  MenuMine mine;

  // Baseline on the map.
  run_frames(core, 30, 0);
  RegionView iwram_before{"iwram", 0x03000000, {}};
  RegionView ewram_before{"wram", 0x02000000, {}};
  capture_regions(core, iwram_before, ewram_before);

  // Open the menu.
  if (use_start_menu) {
    press(core, kKeyStart);
  } else {
    press(core, kKeyA);  // select unit / show move range
    press(core, kKeyA);  // confirm in place -> action menu (if a unit was there)
  }

  RegionView iwram_open{"iwram", 0x03000000, {}};
  RegionView ewram_open{"wram", 0x02000000, {}};
  capture_regions(core, iwram_open, ewram_open);

  const bool cursor_frozen = !cursor_responsive(core, false) && !cursor_responsive(core, true);
  std::printf(use_start_menu ? "start menu: " : "action menu: ");
  std::printf("map cursor %s after open\n", cursor_frozen ? "FROZEN (menu took input)" : "still moving (no menu?)");

  // Track the menu cursor: press Up, Down, Up, Down and keep bytes that moved
  // -1, +1, -1, +1 in lockstep.
  RegionView iwram_step{"iwram", 0x03000000, {}};
  RegionView ewram_step{"wram", 0x02000000, {}};
  std::vector<std::uint32_t> live_addrs;
  {
    std::vector<std::uint32_t> candidates;
    for (int round = 0; round < 2; ++round) {
      const std::uint16_t keys[2] = {kKeyUp, kKeyDown};
      for (int k = 0; k < 2; ++k) {
        capture_regions(core, iwram_step, ewram_step);
        press(core, keys[k]);
        RegionView iwram_after{"iwram", 0x03000000, {}};
        RegionView ewram_after{"wram", 0x02000000, {}};
        capture_regions(core, iwram_after, ewram_after);
        const int expect = (keys[k] == kKeyUp) ? -1 : +1;
        std::vector<std::uint32_t> round_hits;
        for (const ByteDelta& d : diff_exact(iwram_step, iwram_after, expect)) {
          round_hits.push_back(d.addr);
        }
        for (const ByteDelta& d : diff_exact(ewram_step, ewram_after, expect)) {
          round_hits.push_back(d.addr);
        }
        if (round == 0 && k == 0) {
          candidates = round_hits;
        } else {
          std::vector<std::uint32_t> kept;
          std::set_intersection(candidates.begin(), candidates.end(),
                                round_hits.begin(), round_hits.end(),
                                std::back_inserter(kept));
          candidates = std::move(kept);
        }
      }
    }
    live_addrs = std::move(candidates);
  }

  // Close the menu and require the toggle bytes to revert.
  press(core, kKeyB);
  if (!use_start_menu) press(core, kKeyB);  // back out of unit selection too
  run_frames(core, 20, 0);
  RegionView iwram_closed{"iwram", 0x03000000, {}};
  RegionView ewram_closed{"wram", 0x02000000, {}};
  capture_regions(core, iwram_closed, ewram_closed);

  for (RegionView* closed : {&iwram_closed, &ewram_closed}) {
    const RegionView& before = (closed == &iwram_closed) ? iwram_before : ewram_before;
    const RegionView& open = (closed == &iwram_closed) ? iwram_open : ewram_open;
    for (const ByteDelta& d : diff_any(before, open)) {
      const std::size_t off = d.addr - before.base_addr;
      if (off < closed->bytes.size() && closed->bytes[off] == before.bytes[off]) {
        mine.toggle_bytes.push_back(d);
      }
    }
  }
  for (const std::uint32_t addr : live_addrs) {
    const RegionView* region = (addr < 0x03000000) ? &ewram_open : &iwram_open;
    const std::size_t off = addr - region->base_addr;
    mine.up_down_bytes.push_back({addr, 0, off < region->bytes.size() ? region->bytes[off] : 0});
  }
  return mine;
}

int mode_menuprobe(const std::string& rom, const std::string& state) {
  CoreHandle handle(rom);
  if (!handle.ok()) return 1;
  struct mCore* core = handle.core;
  if (!aw_mgba_load_state(core, state.c_str())) {
    std::fprintf(stderr, "menuprobe: load state failed: %s\n", state.c_str());
    return 1;
  }

  std::printf("== in-game (Start) menu ==\n");
  MenuMine start_mine = mine_menu(core, true);
  print_deltas("open/close toggle bytes", start_mine.toggle_bytes);
  print_deltas("menu cursor (Up/Down +/-1) bytes", start_mine.up_down_bytes);

  std::printf("== unit action menu (A, A) ==\n");
  MenuMine action_mine = mine_menu(core, false);
  print_deltas("open/close toggle bytes", action_mine.toggle_bytes);
  print_deltas("menu cursor (Up/Down +/-1) bytes", action_mine.up_down_bytes);

  // Toggle bytes present in BOTH menus are the generic "list menu open" state.
  std::vector<std::uint32_t> shared;
  for (const ByteDelta& s : start_mine.toggle_bytes) {
    for (const ByteDelta& a : action_mine.toggle_bytes) {
      if (s.addr == a.addr) shared.push_back(s.addr);
    }
  }
  std::printf("== shared by both menus (ListMenu predicate candidates) ==\n");
  std::printf("  %zu address(es)\n", shared.size());
  for (const std::uint32_t addr : shared) {
    std::printf("    0x%08X (decimal %u)\n", addr, addr);
  }
  return 0;
}

// ---------------------------------------------------------------------------
// turnprobe
// ---------------------------------------------------------------------------
int mode_turnprobe(const std::string& rom, const std::string& state,
                   std::uint32_t menu_cursor_addr, int target_index) {
  CoreHandle handle(rom);
  if (!handle.ok()) return 1;
  struct mCore* core = handle.core;
  if (!aw_mgba_load_state(core, state.c_str())) {
    std::fprintf(stderr, "turnprobe: load state failed\n");
    return 1;
  }
  run_frames(core, 30, 0);

  const auto menu_value = [&]() {
    return static_cast<int>(aw_mgba_read8(core, menu_cursor_addr));
  };

  // Open the in-game menu and drive the cursor to the requested index.
  press(core, kKeyStart);
  for (int guard = 0; guard < 16 && menu_value() != target_index; ++guard) {
    press(core, kKeyUp);  // wraps or climbs; AW menus are short
  }
  if (menu_value() != target_index) {
    for (int guard = 0; guard < 16 && menu_value() != target_index; ++guard) {
      press(core, kKeyDown);
    }
  }
  std::printf("turnprobe: menu cursor at %d (target %d)\n", menu_value(), target_index);

  // Candidate filter: bytes that hold still while idle stay eligible.
  RegionView iwram_idle_a{"iwram", 0x03000000, {}};
  RegionView ewram_idle_a{"wram", 0x02000000, {}};
  capture_regions(core, iwram_idle_a, ewram_idle_a);
  run_frames(core, 60, 0);
  RegionView iwram_idle_b{"iwram", 0x03000000, {}};
  RegionView ewram_idle_b{"wram", 0x02000000, {}};
  capture_regions(core, iwram_idle_b, ewram_idle_b);

  // Confirm the menu item and ride out the AI turn until the cursor answers
  // to the D-pad again (next player turn).
  press(core, kKeyA);
  int waited = 0;
  while (waited < 3600) {
    run_frames(core, 10, 0);
    waited += 10;
    // Responsiveness probing presses D-pad keys; during the AI turn the game
    // ignores them, so probing is safe.
    if (cursor_responsive(core, false)) break;
  }
  std::printf("turnprobe: player control returned after %d frames%s\n", waited,
              waited >= 3600 ? " (TIMEOUT - maybe not End Turn)" : "");

  RegionView iwram_after{"iwram", 0x03000000, {}};
  RegionView ewram_after{"wram", 0x02000000, {}};
  capture_regions(core, iwram_after, ewram_after);

  std::printf("== day-counter candidates: +1 across the turn AND idle-stable ==\n");
  for (const RegionView* idle_a : {&iwram_idle_a, &ewram_idle_a}) {
    const RegionView& idle_b = (idle_a == &iwram_idle_a) ? iwram_idle_b : ewram_idle_b;
    const RegionView& after = (idle_a == &iwram_idle_a) ? iwram_after : ewram_after;
    for (const ByteDelta& d : diff_exact(*idle_a, after, +1)) {
      const std::size_t off = d.addr - idle_a->base_addr;
      const bool stable = idle_b.bytes[off] == idle_a->bytes[off];
      if (stable) {
        std::printf("    0x%08X (decimal %u): %d -> %d\n", d.addr, d.addr, d.before, d.after);
      }
    }
  }
  return 0;
}

// ---------------------------------------------------------------------------
// rawprobe: load a state, run a scripted key sequence, print every byte that
// changed after each step. For reconstructing unknown UI encodings.
// ---------------------------------------------------------------------------
int mode_rawprobe(const std::string& rom, const std::string& state, const std::string& script) {
  CoreHandle handle(rom);
  if (!handle.ok()) return 1;
  struct mCore* core = handle.core;
  if (!aw_mgba_load_state(core, state.c_str())) {
    std::fprintf(stderr, "rawprobe: load state failed\n");
    return 1;
  }
  run_frames(core, 30, 0);
  dump_bmp(handle.video, "rawprobe_screen.bmp");

  auto key_for = [](char c) -> std::uint16_t {
    switch (c) {
      case 'U': return aw::kKeyUp;
      case 'D': return aw::kKeyDown;
      case 'L': return aw::kKeyLeft;
      case 'R': return aw::kKeyRight;
      case 'A': return aw::kKeyA;
      case 'B': return aw::kKeyB;
      case 'S': return aw::kKeyStart;
      default: return 0;
    }
  };

  RegionView iwram{"iwram", 0x03000000, {}};
  RegionView ewram{"wram", 0x02000000, {}};
  RegionView oam{"oam", 0x07000000, {}};
  auto capture = [&]() {
    capture_regions(core, iwram, ewram);
    std::size_t oam_size = 0;
    void* oam_ptr = aw_mgba_memory_block(core, "oam", &oam_size);
    oam.bytes.assign(oam_size, 0);
    if (oam_ptr != nullptr && oam_size > 0) {
      std::memcpy(oam.bytes.data(), oam_ptr, oam_size);
    }
  };
  capture();
  int shot_index = 0;
  for (const char c : script) {
    if (c == '.') {
      run_frames(core, 5, 0);
      continue;
    }
    press(core, key_for(c));
    char shot_name[64];
    std::snprintf(shot_name, sizeof(shot_name), "rawshot_%03d_%c.bmp", shot_index++, c);
    dump_bmp(handle.video, shot_name);
    RegionView iwram_now{"iwram", 0x03000000, {}};
    RegionView ewram_now{"wram", 0x02000000, {}};
    RegionView oam_now{"oam", 0x07000000, {}};
    std::size_t oam_size = 0;
    void* oam_ptr = aw_mgba_memory_block(core, "oam", &oam_size);
    oam_now.bytes.assign(oam_size, 0);
    if (oam_ptr != nullptr && oam_size > 0) {
      std::memcpy(oam_now.bytes.data(), oam_ptr, oam_size);
    }
    capture_regions(core, iwram_now, ewram_now);
    std::printf("after '%c':\n", c);
    print_deltas("IWRAM", diff_any(iwram, iwram_now), 16);
    print_deltas("EWRAM", diff_any(ewram, ewram_now), 16);
    print_deltas("OAM", diff_any(oam, oam_now), 32);
    iwram.bytes = iwram_now.bytes;
    ewram.bytes = ewram_now.bytes;
    oam.bytes = oam_now.bytes;
  }
  return 0;
}

// ---------------------------------------------------------------------------
// tomap: from a name-entry savestate, escape (varied wrap-cycle counts until
// the screen transitions), then A/Start-spam through dialogue until the map
// cursor answers to the D-pad. Saves the in-map savestate for offline mining.
// ---------------------------------------------------------------------------
int mode_tomap(const std::string& rom, const std::string& state, const std::string& out) {
  CoreHandle handle(rom);
  if (!handle.ok()) return 1;
  struct mCore* core = handle.core;
  if (!aw_mgba_load_state(core, state.c_str())) {
    std::fprintf(stderr, "tomap: load state failed\n");
    return 1;
  }
  run_frames(core, 30, 0);

  // Stage 1: the escape sequence verified by hand against this screen: 80
  // Rights and 80 Downs cycle the wrapping grid (entering a name along the
  // way), and the confirming A lands on END. Pixel-change heuristics are
  // useless here - the portrait blink repaints the whole screen every few
  // frames - so the sequence is replayed verbatim and not verified visually.
  const auto run_escape = [&]() {
    for (int i = 0; i < 80; ++i) press(core, kKeyRight, 2, 2);
    for (int i = 0; i < 80; ++i) press(core, kKeyDown, 2, 2);
    press(core, kKeyA);
  };
  run_escape();

  // Stage 2: march dialogue to the map with A presses only. The escape
  // sequence is NOT replayed: re-running it on later screens regresses to
  // name entry. If the march stalls, the caller inspects state_dialogue.ss.
  aw_mgba_save_state(core, "state_dialogue.ss");
  int cycle = 0;
  for (int frame = 0; frame < 30000; ) {
    ++cycle;
    if (cursor_responsive(core, false) || cursor_responsive(core, true)) {
      std::printf("tomap: map cursor responsive\n");
      dump_bmp(handle.video, "tomap_map.bmp");
      if (!out.empty()) {
        if (aw_mgba_save_state(core, out.c_str())) {
          std::printf("tomap: saved %s\n", out.c_str());
          return 0;
        }
        return 1;
      }
      return 0;
    }
    for (int i = 0; i < 6; ++i) {
      run_frames(core, 4, kKeyA);
      run_frames(core, 11, 0);
      frame += 15;
    }
    if (cycle % 200 == 0) {
      char shot[64];
      std::snprintf(shot, sizeof(shot), "tomap_stage2_%04d.bmp", cycle);
      dump_bmp(handle.video, shot);
    }
  }
  std::fprintf(stderr, "tomap: cursor never became responsive\n");
  dump_bmp(handle.video, "tomap_stuck.bmp");
  return 1;
}

// ---------------------------------------------------------------------------
// gridprobe: find the name-entry grid cursor variable. Presses
// R,R,L,L,U,U,D,D against the loaded state and keeps IWRAM/EWRAM bytes whose
// values trace a clean out-and-back walk (constant stride out, same stride
// back) for one axis and stay flat for the other. Prints survivors.
// ---------------------------------------------------------------------------
int mode_gridprobe(const std::string& rom, const std::string& state) {
  CoreHandle handle(rom);
  if (!handle.ok()) return 1;
  struct mCore* core = handle.core;
  if (!aw_mgba_load_state(core, state.c_str())) {
    std::fprintf(stderr, "gridprobe: load state failed\n");
    return 1;
  }
  run_frames(core, 30, 0);

  constexpr int kSteps = 8;
  const std::uint16_t script[kSteps] = {aw::kKeyRight, aw::kKeyRight, aw::kKeyLeft, aw::kKeyLeft,
                                        aw::kKeyUp, aw::kKeyUp, aw::kKeyDown, aw::kKeyDown};
  RegionView iwram{"iwram", 0x03000000, {}};
  RegionView ewram{"wram", 0x02000000, {}};
  capture_regions(core, iwram, ewram);

  // values[i] = value of every byte after script step i (values[0] = base).
  std::array<std::vector<std::uint8_t>, kSteps + 1> iwram_vals{};
  std::array<std::vector<std::uint8_t>, kSteps + 1> ewram_vals{};
  iwram_vals[0] = iwram.bytes;
  ewram_vals[0] = ewram.bytes;
  for (int s = 0; s < kSteps; ++s) {
    press(core, script[s]);
    RegionView iwram_now{"iwram", 0x03000000, {}};
    RegionView ewram_now{"wram", 0x02000000, {}};
    capture_regions(core, iwram_now, ewram_now);
    iwram_vals[s + 1] = iwram_now.bytes;
    ewram_vals[s + 1] = ewram_now.bytes;
  }

  struct AxisTrace {
    std::uint32_t addr;
    int v0;
    int stride;
    int axis;  // 0 = horizontal moves, 1 = vertical moves
  };
  std::vector<AxisTrace> found;
  for (const RegionView* region : {&iwram, &ewram}) {
    const auto& vals = (region == &iwram) ? iwram_vals : ewram_vals;
    const std::size_t n = region->bytes.size();
    for (std::size_t i = 0; i < n; ++i) {
      const int v0 = vals[0][i];
      // Horizontal: 1,2 steps +stride; 3,4 back to v0; vertical flat.
      const int h1 = vals[1][i], h2 = vals[2][i], h3 = vals[3][i], h4 = vals[4][i];
      const int v5 = vals[5][i], v6 = vals[6][i], v7 = vals[7][i], v8 = vals[8][i];
      if (h1 != v0 && h2 != h1 && h3 == h1 && h4 == v0 && v5 == v0 && v6 == v0 && v7 == v0 && v8 == v0) {
        const int stride = h1 - v0;
        if (h2 - h1 == stride && stride >= -16 && stride <= 16) {
          found.push_back({region->base_addr + static_cast<std::uint32_t>(i), v0, stride, 0});
        }
        continue;
      }
      if (v5 != v0 && v6 != v5 && v7 == v5 && v8 == v0 && h1 == v0 && h2 == v0 && h3 == v0 && h4 == v0) {
        const int stride = v5 - v0;
        if (v6 - v5 == stride && stride >= -16 && stride <= 16) {
          found.push_back({region->base_addr + static_cast<std::uint32_t>(i), v0, stride, 1});
        }
      }
    }
  }

  std::printf("== grid cursor candidates (%zu) ==\n", found.size());
  for (const AxisTrace& t : found) {
    std::printf("  0x%08X (decimal %u) axis=%s stride=%+d start=%d\n", t.addr, t.addr,
                t.axis == 0 ? "H" : "V", t.stride, t.v0);
  }
  if (found.empty()) {
    std::printf("  none: no byte traced a clean two-steps-out two-steps-back walk.\n");
  }
  return 0;
}

// ---------------------------------------------------------------------------
// namepass: escape the name-entry screen using only the framebuffer. The
// grid highlight is static, so movement is detected by comparing the whole
// frame before/after each press: press Right until the frame stops changing
// (pinned at the right edge, the DEL/END column), then Down until pinned
// again (END is the bottom cell of that column), then A. If nothing changed
// (empty name rejected), type one letter and retry.
// ---------------------------------------------------------------------------
int mode_namepass(const std::string& rom, const std::string& state) {
  CoreHandle handle(rom);
  if (!handle.ok()) return 1;
  struct mCore* core = handle.core;
  if (!aw_mgba_load_state(core, state.c_str())) {
    std::fprintf(stderr, "namepass: load state failed\n");
    return 1;
  }
  run_frames(core, 30, 0);

  const auto frame = [&]() { return handle.video; };
  const auto press_until_pinned = [&](std::uint16_t key, const char* label) {
    for (int guard = 0; guard < 80; ++guard) {
      const std::vector<std::uint32_t> before = frame();
      press(core, key, 2, 2);
      const std::vector<std::uint32_t>& now = frame();
      int changed = 0;
      for (std::size_t i = 0; i < now.size(); ++i) {
        if (now[i] != before[i]) ++changed;
      }
      if (changed < 8) return guard;  // pressed but nothing moved: pinned
    }
    return -1;
  };

  for (int attempt = 0; attempt < 3; ++attempt) {
    const std::vector<std::uint32_t> before_end = frame();
    const int rights = press_until_pinned(kKeyRight, "right");
    const int downs = press_until_pinned(kKeyDown, "down");
    std::printf("namepass: attempt %d, %d rights + %d downs to reach END\n", attempt, rights, downs);
    press(core, kKeyA);
    run_frames(core, 20, 0);
    const std::vector<std::uint32_t>& after = frame();
    int changed = 0;
    for (std::size_t i = 0; i < after.size(); ++i) {
      if (after[i] != before_end[i]) ++changed;
    }
    if (changed > 200) {
      dump_bmp(handle.video, "namepass_result.bmp");
      std::printf("namepass: screen changed (%d pixels) - escaped name entry\n", changed);
      return 0;
    }
    // END refused (likely empty name): type one letter, then retry.
    std::printf("namepass: END refused (only %d pixels changed); typing a letter and retrying\n", changed);
    press(core, kKeyB);
    press(core, kKeyLeft, 2, 2);
    press(core, kKeyUp, 2, 2);
    press(core, kKeyA);
    run_frames(core, 10, 0);
  }
  dump_bmp(handle.video, "namepass_stuck.bmp");
  std::fprintf(stderr, "namepass: still stuck after 3 attempts\n");
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr,
                 "Usage:\n"
                 "  aw-boot-probe <rom> boot [--out state.ss] [--max-frames N]\n"
                 "  aw-boot-probe <rom> menuprobe <state.ss>\n"
                 "  aw-boot-probe <rom> turnprobe <state.ss> <menu_cursor_abs> <index>\n");
    return 1;
  }
  const std::string rom = argv[1];
  const std::string mode = argv[2];

  if (mode == "boot") {
    std::string out = "state_map.ss";
    int max_frames = 40000;
    for (int i = 3; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--out" && i + 1 < argc) out = argv[++i];
      else if (arg == "--max-frames" && i + 1 < argc) max_frames = std::stoi(argv[++i]);
    }
    return mode_boot(rom, out, max_frames);
  }
  if (mode == "menuprobe" && argc == 4) {
    return mode_menuprobe(rom, argv[3]);
  }
  if (mode == "turnprobe" && argc == 6) {
    return mode_turnprobe(rom, argv[3],
                          static_cast<std::uint32_t>(std::stoul(argv[4], nullptr, 0)),
                          std::stoi(argv[5]));
  }
  if (mode == "rawprobe" && argc == 5) {
    return mode_rawprobe(rom, argv[3], argv[4]);
  }
  if (mode == "gridprobe" && argc == 4) {
    return mode_gridprobe(rom, argv[3]);
  }
  if (mode == "namepass" && argc == 4) {
    return mode_namepass(rom, argv[3]);
  }
  if (mode == "tomap" && (argc == 4 || argc == 5)) {
    return mode_tomap(rom, argv[3], argc == 5 ? argv[4] : "state_map.ss");
  }
  std::fprintf(stderr, "aw-boot-probe: bad arguments for mode '%s'\n", mode.c_str());
  return 1;
}
