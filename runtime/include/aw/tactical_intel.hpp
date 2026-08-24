#pragma once

#include "aw/probe/backend.hpp"
#include "aw/probe/context.hpp"
#include <cstdint>
#include <string>

namespace aw {

struct CoIntel {
  std::string name = "Andy";
  int co_id = 0;
  int power_pct = 0;
  bool super_power_active = false;
};

struct UnitIntel {
  bool valid = false;
  std::string name = "Infantry";
  std::string army = "RED STAR";
  int owner = 0;
  int unit_type = 0;
  int hp = 10;
  int ammo = 99;
  int max_ammo = 99;
  int fuel = 99;
  int max_fuel = 99;
  int x = 0;
  int y = 0;
  int move_range = 3;
  std::string move_type = "FOOT";
  int terrain_stars = 1;
  std::string terrain_name = "PLAIN";
};

struct DamageForecast {
  bool valid = false;
  std::string attacker_name = "Tank";
  std::string defender_name = "Mech";
  int attacker_hp = 10;
  int defender_hp = 10;
  int min_damage = 55;
  int max_damage = 65;
  int counter_damage = 15;
  int defender_terrain_stars = 2;
};

class TacticalIntel {
public:
  void update(ProbeBackend& backend, ContextId context);

  bool in_gameplay() const { return in_gameplay_; }
  const CoIntel& active_co() const { return active_co_; }
  const UnitIntel& selected_unit() const { return selected_unit_; }
  const UnitIntel& hovered_unit() const { return hovered_unit_; }
  const DamageForecast& forecast() const { return forecast_; }
  int turn_count() const { return turn_count_; }
  int player_funds() const { return player_funds_; }
  int cursor_x() const { return cursor_x_; }
  int cursor_y() const { return cursor_y_; }

private:
  bool in_gameplay_ = false;
  CoIntel active_co_{};
  UnitIntel selected_unit_{};
  UnitIntel hovered_unit_{};
  DamageForecast forecast_{};
  int turn_count_ = 1;
  int player_funds_ = 1000;
  int cursor_x_ = 0;
  int cursor_y_ = 0;
};

}  // namespace aw
