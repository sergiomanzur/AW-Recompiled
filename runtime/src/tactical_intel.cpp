#include "aw/tactical_intel.hpp"

#include <algorithm>

namespace aw {

namespace {

// The mined map-cursor tile coordinates (IWRAM 0x030036A4/0x030036A6), the
// one game-state read this project has verified end to end (mined by
// aw-cursor-miner, recorded in data/symbols/<sha1>.ini, trusted daily by
// pointer steering). Everything else this class once "read" was fabricated;
// those fields stay invalid until their addresses are mined and verified.
constexpr std::uint32_t kCursorXAddr = 0x030036A4;
constexpr std::uint32_t kCursorYAddr = 0x030036A6;

struct UnitTypeSpec {
  const char* name;
};

const UnitTypeSpec kUnitTypes[] = {
  {"INFANTRY"},
  {"MECH"},
  {"RECON"},
  {"TANK"},
  {"MD TANK"},
  {"NEOTANK"},
  {"MEGATANK"},
  {"ANTI-AIR"},
  {"MISSILES"},
  {"ARTILLERY"},
  {"ROCKETS"},
  {"APC"},
  {"FIGHTER"},
  {"BOMBER"},
  {"B-COPTER"},
  {"T-COPTER"},
  {"BATTLESHIP"},
  {"CRUISER"},
  {"LANDER"},
  {"SUBMARINE"}
};

// Advance Wars 1 Base Damage Matrix Table [Attacker][Defender] in %
const int* base_damage_matrix() {
  static const int matrix[20][20] = {
    // INF, MCH, RCN, TNK, MDT, NEO, MEG,  AA, MIS, ART, RCK, APC, FIG, BMB, BCP, TCP, BSH, CRS, LND, SUB
    {  55,  45,  12,   5,   1,   1,   1,   5,  25,  15,  25,  14,   0,   0,   7,  30,   0,   0,   0,   0}, // Infantry
    {  65,  55,  85,  55,  15,  15,   5,  55,  85,  70,  85,  75,   0,   0,   9,  35,   0,   0,   0,   0}, // Mech
    {  70,  65,  35,  15,   5,   5,   1,  12,  55,  45,  55,  45,   0,   0,  10,  35,   0,   0,   0,   0}, // Recon
    {  75,  70,  85,  55,  15,  15,   5,  65,  85,  70,  85,  75,   0,   0,  10,  40,   1,   5,  10,   1}, // Tank
    { 105,  95, 105,  85,  55,  45,  25, 105, 105, 105, 105, 105,   0,   0,  12,  45,  10,  45,  35,  10}, // Md Tank
    { 125, 115, 125, 105,  75,  55,  35, 115, 125, 115, 125, 125,   0,   0,  22,  55,  15,  55,  45,  15}, // Neotank
    { 135, 125, 135, 115,  85,  65,  45, 125, 135, 125, 135, 135,   0,   0,  25,  65,  20,  65,  55,  20}, // Megatank
    { 105, 105,  60,  25,  10,   5,   1,  45,  55,  50,  55,  50,   0,   0, 105, 105,   0,   0,   0,   0}, // Anti-Air
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 100, 100, 120, 120,   0,   0,   0,   0}, // Missiles
    {  90,  85,  80,  70,  45,  40,  15,  75,  85,  75,  85,  80,   0,   0,  65, 100,  40,  65,  55,  40}, // Artillery
    {  95,  90,  90,  80,  55,  50,  25,  85,  90,  80,  90,  85,   0,   0,  75, 105,  55,  75,  65,  55}, // Rockets
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0}, // APC
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  55, 100, 100, 100,   0,   0,   0,   0}, // Fighter
    { 110, 110, 105,  95,  60,  55,  35,  95, 105, 105, 105, 105,   0,   0,   0,   0,  75,  85,  95,  85}, // Bomber
    {  75,  75,  55,  55,  25,  20,  10,  25,  65,  65,  65,  60,   0,   0,  65,  95,  25,  55,  55,  25}, // B-Copter
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0}, // T-Copter
    {  95,  90,  95,  80,  55,  50,  25,  85,  90,  85,  90,  85,   0,   0,  65, 105,  50,  95,  95,  50}, // Battleship
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  55,  65, 115, 115,  25,  25,  25,  85}, // Cruiser
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0}, // Lander
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  55,  25,  95,  55}  // Submarine
  };
  return &matrix[0][0];
}

int get_base_damage(int att_type, int def_type) {
  att_type = std::clamp(att_type, 0, 19);
  def_type = std::clamp(def_type, 0, 19);
  return base_damage_matrix()[att_type * 20 + def_type];
}

}  // namespace

const char* unit_type_name(int unit_type) {
  if (unit_type < 0 || unit_type >= 20) return "UNIT";
  return kUnitTypes[unit_type].name;
}

DamageForecast compute_forecast(int attacker_type, int defender_type,
                                int defender_hp, int defender_terrain_stars) {
  DamageForecast forecast;
  const int att_type = std::clamp(attacker_type, 0, 19);
  const int def_type = std::clamp(defender_type, 0, 19);
  const int hp = std::clamp(defender_hp, 1, 10);
  const int stars = std::clamp(defender_terrain_stars, 0, 4);

  forecast.valid = true;
  forecast.attacker_name = unit_type_name(att_type);
  forecast.defender_name = unit_type_name(def_type);
  forecast.defender_hp = hp;
  forecast.defender_terrain_stars = stars;

  const int base_dmg = get_base_damage(att_type, def_type);
  if (base_dmg <= 0) {
    forecast.min_damage = 0;
    forecast.max_damage = 0;
    forecast.counter_damage = 0;
    return forecast;
  }

  // AW1 maths: damage scales with the attacker's HP display value (10 for a
  // fresh unit, fed in via defender-style clamping by the caller when real
  // attacker HP becomes readable) and the defender's terrain stars.
  const int def_reduction = 100 - stars * 10;
  const int calc_dmg = (base_dmg * 10 * def_reduction) / 1000;
  forecast.min_damage = std::max(0, calc_dmg - 4);
  forecast.max_damage = std::min(100, calc_dmg + 6);

  const int counter_base = get_base_damage(def_type, att_type);
  const int remaining_def_hp = std::max(0, 10 - (calc_dmg / 10));
  forecast.counter_damage = (counter_base * remaining_def_hp * 9) / 100;
  return forecast;
}

void TacticalIntel::update(ProbeBackend& backend, ContextId /*context*/) {
  backend_ok_ = backend.available();

  // The one verified read: the mined cursor coordinates.
  const CursorTile tile = read_cursor_tile(
      backend, {kCursorXAddr, kCursorYAddr});
  cursor_valid_ = tile.found;
  cursor_x_ = tile.x;
  cursor_y_ = tile.y;

  // CO, funds, turn count and the unit table have no verified addresses
  // yet (see aw-boot-probe and data/symbols/README.md for the mining
  // workflow). Report invalid rather than fabricate.
  active_co_ = CoIntel{};
  selected_unit_ = UnitIntel{};
  hovered_unit_ = UnitIntel{};
  forecast_ = DamageForecast{};
}

}  // namespace aw
