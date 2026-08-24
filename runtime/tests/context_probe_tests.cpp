#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "aw/probe/context.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

// Writes `content` to `path` on construction and deletes it on destruction
// (including on exception unwind from a failed require_equal), so tests that
// exercise load_from_file's real INI parser leave nothing behind.
class TempIniFile {
public:
  TempIniFile(std::string path, const std::string& content) : path_(std::move(path)) {
    std::ofstream out(path_, std::ios::trunc);
    out << content;
  }
  ~TempIniFile() { std::remove(path_.c_str()); }

  const std::string& path() const { return path_; }

private:
  std::string path_;
};

// A ProbeBackend backed by a plain vector, so the probe is testable with no
// emulator.
class FakeBackend final : public aw::ProbeBackend {
public:
  FakeBackend() : ewram_(0x1000, 0) {}

  void poke(std::uint32_t addr, std::uint8_t value) {
    ewram_[addr - 0x02000000] = value;
  }

  bool available() override { return true; }
  const std::uint8_t* oam() override { return nullptr; }
  const std::uint8_t* ewram(std::size_t& size_out) override {
    size_out = ewram_.size();
    return ewram_.data();
  }
  std::uint16_t read_io16(std::uint32_t) override { return 0; }

private:
  std::vector<std::uint8_t> ewram_;
};

aw::SymbolTable two_context_table() {
  aw::SymbolTable table;
  table.rom_sha1 = "15053499D5B3F49128A941D7F2D84876F5424D0C";

  aw::ContextRule map;
  map.id = aw::ContextId::MapView;
  map.predicates.push_back({0x02000100, 0x03});
  map.signature = {0x040, -1};
  map.scroll_bg = 1;
  map.steerable = true;
  table.contexts.push_back(map);

  aw::ContextRule cutscene;
  cutscene.id = aw::ContextId::Cutscene;
  cutscene.predicates.push_back({0x02000100, 0x09});
  cutscene.steerable = false;
  table.contexts.push_back(cutscene);

  return table;
}

void tests_classifies_by_predicate() {
  aw::ContextProbe probe;
  probe.set_table(two_context_table());

  FakeBackend backend;
  backend.poke(0x02000100, 0x03);
  require_equal(probe.classify(backend) == aw::ContextId::MapView, true, "map view");

  backend.poke(0x02000100, 0x09);
  require_equal(probe.classify(backend) == aw::ContextId::Cutscene, true, "cutscene");
}

void tests_unmatched_value_is_unknown() {
  aw::ContextProbe probe;
  probe.set_table(two_context_table());

  FakeBackend backend;
  backend.poke(0x02000100, 0x77);
  require_equal(probe.classify(backend) == aw::ContextId::Unknown, true, "unmatched");
}

void tests_empty_table_is_unknown_but_steerable() {
  aw::ContextProbe probe;  // No table loaded at all.

  FakeBackend backend;
  require_equal(probe.classify(backend) == aw::ContextId::Unknown, true, "no table");

  // Unknown must remain steerable so correlation mode works with no symbols.
  const aw::ContextRule& rule = probe.rule_for(aw::ContextId::Unknown);
  require_equal(rule.steerable, true, "unknown is steerable");
  require_equal(rule.signature.wildcard(), true, "unknown uses wildcard signature");
}

void tests_rule_lookup_returns_the_matching_rule() {
  aw::ContextProbe probe;
  probe.set_table(two_context_table());

  const aw::ContextRule& map = probe.rule_for(aw::ContextId::MapView);
  require_equal(map.scroll_bg, 1, "map scroll bg");
  require_equal(map.signature.tile, 0x040, "map signature tile");

  const aw::ContextRule& cut = probe.rule_for(aw::ContextId::Cutscene);
  require_equal(cut.steerable, false, "cutscene not steerable");
}

void tests_rom_matching_is_case_insensitive() {
  aw::SymbolTable table = two_context_table();
  require_equal(table.matches_rom("15053499d5b3f49128a941d7f2d84876f5424d0c"), true, "lowercase");
  require_equal(table.matches_rom("15053499D5B3F49128A941D7F2D84876F5424D0C"), true, "uppercase");
  require_equal(table.matches_rom("D0A0A4CFE9B95AC7118F7EF476F014CA0242EB65"), false, "rev 0");
}

void tests_predicate_outside_ewram_never_matches() {
  aw::SymbolTable table;
  aw::ContextRule rule;
  rule.id = aw::ContextId::MapView;
  rule.predicates.push_back({0x02FFFFFF, 0x00});  // Beyond the fake's 0x1000 bytes
  table.contexts.push_back(rule);

  aw::ContextProbe probe;
  probe.set_table(table);

  FakeBackend backend;
  require_equal(probe.classify(backend) == aw::ContextId::Unknown, true, "out of range");
}

// Pre-populates `table` with sentinel content that is distinguishable from
// anything a real load would produce, so that an implementation which
// cleared the table early (before discovering the failure) would be caught
// rather than accidentally matching an already-empty starting state.
void populate_with_sentinel(aw::SymbolTable& table) {
  table.rom_sha1 = "SENTINEL0123456789SENTINEL0123456789SENT";

  aw::ContextRule sentinel;
  sentinel.id = aw::ContextId::FrontEnd;
  sentinel.predicates.push_back({0x02000999, 0x42});
  sentinel.scroll_bg = 3;
  sentinel.steerable = false;
  table.contexts.push_back(sentinel);

  table.cursor.x_addr = 0x03009999;
  table.cursor.y_addr = 0x0300999B;
}

void require_sentinel_unchanged(const aw::SymbolTable& table, const char* context) {
  std::ostringstream label;
  label << context << ": rom_sha1";
  require_equal(table.rom_sha1, std::string("SENTINEL0123456789SENTINEL0123456789SENT"), label.str().c_str());

  require_equal(table.contexts.size(), std::size_t(1), context);
  const aw::ContextRule& rule = table.contexts[0];
  require_equal(rule.id == aw::ContextId::FrontEnd, true, context);
  require_equal(rule.predicates.size(), std::size_t(1), context);
  require_equal(rule.predicates[0].addr, static_cast<std::uint32_t>(0x02000999), context);
  require_equal(static_cast<int>(rule.predicates[0].value), 0x42, context);
  require_equal(rule.scroll_bg, 3, context);
  require_equal(rule.steerable, false, context);

  require_equal(table.cursor.x_addr, static_cast<std::uint32_t>(0x03009999), context);
  require_equal(table.cursor.y_addr, static_cast<std::uint32_t>(0x0300999B), context);
}

void tests_missing_file_leaves_populated_table_unchanged() {
  aw::SymbolTable table;
  populate_with_sentinel(table);

  std::string err;
  const bool ok = table.load_from_file("data/symbols/definitely-not-here.ini", err);
  require_equal(ok, false, "missing file fails");
  require_equal(err.empty(), false, "error message set");

  // Atomicity: a failed load must not have touched the table at all, not
  // merely "the table is still empty" (which a table that started empty
  // would satisfy even if the implementation cleared-then-failed).
  require_sentinel_unchanged(table, "missing file");
}

void tests_file_with_no_contexts_leaves_populated_table_unchanged() {
  // A syntactically valid table (has [Rom] sha1) that declares no
  // recognised context sections. This exercises the "declares no contexts"
  // failure branch, distinct from "file missing" and "no sha1".
  TempIniFile ini("context_probe_tests_no_contexts.ini",
                  "[Rom]\n"
                  "sha1 = FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\n");

  aw::SymbolTable table;
  populate_with_sentinel(table);

  std::string err;
  const bool ok = table.load_from_file(ini.path(), err);
  require_equal(ok, false, "no-contexts file fails");
  require_equal(err.empty(), false, "error message set");

  require_sentinel_unchanged(table, "no-contexts file");
}

// A file with only [Rom] and [Cursor] -- no context sections at all -- must
// load successfully: the mined cursor addresses are useful on their own for
// exact-coordinate steering, with no context predicates required. This is
// the behaviour task 9f adds; tests_file_with_no_contexts_leaves_populated_
// table_unchanged (above) pins that a file with *neither* contexts nor a
// valid cursor still fails, so this does not weaken that guarantee.
void tests_cursor_only_file_loads_successfully() {
  TempIniFile ini("context_probe_tests_cursor_only.ini",
                  "[Rom]\n"
                  "sha1 = 15053499D5B3F49128A941D7F2D84876F5424D0C\n"
                  "\n"
                  "[Cursor]\n"
                  "x_addr = 50345636\n"  // 0x030036A4
                  "y_addr = 50345638\n"  // 0x030036A6
                  );

  aw::SymbolTable table;
  std::string err;
  const bool ok = table.load_from_file(ini.path(), err);
  require_equal(ok, true, "cursor-only file loads");
  require_equal(err.empty(), true, "no error on cursor-only success");

  require_equal(table.contexts.empty(), true, "cursor-only file declares no contexts");
  require_equal(table.cursor.valid(), true, "cursor addresses are valid");
  require_equal(table.cursor.x_addr, static_cast<std::uint32_t>(50345636), "cursor x_addr");
  require_equal(table.cursor.y_addr, static_cast<std::uint32_t>(50345638), "cursor y_addr");
}

// A [Cursor] section with only one of the two addresses set is not enough to
// steer by (CursorAddresses::valid() requires both), so with no context
// rules either the file must still be rejected -- the "declares neither"
// failure path must not be fooled by a half-populated Cursor section.
void tests_half_set_cursor_with_no_contexts_still_fails() {
  TempIniFile ini("context_probe_tests_half_cursor.ini",
                  "[Rom]\n"
                  "sha1 = FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\n"
                  "\n"
                  "[Cursor]\n"
                  "x_addr = 50345636\n");  // y_addr omitted -> 0 -> invalid pair

  aw::SymbolTable table;
  populate_with_sentinel(table);

  std::string err;
  const bool ok = table.load_from_file(ini.path(), err);
  require_equal(ok, false, "half-set cursor with no contexts fails");
  require_equal(err.empty(), false, "error message set");

  require_sentinel_unchanged(table, "half-set cursor file");
}

// Covers load_from_file's real INI parsing loop end to end, against the
// exact format documented in data/symbols/README.md. Every prior test either
// builds a SymbolTable directly in C++ or points load_from_file at a file
// that doesn't exist, so none of predicate_addr/predicate_value/
// predicate2_addr/predicate2_value/indicator_tile/indicator_palette/
// scroll_bg/steerable were ever exercised through the parser itself.
void tests_load_from_file_parses_documented_format() {
  // MapView: single predicate, both indicator fields set, scroll_bg set,
  // steerable explicitly disabled.
  // ListMenu: two predicates (order matters), all optional fields omitted
  // so their documented defaults (-1 wildcard, scroll_bg -1, steerable
  // true) must come through.
  // NameEntry: predicate_addr explicitly 0 -> must be skipped entirely.
  // FrontEnd / Cutscene: sections absent from the file entirely -> must
  // also be skipped (missing is equivalent to predicate_addr = 0).
  TempIniFile ini("context_probe_tests_full.ini",
                  "[Rom]\n"
                  "sha1 = 1234567890ABCDEF1234567890ABCDEF12345678\n"
                  "\n"
                  "[MapView]\n"
                  "predicate_addr = 33554944\n"   // 0x02000200
                  "predicate_value = 7\n"
                  "indicator_tile = 64\n"
                  "indicator_palette = 2\n"
                  "scroll_bg = 1\n"
                  "steerable = 0\n"
                  "\n"
                  "[ListMenu]\n"
                  "predicate_addr = 33555200\n"   // 0x02000300
                  "predicate_value = 5\n"
                  "predicate2_addr = 33555204\n"  // 0x02000304
                  "predicate2_value = 9\n"
                  "\n"
                  "[NameEntry]\n"
                  "predicate_addr = 0\n");

  aw::SymbolTable table;
  std::string err;
  const bool ok = table.load_from_file(ini.path(), err);
  require_equal(ok, true, "documented-format file loads");
  require_equal(err.empty(), true, "no error on success");

  // rom_sha1 round-trips and matches_rom accepts it case-insensitively.
  require_equal(table.rom_sha1, std::string("1234567890ABCDEF1234567890ABCDEF12345678"), "rom_sha1 round-trip");
  require_equal(table.matches_rom("1234567890abcdef1234567890abcdef12345678"), true, "matches_rom lowercase");
  require_equal(table.matches_rom("1234567890ABCDEF1234567890ABCDEF12345678"), true, "matches_rom uppercase");

  // Only MapView and ListMenu were declared; NameEntry (explicit 0) and the
  // entirely-absent FrontEnd/Cutscene sections must not appear.
  require_equal(table.contexts.size(), std::size_t(2), "two contexts parsed");

  const aw::ContextRule& map = table.contexts[0];
  require_equal(map.id == aw::ContextId::MapView, true, "contexts[0] is MapView");
  require_equal(map.predicates.size(), std::size_t(1), "MapView has exactly one predicate");
  require_equal(map.predicates[0].addr, static_cast<std::uint32_t>(33554944), "MapView predicate addr");
  require_equal(static_cast<int>(map.predicates[0].value), 7, "MapView predicate value");
  require_equal(map.signature.tile, 64, "MapView indicator tile");
  require_equal(map.signature.palette, 2, "MapView indicator palette");
  require_equal(map.scroll_bg, 1, "MapView scroll_bg");
  require_equal(map.steerable, false, "MapView steerable = 0 parses to false");

  const aw::ContextRule& list = table.contexts[1];
  require_equal(list.id == aw::ContextId::ListMenu, true, "contexts[1] is ListMenu");
  require_equal(list.predicates.size(), std::size_t(2), "ListMenu has two predicates");
  require_equal(list.predicates[0].addr, static_cast<std::uint32_t>(33555200), "ListMenu predicate 1 addr");
  require_equal(static_cast<int>(list.predicates[0].value), 5, "ListMenu predicate 1 value");
  require_equal(list.predicates[1].addr, static_cast<std::uint32_t>(33555204), "ListMenu predicate 2 addr");
  require_equal(static_cast<int>(list.predicates[1].value), 9, "ListMenu predicate 2 value");
  // Omitted optional fields fall back to their documented defaults.
  require_equal(list.signature.tile, -1, "ListMenu indicator tile defaults to wildcard");
  require_equal(list.signature.palette, -1, "ListMenu indicator palette defaults to wildcard");
  require_equal(list.scroll_bg, -1, "ListMenu scroll_bg defaults to -1");
  require_equal(list.steerable, true, "ListMenu steerable defaults to true when omitted");

  // No [Cursor] section in this file -> cursor addresses default to 0/0,
  // i.e. unknown, and the table must still have loaded successfully purely
  // on the strength of its two context rules.
  require_equal(table.cursor.x_addr, static_cast<std::uint32_t>(0), "cursor x_addr defaults to 0");
  require_equal(table.cursor.y_addr, static_cast<std::uint32_t>(0), "cursor y_addr defaults to 0");
  require_equal(table.cursor.valid(), false, "cursor defaults to invalid when section absent");
}

// classify() is documented to return the first rule in `contexts` whose
// predicates all match ("first matching rule wins"). The other fixtures use
// mutually-exclusive predicate values, so none of them actually exercise two
// rules matching the same state at once. This pins that ordering guarantee
// deliberately, rather than leaving it an accident of how the fixtures
// happen to be built.
void tests_first_matching_rule_wins() {
  aw::SymbolTable table;

  aw::ContextRule first;
  first.id = aw::ContextId::MapView;
  first.predicates.push_back({0x02000100, 0x05});
  table.contexts.push_back(first);

  aw::ContextRule second;
  second.id = aw::ContextId::Cutscene;
  second.predicates.push_back({0x02000100, 0x05});  // Same address and value as `first`.
  table.contexts.push_back(second);

  aw::ContextProbe probe;
  probe.set_table(table);

  FakeBackend backend;
  backend.poke(0x02000100, 0x05);

  require_equal(probe.classify(backend) == aw::ContextId::MapView, true, "first matching rule wins");
}

}  // namespace

int main() {
  try {
    tests_classifies_by_predicate();
    tests_unmatched_value_is_unknown();
    tests_empty_table_is_unknown_but_steerable();
    tests_rule_lookup_returns_the_matching_rule();
    tests_rom_matching_is_case_insensitive();
    tests_predicate_outside_ewram_never_matches();
    tests_missing_file_leaves_populated_table_unchanged();
    tests_file_with_no_contexts_leaves_populated_table_unchanged();
    tests_cursor_only_file_loads_successfully();
    tests_half_set_cursor_with_no_contexts_still_fails();
    tests_load_from_file_parses_documented_format();
    tests_first_matching_rule_wins();
    std::cout << "context_probe_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
