#pragma once

#include "aw/probe/backend.hpp"
#include "aw/probe/cursor_probe.hpp"
#include "aw/probe/oam_tracker.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aw {

enum class ContextId : std::uint8_t {
  Unknown,   // Not recognised: correlation tracking, steering allowed
  MapView,   // The gameplay tile grid
  ListMenu,  // Unit action menus, in-game menu, options
  NameEntry, // Letter grid
  FrontEnd,  // Title, CO select, map select, save slots
  Cutscene,  // Dialogue and animation: clicks only, no steering
};

const char* context_name(ContextId id);

// How to drive one UI context. Everything here is mined data, not code, so
// adding a context costs a data edit rather than a rebuild.
struct ContextRule {
  struct Predicate {
    std::uint32_t addr = 0;  // Absolute EWRAM address
    std::uint8_t value = 0;
  };

  ContextId id = ContextId::Unknown;
  std::vector<Predicate> predicates;  // All must match
  IndicatorSignature signature{};     // Wildcard means "use correlation"
  int scroll_bg = -1;                 // BG layer tracking content, -1 = none
  bool steerable = true;
};

struct SymbolTable {
  std::string rom_sha1;
  std::vector<ContextRule> contexts;

  // The map cursor's tile-coordinate addresses, mined by aw-cursor-miner.
  // Unset (x_addr/y_addr both 0) when this ROM revision has no mined
  // cursor addresses yet; NavController then falls back to OamTracker.
  CursorAddresses cursor;

  // Loads the INI form described in data/symbols/README.md. Returns false and
  // sets `err` on failure, leaving the table untouched. A file is valid as
  // long as it declares at least one context rule or valid cursor addresses
  // -- either alone is useful, so neither is required for the other.
  bool load_from_file(const std::string& path, std::string& err);

  bool matches_rom(const std::string& sha1) const;
};

// Classifies the active UI context by evaluating mined predicates against
// EWRAM. With no table, everything is Unknown, which is steerable with a
// wildcard signature so correlation mode still works.
class ContextProbe {
public:
  void set_table(SymbolTable table);
  const SymbolTable& table() const { return table_; }

  ContextId classify(ProbeBackend& backend) const;

  // Never returns null; unmatched ids resolve to a permissive default.
  const ContextRule& rule_for(ContextId id) const;

private:
  SymbolTable table_;
  ContextRule default_rule_{};  // Unknown, wildcard signature, steerable
};

}  // namespace aw
