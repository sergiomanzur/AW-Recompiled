#pragma once

#include <cstdint>
#include <vector>

struct mCore;

namespace aw {

// Advance Wars map cursor position addresses discovered via RAM scanning.
// These are EWRAM addresses (0x0200xxxx) that hold the game's cursor X/Y.
struct CursorAddresses {
  std::uint32_t cursor_x_addr = 0;  // Address holding cursor X tile coordinate
  std::uint32_t cursor_y_addr = 0;  // Address holding cursor Y tile coordinate
  bool validated = false;            // True if addresses have been confirmed working
};

// Manages direct game memory mouse cursor support for Advance Wars (GBA recomp).
// Uses mGBA rawRead/rawWrite to discover and manipulate the in-game cursor position.
class MouseCursor {
public:
  MouseCursor() = default;

  // Set the mGBA core pointer (call after core creation and after ROM switch)
  void set_core(mCore* core);

  // Returns true if cursor addresses have been discovered and validated
  bool is_active() const { return addrs_.validated && core_ != nullptr; }

  // Attempt to discover cursor position addresses via RAM difference scanning.
  // Call this once during early gameplay frames. Returns true if addresses found.
  // Phase-based: call repeatedly over multiple frames with D-pad inputs between calls.
  //
  // scan_phase:
  //   0 = take initial EWRAM snapshot (before any D-pad input)
  //   1 = take snapshot after Right press, find candidates for X
  //   2 = take snapshot after Down press, find candidates for Y
  //   3 = validate by checking addresses respond to D-pad correctly
  bool run_scan_phase(int phase);

  // Handle a mouse click at GBA screen coordinates (0-239, 0-159).
  // Returns the key bitmask to inject this frame (e.g., kKeyA after teleporting cursor).
  // If not active, returns 0 (caller should use fallback input).
  std::uint16_t handle_click(int gba_x, int gba_y);

  // Passive RAM scanning based on real gameplay D-pad inputs
  void update_passive_scan(std::uint16_t keys_pressed);

  // Handle mouse movement to GBA screen coordinates.
  // For map screens, this teleports the cursor to the tile under the mouse.
  // Returns D-pad bitmask to inject (0 if direct write was used).
  std::uint16_t handle_move(int gba_x, int gba_y);

  // Read the current cursor position from game RAM.
  // Returns false if not active.
  bool read_cursor(int& out_x, int& out_y) const;

  // Write cursor position directly to game RAM.
  // Returns false if not active.
  bool write_cursor(int x, int y);

  // Reset state (e.g., on ROM switch)
  void reset();

  // Get scan status for debug output
  int scan_phase() const { return scan_phase_; }

  // Whether a click-triggered A press is pending (injected next frame)
  bool has_pending_a_press() const { return pending_a_frames_ > 0; }
  std::uint16_t consume_pending_keys();

private:
  // Take a snapshot of a portion of EWRAM for difference scanning
  void snapshot_ewram(std::vector<std::uint8_t>& out);

  // Find bytes that changed between two snapshots
  void find_changed_bytes(const std::vector<std::uint8_t>& before,
                          const std::vector<std::uint8_t>& after,
                          std::vector<std::uint32_t>& candidates);

  // Intersect a candidate list with bytes that changed in a new scan
  void narrow_candidates(const std::vector<std::uint8_t>& before,
                         const std::vector<std::uint8_t>& after,
                         std::vector<std::uint32_t>& candidates);

  mCore* core_ = nullptr;
  CursorAddresses addrs_;

  // Scanning state
  int scan_phase_ = -1;  // -1 = not started, 0-3 = phases
  bool has_snapshot_ = false;
  int scan_count_x_ = 0;
  int scan_count_y_ = 0;
  std::vector<std::uint8_t> snapshot_before_;
  std::vector<std::uint32_t> x_candidates_;
  std::vector<std::uint32_t> y_candidates_;

  // Click handling state
  int pending_a_frames_ = 0;       // Frames remaining for A press after click
  std::uint16_t pending_keys_ = 0; // Keys to inject next frame

  // EWRAM scan range (Full 256 KB EWRAM: 0x02000000 - 0x0203FFFF)
  static constexpr std::uint32_t kEwramBase = 0x02000000;
  static constexpr std::uint32_t kEwramSize = 0x00040000; // 256 KB
  static constexpr std::uint32_t kScanBase = 0x02000000;
  static constexpr std::uint32_t kScanSize = 0x00040000; // Full 256 KB
};

}  // namespace aw
