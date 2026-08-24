#include "aw/nav/nav_controller.hpp"

#include "aw/hardware.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>

namespace aw {

namespace {

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
}

void NavController::load_symbols(const std::string& rom_sha1) {
  const std::string path = "data/symbols/" + to_lower(rom_sha1) + ".ini";
  SymbolTable table;
  std::string err;
  if (!table.load_from_file(path, err)) {
    std::cout << "[nav] no symbol table (" << err
              << "); using correlation tracking\n";
    symbols_loaded_ = false;
    return;
  }
  if (!table.matches_rom(rom_sha1)) {
    std::cout << "[nav] symbol table ROM mismatch; using correlation tracking\n";
    symbols_loaded_ = false;
    return;
  }
  context_probe_.set_table(std::move(table));
  symbols_loaded_ = true;
  std::cout << "[nav] loaded symbol table " << path << "\n";
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

  // The OAM we are about to read is the result of last frame's keys, so
  // correlation is scored against last frame's emitted D-pad.
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
    in.target_x = p->gba_x;
    in.target_y = p->gba_y;
    in.primary_edge = p->primary_edge;
    in.secondary_edge = p->secondary_edge;
  } else {
    pointer_armed_ = false;
  }

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

  if (scroll_bg >= 0) {
    in.scroll_x = backend_->read_io16(bg_hofs_reg(scroll_bg));
    in.scroll_y = backend_->read_io16(bg_vofs_reg(scroll_bg));
  }

  const NavOutput out = nav_.step(in);

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
    debug_log(in, out);
  }

  return out.keys;
}

void NavController::debug_log(const NavInput& in, const NavOutput& out) {
  // ~once per second at 60 fps. frame_counter_ was already incremented for
  // this frame in update(), so frame 1 (the very first frame) logs too.
  if ((frame_counter_ % 60) != 1) return;

  const IndicatorSignature sig = tracker_.signature();
  std::cout << "[nav-debug] frame=" << frame_counter_
            << " ctx=" << context_name(context_)
            << " locked=" << tracker_.locked()
            << " sig(tile=" << sig.tile << ",pal=" << sig.palette << ")"
            << " oam_index=" << indicator_.oam_index
            << " size=" << indicator_.width << "x" << indicator_.height
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

}  // namespace aw
