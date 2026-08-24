// aw-cursor-miner — offline tool that locates the memory byte(s) holding the
// on-screen cursor's tile X/Y coordinates.
//
// Why this exists: the live steering path (OamTracker + PointerNav) guesses
// which OAM sprite is the cursor and closes a pixel-space error loop against
// that guess. When the guess is wrong the loop never converges. The fix is
// to stop guessing and read the cursor's tile coordinates directly out of
// game RAM — which requires knowing which byte(s) those are. This tool finds
// them, offline, from a savestate: press a D-pad direction against a live
// core, diff memory before/after, and keep only the bytes that moved by
// exactly +/-1 in lockstep with the press, across many repeats.
//
// This tool NEVER writes game memory. It only reads (via memcpy snapshots of
// live core memory) and sends input (D-pad presses) to advance the core.
//
// Usage: aw-cursor-miner <rom> <state.ss>

#include "aw/hardware.hpp"
#include "aw/mgba_adapter.h"
#include "aw/probe/backend_mgba.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace {

// At least 4 per the task spec; a couple extra rounds cheaply raises
// confidence without meaningfully slowing the tool down.
constexpr int kRounds = 6;
constexpr int kIdleFramesAfterRelease = 2;
constexpr std::size_t kMaxPrintedPerAxis = 40;

struct RegionView {
  const char* label;        // "EWRAM" / "IWRAM"
  std::uint32_t base_addr;  // absolute GBA address corresponding to offset 0
  const std::uint8_t* ptr;  // live core pointer, or nullptr if unavailable
  std::size_t size;
};

// Rolling intersection of "byte offsets whose delta matched exactly what we
// expected this round" across however many rounds actually produced a
// change. A round the game ignored (no byte changed at all) is skipped
// rather than intersected away, so a rejected press (e.g. cursor pinned at a
// map edge) doesn't wrongly collapse the candidate set to empty.
class CandidateAccumulator {
 public:
  void ingest_round(const std::vector<std::size_t>& matches, bool any_change) {
    if (!any_change) {
      ++skipped_rounds_;
      return;
    }
    ++valid_rounds_;
    std::set<std::size_t> round_set(matches.begin(), matches.end());
    if (!initialized_) {
      candidates_ = std::move(round_set);
      initialized_ = true;
    } else {
      std::set<std::size_t> next;
      std::set_intersection(candidates_.begin(), candidates_.end(), round_set.begin(),
                             round_set.end(), std::inserter(next, next.begin()));
      candidates_ = std::move(next);
    }
  }

  bool had_any_valid_round() const { return valid_rounds_ > 0; }
  int valid_rounds() const { return valid_rounds_; }
  int skipped_rounds() const { return skipped_rounds_; }
  const std::set<std::size_t>& candidates() const { return candidates_; }

 private:
  bool initialized_ = false;
  int valid_rounds_ = 0;
  int skipped_rounds_ = 0;
  std::set<std::size_t> candidates_;
};

void snapshot(const RegionView& region, std::vector<std::uint8_t>& out) {
  out.assign(region.size, 0);
  if (region.ptr != nullptr && region.size > 0) {
    std::memcpy(out.data(), region.ptr, region.size);
  }
}

bool any_changed(const std::vector<std::uint8_t>& before, const std::vector<std::uint8_t>& after) {
  return before != after;
}

std::vector<std::size_t> matches_with_delta(const std::vector<std::uint8_t>& before,
                                             const std::vector<std::uint8_t>& after,
                                             int expected_delta) {
  std::vector<std::size_t> out;
  const std::size_t n = before.size();
  for (std::size_t i = 0; i < n; ++i) {
    if (before[i] == after[i]) continue;
    const int delta = static_cast<int>(after[i]) - static_cast<int>(before[i]);
    if (delta == expected_delta) out.push_back(i);
  }
  return out;
}

void press_release_settle(mCore* core, std::uint16_t key) {
  aw_mgba_run_frame(core, key);  // pressed for one frame
  aw_mgba_run_frame(core, 0);    // released for one frame
  for (int i = 0; i < kIdleFramesAfterRelease; ++i) {
    aw_mgba_run_frame(core, 0);  // let the game commit the move
  }
}

// Runs `kRounds` press/release/settle cycles of `key`, feeding each region's
// accumulator with the bytes that moved by exactly `expected_delta` that
// round (or skipping the round if that region didn't change at all).
void run_direction(mCore* core, std::uint16_t key, int expected_delta,
                    std::vector<RegionView>& regions,
                    std::vector<CandidateAccumulator>& accumulators) {
  std::vector<std::vector<std::uint8_t>> before(regions.size());
  std::vector<std::vector<std::uint8_t>> after(regions.size());

  for (int round = 0; round < kRounds; ++round) {
    for (std::size_t r = 0; r < regions.size(); ++r) {
      snapshot(regions[r], before[r]);
    }

    press_release_settle(core, key);

    for (std::size_t r = 0; r < regions.size(); ++r) {
      snapshot(regions[r], after[r]);
      const bool changed = any_changed(before[r], after[r]);
      const std::vector<std::size_t> matches =
          changed ? matches_with_delta(before[r], after[r], expected_delta)
                  : std::vector<std::size_t>{};
      accumulators[r].ingest_round(matches, changed);
    }
  }
}

std::set<std::size_t> intersect(const std::set<std::size_t>& a, const std::set<std::size_t>& b) {
  std::set<std::size_t> out;
  std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::inserter(out, out.begin()));
  return out;
}

void print_axis_report(const char* axis_name, const RegionView& region,
                        const CandidateAccumulator& inc_acc, const CandidateAccumulator& dec_acc,
                        const std::set<std::size_t>& final_candidates,
                        const std::vector<std::uint8_t>& current_snapshot) {
  std::printf("\n--- %s candidates in %s ---\n", axis_name, region.label);
  std::printf("  increase-direction rounds: %d valid, %d skipped (no change)\n", inc_acc.valid_rounds(),
              inc_acc.skipped_rounds());
  std::printf("  decrease-direction rounds: %d valid, %d skipped (no change)\n", dec_acc.valid_rounds(),
              dec_acc.skipped_rounds());

  if (!inc_acc.had_any_valid_round() && !dec_acc.had_any_valid_round()) {
    std::printf("  NO CANDIDATES: neither direction ever changed a byte in %s. Likely reasons:\n",
                region.label);
    std::printf("    - the savestate was captured on a screen where the D-pad does not move a cursor\n");
    std::printf("    - the cursor was pinned at a map edge, so every press was rejected\n");
    std::printf("    - an animation or menu transition was playing and ate the input\n");
    return;
  }

  if (final_candidates.empty()) {
    std::printf("  NO CANDIDATES survived intersection (some bytes moved, but none tracked both\n");
    std::printf("  directions one-for-one). Likely reasons:\n");
    std::printf("    - the cursor was pinned at a map edge for one of the two directions\n");
    std::printf("    - camera scroll and cursor position changed different bytes each round\n");
    std::printf("    - the savestate is not actually on a steerable cursor screen\n");
    return;
  }

  std::printf("  %zu candidate(s) (showing up to %zu):\n", final_candidates.size(), kMaxPrintedPerAxis);
  std::size_t shown = 0;
  for (const std::size_t offset : final_candidates) {
    if (shown >= kMaxPrintedPerAxis) {
      std::printf("  ... (%zu more suppressed)\n", final_candidates.size() - shown);
      break;
    }
    const unsigned absolute = static_cast<unsigned>(region.base_addr + static_cast<std::uint32_t>(offset));
    const unsigned value =
        (offset < current_snapshot.size()) ? static_cast<unsigned>(current_snapshot[offset]) : 0u;
    std::printf("  %s+0x%05zX (absolute 0x%08X, decimal %u) value=%u\n", region.label, offset, absolute,
                absolute, value);
    ++shown;
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "Usage: aw-cursor-miner <rom> <state.ss>\n");
    return 1;
  }

  const std::string rom_path = argv[1];
  const std::string state_path = argv[2];

  struct mCore* core = aw_mgba_create(rom_path.c_str(), nullptr, 0);
  if (!core) {
    std::fprintf(stderr, "error: failed to create mGBA core for ROM: %s\n", rom_path.c_str());
    return 1;
  }

  if (!aw_mgba_load_state(core, state_path.c_str())) {
    std::fprintf(stderr, "error: failed to load savestate: %s\n", state_path.c_str());
    aw_mgba_destroy(core);
    return 1;
  }

  // Let transients (DMA in flight, palette fades, etc.) settle before probing.
  aw_mgba_run_frame(core, 0);
  aw_mgba_run_frame(core, 0);

  aw::MgbaProbeBackend probe(core);
  if (!probe.available()) {
    std::fprintf(stderr, "error: mGBA memory blocks unavailable; cannot probe this core\n");
    aw_mgba_destroy(core);
    return 1;
  }

  std::size_t ewram_size = 0;
  const std::uint8_t* ewram_ptr = probe.ewram(ewram_size);
  std::size_t iwram_size = 0;
  const std::uint8_t* iwram_ptr = probe.iwram(iwram_size);

  std::vector<RegionView> regions;
  regions.push_back(RegionView{"EWRAM", 0x02000000u, ewram_ptr, ewram_size});
  if (iwram_ptr != nullptr && iwram_size > 0) {
    // IWRAM is only 32 KB, but some games keep hot state (e.g. a cursor
    // position) there instead of in EWRAM, so we search both.
    regions.push_back(RegionView{"IWRAM", 0x03000000u, iwram_ptr, iwram_size});
  } else {
    std::printf("[info] IWRAM block unavailable from this core; scanning EWRAM only.\n");
  }

  std::printf("Loaded savestate '%s' against ROM '%s'.\n", state_path.c_str(), rom_path.c_str());
  std::printf("Regions scanned: ");
  for (std::size_t i = 0; i < regions.size(); ++i) {
    std::printf("%s%s (%zu bytes)", (i > 0 ? ", " : ""), regions[i].label, regions[i].size);
  }
  std::printf("\n");

  std::vector<CandidateAccumulator> right_acc(regions.size());
  std::vector<CandidateAccumulator> left_acc(regions.size());
  std::vector<CandidateAccumulator> down_acc(regions.size());
  std::vector<CandidateAccumulator> up_acc(regions.size());

  std::printf("\nProbing X axis (Right = +1, Left = -1) over %d rounds each direction...\n", kRounds);
  run_direction(core, aw::kKeyRight, +1, regions, right_acc);
  run_direction(core, aw::kKeyLeft, -1, regions, left_acc);

  std::printf("Probing Y axis (Down = +1, Up = -1) over %d rounds each direction...\n", kRounds);
  run_direction(core, aw::kKeyDown, +1, regions, down_acc);
  run_direction(core, aw::kKeyUp, -1, regions, up_acc);

  std::vector<std::vector<std::uint8_t>> final_snapshot(regions.size());
  for (std::size_t r = 0; r < regions.size(); ++r) {
    snapshot(regions[r], final_snapshot[r]);
  }

  std::vector<std::set<std::size_t>> x_candidates(regions.size());
  std::vector<std::set<std::size_t>> y_candidates(regions.size());

  for (std::size_t r = 0; r < regions.size(); ++r) {
    x_candidates[r] = intersect(right_acc[r].candidates(), left_acc[r].candidates());
    y_candidates[r] = intersect(down_acc[r].candidates(), up_acc[r].candidates());
    print_axis_report("X", regions[r], right_acc[r], left_acc[r], x_candidates[r], final_snapshot[r]);
    print_axis_report("Y", regions[r], down_acc[r], up_acc[r], y_candidates[r], final_snapshot[r]);
  }

  // Strong extra signal: an X candidate with a Y candidate adjacent in
  // memory (+/-1 or +/-2 bytes) is far more likely to be the real cursor
  // coordinate pair than an isolated byte, since games conventionally store
  // a cursor's X and Y next to each other (e.g. a packed struct).
  std::printf("\n=== Likely X/Y coordinate pairs (adjacent in memory) ===\n");
  bool any_pair_found = false;
  for (std::size_t r = 0; r < regions.size(); ++r) {
    for (const std::size_t x_off : x_candidates[r]) {
      for (int delta = -2; delta <= 2; ++delta) {
        if (delta == 0) continue;
        const long long y_off_signed = static_cast<long long>(x_off) + delta;
        if (y_off_signed < 0) continue;
        const std::size_t y_off = static_cast<std::size_t>(y_off_signed);
        if (y_candidates[r].count(y_off) == 0) continue;

        any_pair_found = true;
        const unsigned x_abs = static_cast<unsigned>(regions[r].base_addr + static_cast<std::uint32_t>(x_off));
        const unsigned y_abs = static_cast<unsigned>(regions[r].base_addr + static_cast<std::uint32_t>(y_off));
        const unsigned x_val = (x_off < final_snapshot[r].size())
                                    ? static_cast<unsigned>(final_snapshot[r][x_off])
                                    : 0u;
        const unsigned y_val = (y_off < final_snapshot[r].size())
                                    ? static_cast<unsigned>(final_snapshot[r][y_off])
                                    : 0u;
        std::printf(
            "  X %s+0x%05zX (0x%08X, value=%u)  <->  Y %s+0x%05zX (0x%08X, value=%u)  [offset %+d]\n",
            regions[r].label, x_off, x_abs, x_val, regions[r].label, y_off, y_abs, y_val, delta);
      }
    }
  }
  if (!any_pair_found) {
    std::printf("  none found (no X candidate had a Y candidate within +/-2 bytes)\n");
  }

  aw_mgba_destroy(core);
  return 0;
}
