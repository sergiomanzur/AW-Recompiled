#pragma once

#include "aw/input/input_frame.hpp"
#include "aw/nav/pointer_nav.hpp"
#include "aw/probe/backend.hpp"
#include "aw/probe/context.hpp"
#include "aw/probe/cursor_probe.hpp"
#include "aw/probe/oam_tracker.hpp"

#include <cstdint>
#include <string>

namespace aw {

// Owns the context probe, tracker and steering controller, and runs them in
// the right order once per frame. This is the only piece that knows the
// frame ordering: the tracker must see the OAM that resulted from the keys
// we emitted last frame, so update() correlates against the previous
// frame's output.
//
// Deliberately holds a ProbeBackend* rather than a concrete backend: this
// header (and nav_controller.cpp) must stay free of mGBA and Windows
// dependencies so Spec 2/3 can reuse it unchanged on SDL and Android. The
// caller (main.cpp today) owns the concrete backend, calls its set_core()
// itself, and hands it here via set_backend().
class NavController {
public:
  // Must be called at least once before update(). The referenced backend
  // must remain valid; after a ROM switch the caller re-resolves the
  // backend's block pointers (backend.set_core(core)) and should call
  // NavController::reset() so tracking doesn't survive across cores.
  void set_backend(ProbeBackend& backend) { backend_ = &backend; }

  void reset();

  // Loads data/symbols/<rom_sha1>.ini if present. Missing or mismatched is
  // fine: tracking falls back to correlation and steering still works.
  void load_symbols(const std::string& rom_sha1);

  // Returns extra GBA keys to OR into the frame's keys before running the
  // emulator. Call once per frame, after the input sources have polled.
  std::uint16_t update(const InputFrame& frame);

  ContextId context() const { return context_; }

  // The tracker's current view of the game's selection indicator. `.found`
  // reports whether the tracker is currently reporting a position at all.
  Indicator indicator() const { return indicator_; }

private:
  // Extra bits the debug line reports beyond NavInput/NavOutput: which mode
  // steered this frame, the raw mined tile and computed target tile, and the
  // raw BG scroll values used to get there. Bundled into one struct rather
  // than a long debug_log() parameter list.
  struct DebugFrame {
    bool exact_mode = false;
    CursorTile cursor_tile{};
    int target_tile_x = 0;
    int target_tile_y = 0;
    int scroll_x = 0;
    int scroll_y = 0;
  };

  void debug_log(const NavInput& in, const NavOutput& out, const DebugFrame& debug);

  // Logs, once, which steering mode load_symbols() just resolved to. Split
  // out so every load_symbols() return path (no file, ROM mismatch, loaded)
  // reports the same way instead of duplicating the if/else three times.
  void log_steering_mode() const;

  ProbeBackend* backend_ = nullptr;
  ContextProbe context_probe_;
  OamTracker tracker_;
  PointerNav nav_;

  ContextId context_ = ContextId::Unknown;
  // The context classified last frame. Used to detect a context change so
  // the tracker is only reset/re-signed at the transition, not every frame.
  ContextId prev_context_ = ContextId::Unknown;
  Indicator indicator_{};
  std::uint16_t last_emitted_dpad_ = 0;
  bool symbols_loaded_ = false;

  // Latches true while the pointer is arming steering: set on a frame where
  // the primary pointer is in-viewport and moved, held true across
  // subsequent motionless frames (motion arms, it need not sustain), and
  // cleared when the pointer leaves the viewport, disappears, a physical
  // D-pad is held, or on reset(). See update() for the full rationale.
  bool pointer_armed_ = false;

  // AW_NAV_DEBUG=1 logs a one-line status roughly once per second. Kept in
  // the shipped binary, off by default: a real run can be diagnosed without
  // a rebuild.
  bool debug_checked_ = false;
  bool debug_enabled_ = false;
  std::uint64_t frame_counter_ = 0;
};

}  // namespace aw
