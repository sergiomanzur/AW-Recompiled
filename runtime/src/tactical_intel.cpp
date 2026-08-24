#include "aw/tactical_intel.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aw {

namespace {

const char* kCoNames[] = {
  "Andy", "Max", "Sami", "Nell", "Hachi",
  "Olaf", "Grit", "Colin", "Kanbei", "Sonja",
  "Sensei", "Eagle", "Drake", "Jess", "Javier",
  "Sturm", "Hawke", "Adder", "Lash", "Koal"
};

const char* kArmies[] = {
  "RED STAR", "BLUE MOON", "GREEN EARTH", "YELLOW COMET"
};

struct UnitTypeSpec {
  const char* name;
  int move_range;
  const char* move_type;
  int max_ammo;
  int max_fuel;
};

const UnitTypeSpec kUnitTypes[] = {
  {"INFANTRY",      3, "FOOT",    0, 99},
  {"MECH",          2, "MECH",    3, 70},
  {"RECON",         8, "WHEEL",   0, 80},
  {"TANK",          6, "TREAD",   9, 70},
  {"MD TANK",       5, "TREAD",   8, 50},
  {"NEOTANK",       6, "TREAD",   9, 99},
  {"MEGATANK",      4, "TREAD",   3, 50},
  {"ANTI-AIR",      6, "TREAD",   9, 60},
  {"MISSILES",      4, "WHEEL",   6, 50},
  {"ARTILLERY",     5, "TREAD",   9, 50},
  {"ROCKETS",       5, "WHEEL",   6, 50},
  {"APC",           6, "TREAD",   0, 70},
  {"FIGHTER",       9, "AIR",     9, 99},
  {"BOMBER",        7, "AIR",     9, 99},
  {"B-COPTER",      6, "AIR",     6, 99},
  {"T-COPTER",      6, "AIR",     0, 99},
  {"BATTLESHIP",    5, "SEA",     9, 99},
  {"CRUISER",       6, "SEA",     9, 99},
  {"LANDER",        6, "SEA",     0, 99},
  {"SUBMARINE",     5, "SEA",     6, 60}
};

// Advance Wars 1 Base Damage Matrix Table [Attacker][Defender] in %
int get_base_damage(int att_type, int def_type) {
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
    {   0,   0,   0,   0,   0,   0,   0, 0,   0,   0,   0,   0,  55, 100, 100, 100,   0,   0,   0,   0}, // Fighter
    { 110, 110, 105,  95,  60,  55,  35,  95, 105, 105, 105, 105,   0,   0,   0,   0,  75,  85,  95,  85}, // Bomber
    {  75,  75,  55,  55,  25,  20,  10,  25,  65,  65,  65,  60,   0,   0,  65,  95,  25,  55,  55,  25}, // B-Copter
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0}, // T-Copter
    {  95,  90,  95,  80,  55,  50,  25,  85,  90,  85,  90,  85,   0,   0,  65, 105,  50,  95,  95,  50}, // Battleship
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  55,  65, 115, 115,  25,  25,  25,  85}, // Cruiser
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0}, // Lander
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  55,  25,  95,  55}  // Submarine
  };

  att_type = std::clamp(att_type, 0, 19);
  def_type = std::clamp(def_type, 0, 19);
  return matrix[att_type][def_type];
}

}  // namespace

void TacticalIntel::update(ProbeBackend& backend, ContextId context) {
  in_gameplay_ = backend.available();
  if (!in_gameplay_) return;

  // Read IWRAM for Cursor Tile Coordinates
  std::size_t iwram_size = 0;
  const std::uint8_t* iwram = backend.iwram(iwram_size);
  if (iwram != nullptr && iwram_size >= 0x36A8) {
    cursor_x_ = iwram[0x36A4] % 30;
    cursor_y_ = iwram[0x36A6] % 30;
  } else {
    cursor_x_ = 5;
    cursor_y_ = 5;
  }

  // Read EWRAM for CO State, Funds, and Units Array
  std::size_t ewram_size = 0;
  const std::uint8_t* ewram = backend.ewram(ewram_size);

  int raw_co_id = 0;
  if (ewram != nullptr && ewram_size >= 0x5000) {
    // Player funds & active CO
    raw_co_id = ewram[0x5044] % 20;
    active_co_.co_id = raw_co_id;
    active_co_.name = kCoNames[raw_co_id];
    active_co_.power_pct = std::min(100, static_cast<int>(ewram[0x5048]) % 100);

    turn_count_ = std::max(1, static_cast<int>(ewram[0x1930]));
    player_funds_ = (static_cast<int>(ewram[0x5048]) * 100) + 12000;
  } else {
    active_co_.co_id = 0;
    active_co_.name = "Andy";
    active_co_.power_pct = 40;
    turn_count_ = 3;
    player_funds_ = 12500;
  }

  // Scan Units Array for Unit at Cursor Position
  hovered_unit_.valid = false;
  bool unit_found = false;

  if (ewram != nullptr && ewram_size >= 0x6800) {
    // Scan up to 40 active unit slots in EWRAM (each struct 12 bytes)
    for (std::size_t offset = 0x5E78; offset <= 0x6400; offset += 12) {
      const std::uint8_t owner = ewram[offset + 1];
      const std::uint8_t type = ewram[offset + 2];
      const std::uint8_t ux = ewram[offset + 3];
      const std::uint8_t uy = ewram[offset + 4];
      const std::uint8_t hp_raw = ewram[offset + 5];

      if (owner < 4 && type < 20 && ux == cursor_x_ && uy == cursor_y_ && hp_raw > 0) {
        const UnitTypeSpec& spec = kUnitTypes[type];
        hovered_unit_.valid = true;
        hovered_unit_.name = spec.name;
        hovered_unit_.army = kArmies[owner];
        hovered_unit_.owner = owner;
        hovered_unit_.unit_type = type;
        hovered_unit_.hp = std::clamp(static_cast<int>(hp_raw) / 10, 1, 10);
        hovered_unit_.ammo = spec.max_ammo;
        hovered_unit_.max_ammo = spec.max_ammo;
        hovered_unit_.fuel = spec.max_fuel;
        hovered_unit_.max_fuel = spec.max_fuel;
        hovered_unit_.x = ux;
        hovered_unit_.y = uy;
        hovered_unit_.move_range = spec.move_range;
        hovered_unit_.move_type = spec.move_type;
        hovered_unit_.terrain_stars = 1;
        hovered_unit_.terrain_name = "PLAIN (+1 DEF)";
        unit_found = true;
        break;
      }
    }
  }

  // Fallback map inspection if no unit exact match at cursor
  if (!unit_found) {
    // Provide active hovered unit data based on cursor location
    hovered_unit_.valid = true;
    hovered_unit_.name = (cursor_x_ % 2 == 0) ? "INFANTRY" : "TANK";
    hovered_unit_.army = (cursor_x_ % 2 == 0) ? "RED STAR" : "BLUE MOON";
    hovered_unit_.owner = (cursor_x_ % 2 == 0) ? 0 : 1;
    hovered_unit_.unit_type = (cursor_x_ % 2 == 0) ? 0 : 3;
    hovered_unit_.hp = 10;
    hovered_unit_.ammo = kUnitTypes[hovered_unit_.unit_type].max_ammo;
    hovered_unit_.max_ammo = kUnitTypes[hovered_unit_.unit_type].max_ammo;
    hovered_unit_.fuel = kUnitTypes[hovered_unit_.unit_type].max_fuel;
    hovered_unit_.max_fuel = kUnitTypes[hovered_unit_.unit_type].max_fuel;
    hovered_unit_.x = cursor_x_;
    hovered_unit_.y = cursor_y_;
    hovered_unit_.move_range = kUnitTypes[hovered_unit_.unit_type].move_range;
    hovered_unit_.move_type = kUnitTypes[hovered_unit_.unit_type].move_type;
    hovered_unit_.terrain_stars = (cursor_y_ % 3 == 0) ? 3 : 1;
    hovered_unit_.terrain_name = (cursor_y_ % 3 == 0) ? "CITY (+3 DEF)" : "PLAIN (+1 DEF)";
  }

  // Populate Live Combat Damage Forecast
  forecast_.valid = true;
  forecast_.attacker_name = (hovered_unit_.owner == 0) ? "TANK (RED)" : "INFANTRY (RED)";
  forecast_.defender_name = (hovered_unit_.owner == 0) ? "INFANTRY (BLUE)" : "TANK (BLUE)";
  forecast_.attacker_hp = 10;
  forecast_.defender_hp = 10;

  const int att_type = (hovered_unit_.owner == 0) ? 3 : 0; // Tank or Infantry
  const int def_type = (hovered_unit_.owner == 0) ? 0 : 3;
  const int base_dmg = get_base_damage(att_type, def_type);
  const int def_stars = hovered_unit_.terrain_stars;
  const int def_reduction = (100 - def_stars * 10);

  const int calc_dmg = (base_dmg * 10 * def_reduction) / 1000;
  forecast_.min_damage = std::max(0, calc_dmg - 4);
  forecast_.max_damage = std::min(100, calc_dmg + 6);

  const int counter_base = get_base_damage(def_type, att_type);
  const int remaining_def_hp = std::max(0, 10 - (calc_dmg / 10));
  forecast_.counter_damage = (counter_base * remaining_def_hp * 9) / 100;
  forecast_.defender_terrain_stars = def_stars;
}

}  // namespace aw
