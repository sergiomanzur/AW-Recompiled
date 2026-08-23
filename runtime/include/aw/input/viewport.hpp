#pragma once

namespace aw {

// Maps a window client coordinate into GBA screen space (0..239, 0..159).
// Returns false, leaving the outputs untouched, when the point lies outside
// the viewport or the viewport is degenerate.
//
// This is the transform formerly known as Window::client_to_gba. It lives in
// the input layer because every platform needs it and none of them need a
// window handle to compute it.
bool viewport_to_gba(int vp_x, int vp_y, int vp_width, int vp_height,
                     int client_x, int client_y,
                     int& out_gba_x, int& out_gba_y);

}  // namespace aw
