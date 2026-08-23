#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
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

void tests_missing_file_reports_error_and_leaves_table_empty() {
  aw::SymbolTable table;
  std::string err;
  const bool ok = table.load_from_file("data/symbols/definitely-not-here.ini", err);
  require_equal(ok, false, "missing file fails");
  require_equal(err.empty(), false, "error message set");
  require_equal(table.contexts.empty(), true, "no contexts loaded");
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
    tests_missing_file_reports_error_and_leaves_table_empty();
    std::cout << "context_probe_tests passed!\n";
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
