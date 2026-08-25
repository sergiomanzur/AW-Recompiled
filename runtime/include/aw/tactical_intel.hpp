#pragma once

#include "aw/probe/backend.hpp"
#include "aw/probe/context.hpp"
#include <cstdint>
#include <string>

namespace aw {

struct CoIntel {
  bool valid = false;  // False until a CO-state address is mined and verified
  std::string name;
  int co_id = 0;
  int power_pct = 0;
  bool super_power_active = false;
};

struct UnitIntel {
  bool valid = false;  // False until the unit table is mined and verified
  std::string name;
  std::string army;
  int owner = 0;
  int unit_type = 0;
  int hp = 0;
  int ammo = 0;
  int max_ammo = 0;
  int fuel = 0;
  int max_fuel = 0;
  int x = 0;
  int y = 0;
  int move_range = 0;
  std::string move_type;
  int terrain_stars = 0;
  std::string terrain_name;
};

struct DamageForecast {
  bool valid = false;
  std::string attacker_name;
  std::string defender_name;
  int attacker_hp = 0;
  int defender_hp = 0;
  int min_damage = 0;
  int max_damage = 0;
  int counter_damage = 0;
  int defender_terrain_stars = 0;
};

// Pure damage calculator over the Advance Wars base damage matrix. Inputs
// are unit type indices (0..19) and the defender's terrain stars. This is
// the extension point for the sidebar's combat forecast: once the selected
// and hovered units are readable from verified addresses, the caller feeds
// them here and gets honest numbers out.
DamageForecast compute_forecast(int attacker_type, int defender_type,
                                int defender_hp, int defender_terrain_stars);

// The name of unit type index 0..19, or "UNIT" when out of range.
const char* unit_type_name(int unit_type);

// Live view of readable game state. Everything here reports valid=false
// rather than inventing values: the only reads this class trusts are the
// mined map-cursor coordinates (IWRAM 0x030036A4/0x030036A6, see
// data/symbols/README.md). CO/funds/turn/unit fields light up once their
// addresses are mined and recorded in the symbol table.
class TacticalIntel {
public:
  void update(ProbeBackend& backend, ContextId context);

  bool in_gameplay() const { return backend_ok_; }
  const CoIntel& active_co() const { return active_co_; }
  const UnitIntel& selected_unit() const { return selected_unit_; }
  const UnitIntel& hovered_unit() const { return hovered_unit_; }
  const DamageForecast& forecast() const { return forecast_; }

  bool cursor_valid() const { return cursor_valid_; }
  int cursor_x() const { return cursor_x_; }
  int cursor_y() const { return cursor_y_; }

private:
  bool backend_ok_ = false;
  CoIntel active_co_{};
  UnitIntel selected_unit_{};
  UnitIntel hovered_unit_{};
  DamageForecast forecast_{};
  bool cursor_valid_ = false;
  int cursor_x_ = 0;
  int cursor_y_ = 0;
};

}  // namespace aw
