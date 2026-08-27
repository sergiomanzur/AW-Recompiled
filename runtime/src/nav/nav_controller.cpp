#include "aw/nav/nav_controller.hpp"

#include "aw/config.hpp"
#include "aw/hardware.hpp"
#include "aw/probe/cursor_probe.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>

namespace aw {

namespace {

// GBA map tiles are 16x16 px. Both the exact-coordinate indicator and the
// pointer's target are expressed in these units (scaled back up to pixels,
// see update()) so PointerNav's interface -- which speaks pixels -- needs no
// change at all.
constexpr int kTileSize = 16;

// BG layer whose scroll registers track the map, used only as a fallback
// when a context rule doesn't pin scroll_bg itself (the mined symbol table
// for this ROM currently declares no context rules at all, only cursor
// addresses -- see data/symbols/README.md). Chosen from BGxCNT structure on
// the one real savestate available (build/native/runtime/state_1.ss):
// BG0/BG1 share character base 0 (a font/UI tile set) while BG2/BG3 share
// character base 2 (a distinct, terrain-sized tile set), and BG2 sits above
// BG3 in priority. That map never actually scrolls (it is exactly one
// screen, 15x10 tiles), so this could not be confirmed by watching HOFS/VOFS
// change -- see task-9f-report.md for the full investigation. Wrong or not,
// a map that fits on screen (scroll stays 0 on every layer) is unaffected
// either way.
constexpr int kDefaultMapScrollBg = 2;

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// std::getenv rather than the Windows-only _dupenv_s: this file may not
// include Windows headers, even transitively.
bool env_is_set(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr && std::string(value) == "1";
}

}  // namespace

void NavController::reset() {
  tracker_.reset();
  nav_.reset();
  context_ = ContextId::Unknown;
  prev_context_ = ContextId::Unknown;
  indicator_ = {};
  last_emitted_dpad_ = 0;
  pointer_armed_ = false;
  burst_target_x_ = 0;
  burst_target_y_ = 0;
  burst_remaining_ = 0;
  burst_phase_ = 0;
  burst_dir_ = 0;
  prev_step_tile_x_ = 0;
  prev_step_tile_y_ = 0;
  has_prev_step_tile_ = false;
  in_map_ = false;
}

void NavController::load_symbols(const std::string& rom_sha1) {
  const std::string path = std::string(AW_DATA_DIR) + "/symbols/" + to_lower(rom_sha1) + ".ini";
  SymbolTable table;
  std::string err;
  if (!table.load_from_file(path, err)) {
    std::cout << "[nav] no symbol table (" << err
              << "); using correlation tracking\n";
    symbols_loaded_ = false;
    log_steering_mode();
    return;
  }
  if (!table.matches_rom(rom_sha1)) {
    std::cout << "[nav] symbol table ROM mismatch; using correlation tracking\n";
    symbols_loaded_ = false;
    log_steering_mode();
    return;
  }
  context_probe_.set_table(std::move(table));
  symbols_loaded_ = true;
  std::cout << "[nav] loaded symbol table " << path << "\n";
  log_steering_mode();
}

void NavController::log_steering_mode() const {
  // Logged once at startup (right after load_symbols() resolves the table),
  // not per frame: this is a fixed property of the loaded table, not
  // something that changes frame to frame.
  if (context_probe_.table().cursor.valid()) {
    std::cout << "[nav] steering mode: exact tile coordinates (mined cursor addresses found)\n";
  } else {
    std::cout << "[nav] steering mode: OAM correlation fallback (no mined cursor addresses)\n";
  }
}

std::uint16_t NavController::update(const InputFrame& frame) {
  if (!debug_checked_) {
    debug_enabled_ = env_is_set("AW_NAV_DEBUG");
    debug_checked_ = true;
  }
  ++frame_counter_;

  if (backend_ == nullptr || !backend_->available()) {
    if (debug_enabled_ && (frame_counter_ % 60) == 1) {
      std::cout << "[nav-debug] frame=" << frame_counter_
                << " backend unavailable (backend_set="
                << (backend_ != nullptr) << ")\n";
    }
    return 0;
  }

  context_ = context_probe_.classify(*backend_);

  // rule_for() returns a reference into the symbol table's context list,
  // which a later set_table() call would invalidate. Nothing here calls
  // set_table(), but the fields are copied out immediately regardless so
  // this stays correct if that ever changes.
  const ContextRule& rule = context_probe_.rule_for(context_);
  const IndicatorSignature signature = rule.signature;
  const bool steerable = rule.steerable;
  const int scroll_bg = rule.scroll_bg;

  // Only reset/re-sign the tracker on a context change. Calling
  // set_signature() unconditionally every frame (as the wildcard check
  // alone would do) would reset OamTracker's missing-frame counter every
  // frame too, so its own unlock path could never fire, and it would leave
  // a mined context's signature pinned after leaving that context for an
  // unmined one -- the tracker would keep hunting for the previous screen's
  // sprite. Resetting on change and falling back to a wildcard signature
  // for an unmined context makes it re-correlate from scratch instead.
  if (context_ != prev_context_) {
    tracker_.reset();
    if (!signature.wildcard()) {
      tracker_.set_signature(signature);
    }
    prev_context_ = context_;
  }

  // Only steer and process mouse/touch pointer navigation during gameplay on the map.
  // In menus, name entry, title, and cutscenes, pointer navigation is disabled so the
  // player uses the standard D-pad / keyboard / joystick controls.
  const bool on_map = in_map_ || context_ == ContextId::MapView;
  if (!on_map) {
    tracker_.reset();
    nav_.reset();
    pointer_armed_ = false;
    last_emitted_dpad_ = 0;
    return 0;
  }

  const CursorTile cursor_tile = read_cursor_tile(*backend_, context_probe_.table().cursor);
  const bool exact_mode = cursor_tile.found;

  // OamTracker still runs unconditionally, exact mode or not: The OAM we are
  // about to read is the result of last frame's keys, so correlation is
  // scored against last frame's emitted D-pad. Running it every frame (not
  // just when falling back to it) keeps its lock/missing-frame bookkeeping
  // live so it is instantly usable the moment a screen with no mined cursor
  // (or a ROM whose table has no [Cursor] section at all) needs it, rather
  // than picking up mid-reset.
  indicator_ = tracker_.update(backend_->oam(), last_emitted_dpad_);

  NavInput in;
  in.steerable = steerable;
  in.device_dpad = frame.device_dpad;

  // A physical D-pad press always wins (PointerNav::step() disarms itself on
  // device_dpad too), but that disarm is per-frame: without also clearing the
  // latch here, the very next frame the key is released -- with the mouse
  // still sitting motionless -- would see pointer_armed_ still true from
  // before the press and hand steering straight back. Clearing it every
  // frame the physical dpad is held means the mouse must move again once the
  // player lets go, which is the documented intent: a resting mouse must not
  // fight the controller.
  if ((frame.device_dpad & kDpadMask) != 0) {
    pointer_armed_ = false;
  }

  // scroll_bg to read for camera-aware pixel<->tile conversion: a mined
  // context rule's own scroll_bg (from a future table that pins one) wins;
  // exact mode falls back to kDefaultMapScrollBg rather than leaving scroll
  // at 0, since today's real table declares no context rules to pin it from
  // at all. The OAM fallback path keeps its old behaviour exactly -- no
  // scroll info when scroll_bg is unset -- by not falling back to the
  // default itself.
  const int effective_scroll_bg = (scroll_bg >= 0) ? scroll_bg : (exact_mode ? kDefaultMapScrollBg : -1);

  DebugFrame debug_frame;
  debug_frame.exact_mode = exact_mode;
  debug_frame.cursor_tile = cursor_tile;
  if (effective_scroll_bg >= 0) {
    debug_frame.scroll_x = backend_->read_io16(bg_hofs_reg(effective_scroll_bg));
    debug_frame.scroll_y = backend_->read_io16(bg_vofs_reg(effective_scroll_bg));
  }

  if (const PointerState* p = frame.primary_pointer()) {
    // Motion arms steering; it does not need to be sustained to keep it
    // armed. The game's cursor moves at best one cell every few frames, far
    // slower than the mouse crosses the screen, so requiring `moved` on every
    // frame (as opposed to just the frame that starts it) stranded the
    // cursor the instant the player stopped moving the mouse to look at it.
    if (!p->in_viewport) {
      pointer_armed_ = false;
    } else if (p->moved) {
      pointer_armed_ = true;
    }
    in.armed_pointer = pointer_armed_ && p->in_viewport;
    in.primary_edge = p->primary_edge;
    in.secondary_edge = p->secondary_edge;

    if (exact_mode) {
      // Convert the pointer's GBA screen pixel to the map tile the game
      // itself would land the cursor on: the map scrolls, so the pointer's
      // screen position is shifted by the camera offset before dividing
      // into 16 px tiles -- the same units the mined cursor tile is in.
      const int target_tile_x = (p->gba_x + debug_frame.scroll_x) / kTileSize;
      const int target_tile_y = (p->gba_y + debug_frame.scroll_y) / kTileSize;
      debug_frame.target_tile_x = target_tile_x;
      debug_frame.target_tile_y = target_tile_y;
      in.target_x = target_tile_x * kTileSize;
      in.target_y = target_tile_y * kTileSize;
    } else {
      in.target_x = p->gba_x;
      in.target_y = p->gba_y;
    }
  } else {
    pointer_armed_ = false;
  }

  if (exact_mode) {
    in.indicator_found = true;
    // Already an absolute map-tile position, not a screen sprite position,
    // so unlike the OAM path below, scroll must NOT be added again here:
    // in.scroll_x/in.scroll_y stay 0, which is exactly what PointerNav's
    // "world = indicator + scroll" step-detection needs -- the tile itself
    // already is the world position.
    in.indicator_x = cursor_tile.x * kTileSize;
    in.indicator_y = cursor_tile.y * kTileSize;
  } else {
    in.indicator_found = indicator_.found;
    // GBA OAM reports a sprite's top-left corner, but PointerNav wants the
    // indicator's *centre* so it can be compared directly against the pointer
    // target (a point, not a corner). The tracker now carries the sprite's
    // real decoded size, so this works for any OBJ shape/size -- not just the
    // 16x16 tile cursor most contexts use. Without it, steering settles with
    // the indicator's top-left corner under the target instead of its middle,
    // which on a small sprite (e.g. an 8x8 letter-grid selector) is a
    // systematic offset of a full cell or more.
    in.indicator_x = indicator_.screen_x + indicator_.width / 2;
    in.indicator_y = indicator_.screen_y + indicator_.height / 2;
    in.scroll_x = debug_frame.scroll_x;
    in.scroll_y = debug_frame.scroll_y;
  }

  NavOutput out = nav_.step(in);

  if (const PointerState* p = frame.primary_pointer()) {
    if (p->in_viewport) {
      if (p->secondary_edge) {
        out.keys |= kKeyB;
      }

      // Middle-click drag, Right-drag, or Touch-drag Camera Panning
      if (p->middle_down || (p->kind == PointerKind::Touch && p->primary_down)) {
        if (p->drag_dx < -1) out.keys |= kKeyRight;
        else if (p->drag_dx > 1) out.keys |= kKeyLeft;
        if (p->drag_dy < -1) out.keys |= kKeyDown;
        else if (p->drag_dy > 1) out.keys |= kKeyUp;
      }

      // RTS Edge Scrolling (when mouse is at screen border)
      if (!p->primary_down && !p->middle_down && on_map) {
        if (p->gba_x <= 1) out.keys |= kKeyLeft;
        else if (p->gba_x >= 238) out.keys |= kKeyRight;
        if (p->gba_y <= 1) out.keys |= kKeyUp;
        else if (p->gba_y >= 158) out.keys |= kKeyDown;
      }
    }
  }

  // Hidden-frame burst initialization for smooth steering fallback
  burst_remaining_ = 0;
  has_prev_step_tile_ = false;

  if (exact_mode && pointer_armed_ && steerable && cursor_tile.found) {
    const int dx_tiles = std::abs(debug_frame.target_tile_x - cursor_tile.x);
    const int dy_tiles = std::abs(debug_frame.target_tile_y - cursor_tile.y);
    const int total_steps = dx_tiles + dy_tiles;
    if (total_steps > 1) {
      burst_target_x_ = debug_frame.target_tile_x;
      burst_target_y_ = debug_frame.target_tile_y;
      burst_phase_ = 1;
      burst_remaining_ = std::min((total_steps - 1) * kBurstFramesPerStep + (kBurstFramesPerStep - 1), kMaxBurstFrames);
      prev_step_tile_x_ = cursor_tile.x;
      prev_step_tile_y_ = cursor_tile.y;
      has_prev_step_tile_ = true;
    }
  }

  // OamTracker's wildcard correlation locks onto "the sprite that keeps
  // moving the way the D-pad was pressed" -- but PointerNav only emits
  // D-pad bits once it already has a lock (its indicator_found gate in
  // PointerNav::step()). If last_emitted_dpad_ echoed only PointerNav's own
  // output, correlation could never receive a first nonzero signal and a
  // wildcard signature could never lock at all, from any input source, ever
  // -- an inert mouse forever, even with no symbol table. Folding in the
  // physical device's D-pad too means an ordinary keyboard/gamepad press,
  // which moves the real GBA cursor whether or not nav is steering, is what
  // correlation learns from on a first run with nothing mined.
  const std::uint16_t device_dpad = static_cast<std::uint16_t>(frame.device_dpad & kDpadMask);
  last_emitted_dpad_ = static_cast<std::uint16_t>((out.keys | device_dpad) & kDpadMask);

  if (debug_enabled_) {
    debug_log(in, out, debug_frame);
  }

  return out.keys;
}

void NavController::debug_log(const NavInput& in, const NavOutput& out, const DebugFrame& debug) {
  // ~once per second at 60 fps. frame_counter_ was already incremented for
  // this frame in update(), so frame 1 (the very first frame) logs too.
  if ((frame_counter_ % 60) != 1) return;

  const IndicatorSignature sig = tracker_.signature();
  std::cout << "[nav-debug] frame=" << frame_counter_
            << " ctx=" << context_name(context_)
            << " mode=" << (debug.exact_mode ? "exact" : "oam-fallback")
            << " locked=" << tracker_.locked()
            << " sig(tile=" << sig.tile << ",pal=" << sig.palette << ")"
            << " oam_index=" << indicator_.oam_index
            << " size=" << indicator_.width << "x" << indicator_.height
            << " cursor_tile(found=" << debug.cursor_tile.found
            << ",x=" << debug.cursor_tile.x << ",y=" << debug.cursor_tile.y << ")"
            << " target_tile(x=" << debug.target_tile_x << ",y=" << debug.target_tile_y << ")"
            << " scroll(x=" << debug.scroll_x << ",y=" << debug.scroll_y << ")"
            << " indicator(found=" << in.indicator_found
            << ",x=" << in.indicator_x << ",y=" << in.indicator_y << ")"
            << " target(armed=" << in.armed_pointer
            << ",x=" << in.target_x << ",y=" << in.target_y << ")"
            << " error(x=" << (in.target_x - in.indicator_x)
            << ",y=" << (in.target_y - in.indicator_y) << ")"
            << " steerable=" << in.steerable
            << " keys=0x" << std::hex << out.keys << std::dec
            << " phase(x=" << nav_.x_phase_name() << ",y=" << nav_.y_phase_name() << ")"
            << "\n";
}

std::uint16_t NavController::burst_step() {
  if (burst_remaining_ <= 0 || backend_ == nullptr || !backend_->available()) {
    burst_remaining_ = 0;
    return 0;
  }

  if (burst_phase_ == 0) {
    const CursorTile cur = read_cursor_tile(*backend_, context_probe_.table().cursor);
    if (!cur.found) {
      burst_remaining_ = 0;
      return 0;
    }

    const int dx = burst_target_x_ - cur.x;
    const int dy = burst_target_y_ - cur.y;
    if (dx == 0 && dy == 0) {
      burst_remaining_ = 0;
      return 0;
    }

    if (has_prev_step_tile_ && cur.x == prev_step_tile_x_ && cur.y == prev_step_tile_y_) {
      // Cursor did not move after the previous step (e.g. hit map boundary).
      burst_remaining_ = 0;
      return 0;
    }

    prev_step_tile_x_ = cur.x;
    prev_step_tile_y_ = cur.y;
    has_prev_step_tile_ = true;

    if (dx != 0) {
      burst_dir_ = (dx > 0) ? kKeyRight : kKeyLeft;
    } else {
      burst_dir_ = (dy > 0) ? kKeyDown : kKeyUp;
    }

    burst_phase_ = 1;
    burst_remaining_--;
    return burst_dir_;
  }

  if (burst_phase_ == 1) {
    burst_phase_ = 2;
    burst_remaining_--;
    return 0;
  }

  if (burst_phase_ == 2) {
    burst_phase_ = 3;
    burst_remaining_--;
    return 0;
  }

  // burst_phase_ == 3
  burst_phase_ = 0;
  burst_remaining_--;
  return 0;
}

}  // namespace aw
