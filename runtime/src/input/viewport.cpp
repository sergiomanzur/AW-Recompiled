#include "aw/input/viewport.hpp"

#include <algorithm>

namespace aw {

bool viewport_to_gba(int vp_x, int vp_y, int vp_width, int vp_height,
                     int client_x, int client_y,
                     int& out_gba_x, int& out_gba_y) {
  if (vp_width <= 0 || vp_height <= 0) return false;
  if (client_x < vp_x || client_x >= vp_x + vp_width) return false;
  if (client_y < vp_y || client_y >= vp_y + vp_height) return false;

  out_gba_x = std::clamp(((client_x - vp_x) * 240) / vp_width, 0, 239);
  out_gba_y = std::clamp(((client_y - vp_y) * 160) / vp_height, 0, 159);
  return true;
}

}  // namespace aw
