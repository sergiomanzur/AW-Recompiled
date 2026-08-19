#include "aw/mouse_cursor.hpp"
#include "aw/hardware.hpp"
#include "aw/mgba_adapter.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace aw {

void MouseCursor::set_core(mCore* core) {
  core_ = core;
  // Don't reset addresses on core change — they persist across frames
}

void MouseCursor::reset() {
  addrs_ = {};
  scan_phase_ = -1;
  has_snapshot_ = false;
  scan_count_x_ = 0;
  scan_count_y_ = 0;
  snapshot_before_.clear();
  x_candidates_.clear();
  y_candidates_.clear();
  pending_a_frames_ = 0;
  pending_keys_ = 0;
}

void MouseCursor::update_passive_scan(std::uint16_t keys_pressed) {
  if (addrs_.validated || !core_) return;

  const bool right = (keys_pressed & kKeyRight) != 0;
  const bool left  = (keys_pressed & kKeyLeft) != 0;
  const bool down  = (keys_pressed & kKeyDown) != 0;
  const bool up    = (keys_pressed & kKeyUp) != 0;

  if (!right && !left && !down && !up) {
    // Refresh idle EWRAM snapshot
    snapshot_ewram(snapshot_before_);
    has_snapshot_ = true;
    return;
  }

  if (!has_snapshot_) return;

  // Take snapshot after D-pad movement
  std::vector<std::uint8_t> snapshot_after;
  snapshot_ewram(snapshot_after);

  if (right || left) {
    const int expected_diff = right ? 1 : -1;
    std::vector<std::uint32_t> new_cands;
    for (std::uint32_t i = 0; i < kScanSize; ++i) {
      const int diff = static_cast<int>(snapshot_after[i]) - static_cast<int>(snapshot_before_[i]);
      if (diff == expected_diff) {
        const std::uint32_t addr = kScanBase + i;
        if (x_candidates_.empty() && scan_count_x_ == 0) {
          new_cands.push_back(addr);
        } else {
          if (std::find(x_candidates_.begin(), x_candidates_.end(), addr) != x_candidates_.end()) {
            new_cands.push_back(addr);
          }
        }
      }
    }
    x_candidates_ = std::move(new_cands);
    scan_count_x_++;
    std::cout << "[MouseCursor] Passive X candidates: " << x_candidates_.size() << std::endl;
  }

  if (down || up) {
    const int expected_diff = down ? 1 : -1;
    std::vector<std::uint32_t> new_cands;
    for (std::uint32_t i = 0; i < kScanSize; ++i) {
      const int diff = static_cast<int>(snapshot_after[i]) - static_cast<int>(snapshot_before_[i]);
      if (diff == expected_diff) {
        const std::uint32_t addr = kScanBase + i;
        if (y_candidates_.empty() && scan_count_y_ == 0) {
          new_cands.push_back(addr);
        } else {
          if (std::find(y_candidates_.begin(), y_candidates_.end(), addr) != y_candidates_.end()) {
            new_cands.push_back(addr);
          }
        }
      }
    }
    y_candidates_ = std::move(new_cands);
    scan_count_y_++;
    std::cout << "[MouseCursor] Passive Y candidates: " << y_candidates_.size() << std::endl;
  }

  // Validate when candidates have narrowed down
  if (!x_candidates_.empty() && !y_candidates_.empty() && scan_count_x_ >= 1 && scan_count_y_ >= 1) {
    for (std::uint32_t x_addr : x_candidates_) {
      const std::uint8_t vx = aw_mgba_read8(core_, x_addr);
      if (vx <= 30) {
        for (std::uint32_t y_addr : y_candidates_) {
          const std::uint8_t vy = aw_mgba_read8(core_, y_addr);
          if (vy <= 20) {
            addrs_.cursor_x_addr = x_addr;
            addrs_.cursor_y_addr = y_addr;
            addrs_.validated = true;
            std::cout << "[MouseCursor] Passive Scan SUCCESS! Cursor X @ 0x" << std::hex << x_addr
                      << " (val=" << std::dec << (int)vx << "), Y @ 0x" << std::hex << y_addr
                      << " (val=" << std::dec << (int)vy << ")" << std::endl;
            break;
          }
        }
        if (addrs_.validated) break;
      }
    }
  }

  snapshot_before_ = std::move(snapshot_after);
}

void MouseCursor::snapshot_ewram(std::vector<std::uint8_t>& out) {
  out.resize(kScanSize);
  if (!core_) {
    std::memset(out.data(), 0, kScanSize);
    return;
  }
  for (std::uint32_t i = 0; i < kScanSize; ++i) {
    out[i] = aw_mgba_read8(core_, kScanBase + i);
  }
}

void MouseCursor::find_changed_bytes(
    const std::vector<std::uint8_t>& before,
    const std::vector<std::uint8_t>& after,
    std::vector<std::uint32_t>& candidates) {
  candidates.clear();
  const std::size_t sz = std::min(before.size(), after.size());
  for (std::uint32_t i = 0; i < sz; ++i) {
    if (before[i] != after[i]) {
      candidates.push_back(kScanBase + i);
    }
  }
}

void MouseCursor::narrow_candidates(
    const std::vector<std::uint8_t>& before,
    const std::vector<std::uint8_t>& after,
    std::vector<std::uint32_t>& candidates) {
  // Keep only candidates that changed in this new scan too
  std::vector<std::uint32_t> narrowed;
  for (std::uint32_t addr : candidates) {
    const std::uint32_t offset = addr - kScanBase;
    if (offset < before.size() && offset < after.size()) {
      if (before[offset] != after[offset]) {
        narrowed.push_back(addr);
      }
    }
  }
  candidates = std::move(narrowed);
}

bool MouseCursor::run_scan_phase(int phase) {
  if (!core_) return false;

  switch (phase) {
    case 0: {
      // Phase 0: Take initial snapshot before any movement
      snapshot_ewram(snapshot_before_);
      x_candidates_.clear();
      y_candidates_.clear();
      scan_phase_ = 0;
      std::cout << "[MouseCursor] Phase 0: Initial EWRAM snapshot taken ("
                << kScanSize << " bytes)" << std::endl;
      return true;
    }
    case 1: {
      // Phase 1: After a Right D-pad press, find bytes that changed (X candidates)
      std::vector<std::uint8_t> snapshot_after;
      snapshot_ewram(snapshot_after);

      if (x_candidates_.empty()) {
        find_changed_bytes(snapshot_before_, snapshot_after, x_candidates_);
      } else {
        narrow_candidates(snapshot_before_, snapshot_after, x_candidates_);
      }

      // Filter: X candidates should have increased by exactly 1
      std::vector<std::uint32_t> filtered;
      for (std::uint32_t addr : x_candidates_) {
        const std::uint32_t offset = addr - kScanBase;
        const std::int8_t diff = static_cast<std::int8_t>(
            snapshot_after[offset] - snapshot_before_[offset]);
        if (diff == 1) {
          filtered.push_back(addr);
        }
      }
      x_candidates_ = std::move(filtered);

      snapshot_before_ = std::move(snapshot_after);
      scan_phase_ = 1;
      std::cout << "[MouseCursor] Phase 1: X candidates after Right press: "
                << x_candidates_.size() << std::endl;
      return true;
    }
    case 2: {
      // Phase 2: After a Down D-pad press, find bytes that changed (Y candidates)
      std::vector<std::uint8_t> snapshot_after;
      snapshot_ewram(snapshot_after);

      if (y_candidates_.empty()) {
        find_changed_bytes(snapshot_before_, snapshot_after, y_candidates_);
      } else {
        narrow_candidates(snapshot_before_, snapshot_after, y_candidates_);
      }

      // Filter: Y candidates should have increased by exactly 1
      std::vector<std::uint32_t> filtered;
      for (std::uint32_t addr : y_candidates_) {
        const std::uint32_t offset = addr - kScanBase;
        const std::int8_t diff = static_cast<std::int8_t>(
            snapshot_after[offset] - snapshot_before_[offset]);
        if (diff == 1) {
          filtered.push_back(addr);
        }
      }
      y_candidates_ = std::move(filtered);

      snapshot_before_ = std::move(snapshot_after);
      scan_phase_ = 2;
      std::cout << "[MouseCursor] Phase 2: Y candidates after Down press: "
                << y_candidates_.size() << std::endl;
      return true;
    }
    case 3: {
      // Phase 3: Validate — check if we have exactly 1 candidate each
      // If we have multiple candidates, try additional filtering by reading current values
      if (x_candidates_.size() >= 1 && y_candidates_.size() >= 1) {
        // Pick the best candidate: prefer addresses close together (likely same struct)
        // and with small values (tile coordinates are typically 0-29)
        std::uint32_t best_x = 0;
        std::uint8_t best_x_val = 255;
        for (std::uint32_t addr : x_candidates_) {
          std::uint8_t val = aw_mgba_read8(core_, addr);
          if (val < 30 && val < best_x_val) {
            best_x = addr;
            best_x_val = val;
          }
        }

        std::uint32_t best_y = 0;
        std::uint8_t best_y_val = 255;
        for (std::uint32_t addr : y_candidates_) {
          std::uint8_t val = aw_mgba_read8(core_, addr);
          if (val < 20 && val < best_y_val) {
            best_y = addr;
            best_y_val = val;
          }
        }

        if (best_x != 0 && best_y != 0) {
          addrs_.cursor_x_addr = best_x;
          addrs_.cursor_y_addr = best_y;
          addrs_.validated = true;
          scan_phase_ = 3;

          std::cout << "[MouseCursor] Phase 3: VALIDATED! Cursor X @ 0x"
                    << std::hex << best_x << " (val=" << std::dec << (int)best_x_val
                    << "), Y @ 0x" << std::hex << best_y
                    << " (val=" << std::dec << (int)best_y_val << ")" << std::endl;
          return true;
        }
      }

      std::cout << "[MouseCursor] Phase 3: Validation FAILED. X candidates: "
                << x_candidates_.size() << ", Y candidates: " << y_candidates_.size()
                << std::endl;
      scan_phase_ = -1;  // Reset for retry
      return false;
    }
    default:
      return false;
  }
}

bool MouseCursor::read_cursor(int& out_x, int& out_y) const {
  if (!is_active()) return false;
  out_x = aw_mgba_read8(core_, addrs_.cursor_x_addr);
  out_y = aw_mgba_read8(core_, addrs_.cursor_y_addr);
  return true;
}

bool MouseCursor::write_cursor(int x, int y) {
  if (!is_active()) return false;
  // Clamp to valid Advance Wars map ranges (max 30x20 tiles)
  x = std::clamp(x, 0, 29);
  y = std::clamp(y, 0, 19);
  aw_mgba_write8(core_, addrs_.cursor_x_addr, static_cast<std::uint8_t>(x));
  aw_mgba_write8(core_, addrs_.cursor_y_addr, static_cast<std::uint8_t>(y));
  return true;
}

std::uint16_t MouseCursor::handle_click(int gba_x, int gba_y) {
  if (!is_active()) {
    // Fallback: just return A button press
    return kKeyA;
  }

  // Convert GBA pixel coordinates to tile coordinates
  // Advance Wars uses 16x16 pixel tiles on the 240x160 GBA screen
  // The visible area is 15x10 tiles (240/16 = 15, 160/16 = 10)
  const int tile_x = gba_x / 16;
  const int tile_y = gba_y / 16;

  // Read current cursor position to compute the map-relative target
  int cur_x = 0, cur_y = 0;
  read_cursor(cur_x, cur_y);

  // The viewport shows tiles starting from a scroll offset.
  // The cursor's tile position in the viewport = cursor_pos - scroll_offset
  // So scroll_offset ≈ cursor_pos - cursor_viewport_pos
  // We don't know the exact viewport cursor position, but the cursor is
  // typically centered or near-center. For now, compute the delta and add to current pos.
  // This means: click at tile (5,3) on screen when cursor is at map(10,8) showing at screen(7,5)
  // → target map pos = current_map_pos + (click_tile - cursor_screen_tile)
  // Since we can't know cursor_screen_tile without more RAM scanning, we use a simpler approach:
  // Just write the tile position as an absolute position, which works for small maps
  // that fit entirely on screen (like menus and name entry).

  // For the general case, write the target position directly
  write_cursor(tile_x, tile_y);

  // Queue an A button press for the next frame (after the cursor has moved)
  pending_a_frames_ = 2;  // Press A for 2 frames after teleport
  pending_keys_ = kKeyA;

  return 0;  // Don't press anything this frame — let cursor teleport first
}

std::uint16_t MouseCursor::handle_move(int gba_x, int gba_y) {
  if (!is_active()) return 0;

  // Convert to tile coordinates
  const int tile_x = gba_x / 16;
  const int tile_y = gba_y / 16;

  // Silently move cursor to follow mouse (hover mode)
  write_cursor(tile_x, tile_y);

  return 0;
}

std::uint16_t MouseCursor::consume_pending_keys() {
  if (pending_a_frames_ > 0) {
    pending_a_frames_--;
    std::uint16_t keys = pending_keys_;
    if (pending_a_frames_ == 0) {
      pending_keys_ = 0;
    }
    return keys;
  }
  return 0;
}

}  // namespace aw
