#include "aw/probe/context.hpp"

#include "aw/config_file.hpp"

#include <algorithm>
#include <cctype>

namespace aw {

namespace {

constexpr std::uint32_t kEwramBase = 0x02000000;

std::string to_upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
}

ContextId context_from_name(const std::string& name) {
  if (name == "MapView") return ContextId::MapView;
  if (name == "ListMenu") return ContextId::ListMenu;
  if (name == "NameEntry") return ContextId::NameEntry;
  if (name == "FrontEnd") return ContextId::FrontEnd;
  if (name == "Cutscene") return ContextId::Cutscene;
  return ContextId::Unknown;
}

}  // namespace

const char* context_name(ContextId id) {
  switch (id) {
    case ContextId::MapView:   return "MapView";
    case ContextId::ListMenu:  return "ListMenu";
    case ContextId::NameEntry: return "NameEntry";
    case ContextId::FrontEnd:  return "FrontEnd";
    case ContextId::Cutscene:  return "Cutscene";
    case ContextId::Unknown:   break;
  }
  return "Unknown";
}

bool SymbolTable::matches_rom(const std::string& sha1) const {
  return !rom_sha1.empty() && to_upper(rom_sha1) == to_upper(sha1);
}

bool SymbolTable::load_from_file(const std::string& path, std::string& err) {
  ConfigFile file;
  if (!file.load(path)) {
    err = "cannot open symbol table: " + path;
    return false;
  }

  const std::string sha1 = file.get_string("Rom", "sha1", "");
  if (sha1.empty()) {
    err = "symbol table has no [Rom] sha1: " + path;
    return false;
  }

  SymbolTable parsed;
  parsed.rom_sha1 = sha1;

  static const char* kNames[] = {"MapView", "ListMenu", "NameEntry", "FrontEnd", "Cutscene"};
  for (const char* name : kNames) {
    // A context is present when it declares at least a predicate address.
    const int addr = file.get_int(name, "predicate_addr", 0);
    if (addr == 0) continue;

    ContextRule rule;
    rule.id = context_from_name(name);
    rule.predicates.push_back({static_cast<std::uint32_t>(addr),
                               static_cast<std::uint8_t>(file.get_int(name, "predicate_value", 0))});

    const int addr2 = file.get_int(name, "predicate2_addr", 0);
    if (addr2 != 0) {
      rule.predicates.push_back({static_cast<std::uint32_t>(addr2),
                                 static_cast<std::uint8_t>(file.get_int(name, "predicate2_value", 0))});
    }

    rule.signature.tile = file.get_int(name, "indicator_tile", -1);
    rule.signature.palette = file.get_int(name, "indicator_palette", -1);
    rule.scroll_bg = file.get_int(name, "scroll_bg", -1);
    rule.steerable = file.get_int(name, "steerable", 1) != 0;
    parsed.contexts.push_back(rule);
  }

  if (parsed.contexts.empty()) {
    err = "symbol table declares no contexts: " + path;
    return false;
  }

  *this = std::move(parsed);
  return true;
}

void ContextProbe::set_table(SymbolTable table) {
  table_ = std::move(table);
}

ContextId ContextProbe::classify(ProbeBackend& backend) const {
  if (table_.contexts.empty()) return ContextId::Unknown;

  std::size_t size = 0;
  const std::uint8_t* ewram = backend.ewram(size);
  if (ewram == nullptr || size == 0) return ContextId::Unknown;

  for (const ContextRule& rule : table_.contexts) {
    if (rule.predicates.empty()) continue;

    bool all_match = true;
    for (const ContextRule::Predicate& p : rule.predicates) {
      if (p.addr < kEwramBase) { all_match = false; break; }
      const std::uint32_t offset = p.addr - kEwramBase;
      if (offset >= size) { all_match = false; break; }
      if (ewram[offset] != p.value) { all_match = false; break; }
    }
    if (all_match) return rule.id;
  }
  return ContextId::Unknown;
}

const ContextRule& ContextProbe::rule_for(ContextId id) const {
  for (const ContextRule& rule : table_.contexts) {
    if (rule.id == id) return rule;
  }
  return default_rule_;
}

}  // namespace aw
