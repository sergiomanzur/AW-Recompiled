#include "aw/window.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <xinput.h>
#pragma comment(lib, "xinput.lib")
#endif

#include <algorithm>
#include <cmath>
#include <iostream>

namespace aw {

ViewportRect calculate_viewport_rect(int client_width, int client_height, AspectRatio ratio) {
  if (client_width <= 0 || client_height <= 0) {
    return {0, 0, 0, 0};
  }

  if (ratio == AspectRatio::Stretch) {
    return {0, 0, client_width, client_height};
  }

  double target_aspect = 3.0 / 2.0;
  switch (ratio) {
    case AspectRatio::Original_3_2:  target_aspect = 3.0 / 2.0; break;
    case AspectRatio::Ratio_4_3:     target_aspect = 4.0 / 3.0; break;
    case AspectRatio::Ratio_16_9:    target_aspect = 16.0 / 9.0; break;
    case AspectRatio::Ratio_21_9:    target_aspect = 21.0 / 9.0; break;
    case AspectRatio::Ratio_21_10:   target_aspect = 21.0 / 10.0; break;
    case AspectRatio::Stretch:      break;
  }

  const double client_aspect = static_cast<double>(client_width) / static_cast<double>(client_height);

  int vp_width = 0;
  int vp_height = 0;
  int vp_x = 0;
  int vp_y = 0;

  if (client_aspect > target_aspect) {
    // Window is wider than target aspect ratio -> Pillarboxing (black margins on left & right)
    vp_height = client_height;
    vp_width = static_cast<int>(client_height * target_aspect + 0.5);
    vp_x = (client_width - vp_width) / 2;
    vp_y = 0;
  } else {
    // Window is taller than target aspect ratio -> Letterboxing (black margins on top & bottom)
    vp_width = client_width;
    vp_height = static_cast<int>(client_width / target_aspect + 0.5);
    vp_x = 0;
    vp_y = (client_height - vp_height) / 2;
  }

  return {vp_x, vp_y, vp_width, vp_height};
}

#ifdef _WIN32

namespace {

constexpr UINT IDM_FILE_OPEN            = 1001;
constexpr UINT IDM_FILE_EXIT            = 1002;
constexpr UINT IDM_ASPECT_3_2           = 2001;
constexpr UINT IDM_ASPECT_4_3           = 2002;
constexpr UINT IDM_ASPECT_16_9          = 2003;
constexpr UINT IDM_ASPECT_21_9          = 2004;
constexpr UINT IDM_ASPECT_21_10         = 2005;
constexpr UINT IDM_ASPECT_STRETCH       = 2006;
constexpr UINT IDM_RES_NATIVE           = 2101;
constexpr UINT IDM_RES_720P             = 2102;
constexpr UINT IDM_RES_1080P            = 2103;
constexpr UINT IDM_RES_4K               = 2104;
constexpr UINT IDM_FILTER_NEAREST       = 2201;
constexpr UINT IDM_FILTER_BILINEAR      = 2202;
constexpr UINT IDM_FILTER_SCALE2X        = 2203;
constexpr UINT IDM_SETTINGS_SELECT_ROM  = 3001;
constexpr UINT IDM_SETTINGS_CONTROLS    = 3002;
constexpr UINT IDM_HELP_CONTROLS        = 4001;
constexpr UINT IDM_HELP_ABOUT           = 4002;

void apply_scale2x(const std::uint32_t* src, std::uint32_t* dst, int w, int h) {
  for (int y = 0; y < h; ++y) {
    const int ym1 = (y > 0) ? (y - 1) : 0;
    const int yp1 = (y < h - 1) ? (y + 1) : (h - 1);
    for (int x = 0; x < w; ++x) {
      const int xm1 = (x > 0) ? (x - 1) : 0;
      const int xp1 = (x < w - 1) ? (x + 1) : (w - 1);

      const std::uint32_t B = src[ym1 * w + x];
      const std::uint32_t D = src[y * w + xm1];
      const std::uint32_t E = src[y * w + x];
      const std::uint32_t F = src[y * w + xp1];
      const std::uint32_t H = src[yp1 * w + x];

      std::uint32_t E0 = E, E1 = E, E2 = E, E3 = E;
      if (B != H && D != F) {
        E0 = (D == B) ? D : E;
        E1 = (B == F) ? F : E;
        E2 = (D == H) ? D : E;
        E3 = (H == F) ? F : E;
      }

      const int out_x = x * 2;
      const int out_y = y * 2;
      const int out_w = w * 2;

      dst[out_y * out_w + out_x]       = E0;
      dst[out_y * out_w + out_x + 1]   = E1;
      dst[(out_y + 1) * out_w + out_x] = E2;
      dst[(out_y + 1) * out_w + out_x + 1] = E3;
    }
  }
}

void apply_bilinear_2x(const std::uint32_t* src, std::uint32_t* dst, int w, int h) {
  const int out_w = w * 2;
  const int out_h = h * 2;
  for (int y = 0; y < out_h; ++y) {
    const float src_y = (y + 0.5f) * 0.5f - 0.5f;
    const int y0 = (src_y < 0) ? 0 : ((static_cast<int>(src_y) < h - 1) ? static_cast<int>(src_y) : h - 1);
    const int y1 = (y0 + 1 < h) ? (y0 + 1) : (h - 1);
    const float fy = src_y - y0;

    for (int x = 0; x < out_w; ++x) {
      const float src_x = (x + 0.5f) * 0.5f - 0.5f;
      const int x0 = (src_x < 0) ? 0 : ((static_cast<int>(src_x) < w - 1) ? static_cast<int>(src_x) : w - 1);
      const int x1 = (x0 + 1 < w) ? (x0 + 1) : (w - 1);
      const float fx = src_x - x0;

      const std::uint32_t c00 = src[y0 * w + x0];
      const std::uint32_t c10 = src[y0 * w + x1];
      const std::uint32_t c01 = src[y1 * w + x0];
      const std::uint32_t c11 = src[y1 * w + x1];

      const float w00 = (1.0f - fx) * (1.0f - fy);
      const float w10 = fx * (1.0f - fy);
      const float w01 = (1.0f - fx) * fy;
      const float w11 = fx * fy;

      const std::uint32_t r00 = (c00 >> 16) & 0xFF, g00 = (c00 >> 8) & 0xFF, b00 = c00 & 0xFF;
      const std::uint32_t r10 = (c10 >> 16) & 0xFF, g10 = (c10 >> 8) & 0xFF, b10 = c10 & 0xFF;
      const std::uint32_t r01 = (c01 >> 16) & 0xFF, g01 = (c01 >> 8) & 0xFF, b01 = c01 & 0xFF;
      const std::uint32_t r11 = (c11 >> 16) & 0xFF, g11 = (c11 >> 8) & 0xFF, b11 = c11 & 0xFF;

      const auto r = static_cast<std::uint32_t>(r00 * w00 + r10 * w10 + r01 * w01 + r11 * w11 + 0.5f);
      const auto g = static_cast<std::uint32_t>(g00 * w00 + g10 * w10 + g01 * w01 + g11 * w11 + 0.5f);
      const auto b = static_cast<std::uint32_t>(b00 * w00 + b10 * w10 + b01 * w01 + b11 * w11 + 0.5f);

      dst[y * out_w + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
  }
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  Window* win = reinterpret_cast<Window*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

  switch (msg) {
    case WM_NCCREATE: {
      CREATESTRUCTA* cs = reinterpret_cast<CREATESTRUCTA*>(lparam);
      SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
      return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
    case WM_COMMAND: {
      const WORD id = LOWORD(wparam);
      if (id == IDM_FILE_OPEN || id == IDM_SETTINGS_SELECT_ROM) {
        std::string rom_path = Window::open_file_dialog(hwnd);
        if (!rom_path.empty() && win != nullptr) {
          win->set_pending_rom(rom_path);
          ConfigFile config;
          config.load("config.ini");
          win->save_config(config);
        }
      } else if (id == IDM_FILE_EXIT) {
        PostQuitMessage(0);
      } else if (id >= IDM_ASPECT_3_2 && id <= IDM_ASPECT_STRETCH) {
        if (win != nullptr) {
          switch (id) {
            case IDM_ASPECT_3_2:     win->set_aspect_ratio(AspectRatio::Original_3_2); break;
            case IDM_ASPECT_4_3:     win->set_aspect_ratio(AspectRatio::Ratio_4_3); break;
            case IDM_ASPECT_16_9:    win->set_aspect_ratio(AspectRatio::Ratio_16_9); break;
            case IDM_ASPECT_21_9:    win->set_aspect_ratio(AspectRatio::Ratio_21_9); break;
            case IDM_ASPECT_21_10:   win->set_aspect_ratio(AspectRatio::Ratio_21_10); break;
            case IDM_ASPECT_STRETCH: win->set_aspect_ratio(AspectRatio::Stretch); break;
          }
          ConfigFile config;
          config.load("config.ini");
          win->save_config(config);
        }
      } else if (id >= IDM_RES_NATIVE && id <= IDM_RES_4K) {
        if (win != nullptr) {
          switch (id) {
            case IDM_RES_NATIVE: win->set_internal_resolution(InternalResolution::Native); break;
            case IDM_RES_720P:   win->set_internal_resolution(InternalResolution::Res_720p); break;
            case IDM_RES_1080P:  win->set_internal_resolution(InternalResolution::Res_1080p); break;
            case IDM_RES_4K:     win->set_internal_resolution(InternalResolution::Res_4K); break;
          }
          ConfigFile config;
          config.load("config.ini");
          win->save_config(config);
        }
      } else if (id >= IDM_FILTER_NEAREST && id <= IDM_FILTER_SCALE2X) {
        if (win != nullptr) {
          switch (id) {
            case IDM_FILTER_NEAREST:  win->set_video_filter(VideoFilter::NearestNeighbor); break;
            case IDM_FILTER_BILINEAR: win->set_video_filter(VideoFilter::Bilinear); break;
            case IDM_FILTER_SCALE2X:   win->set_video_filter(VideoFilter::Scale2x); break;
          }
          ConfigFile config;
          config.load("config.ini");
          win->save_config(config);
        }
      } else if (id == IDM_SETTINGS_CONTROLS || id == IDM_HELP_CONTROLS) {
        MessageBoxA(hwnd,
          "AW-Recompiled Controls & Controller Support:\n\n"
          "⌨️ Keyboard Mapping:\n"
          "  D-Pad:  Arrow Keys / W A S D\n"
          "  A Button:  Z / J\n"
          "  B Button:  X / K\n"
          "  Start:  Enter\n"
          "  Select:  Backspace / Shift\n"
          "  L Shoulder:  Q\n"
          "  R Shoulder:  E\n\n"
          "🎮 XInput / USB Gamepad Mapping:\n"
          "  D-Pad / Left Stick: Move Cursor / Map\n"
          "  A / X Buttons: Confirm / Select (GBA A)\n"
          "  B / Y Buttons: Cancel / Info (GBA B)\n"
          "  Start / Menu: Pause Menu (GBA Start)\n"
          "  Back / View: Status / Map View (GBA Select)\n"
          "  LB / LT: Fast Move / Page Left (GBA L)\n"
          "  RB / RT: Fast Move / Page Right (GBA R)\n\n"
          "All preferences and keybindings are saved to config.ini.",
          "Controls & Input Settings",
          MB_OK | MB_ICONINFORMATION);
      } else if (id == IDM_HELP_ABOUT) {
        MessageBoxA(hwnd,
          "AW-Recompiled v0.1 Alpha\n\n"
          "A native C++ static recompilation engine for Advance Wars (GBA).\n"
          "Features native ARM translation, XInput USB controller support, GDI software rendering, and 16-bit PCM audio.\n",
          "About AW-Recompiled",
          MB_OK | MB_ICONINFORMATION);
      }
      return 0;
    }
    case WM_DESTROY:
    case WM_CLOSE: {
      PostQuitMessage(0);
      return 0;
    }
  }

  return DefWindowProcA(hwnd, msg, wparam, lparam);
}

}  // namespace

Window::Window(int width, int height, const char* title)
    : width_(width), height_(height) {
  HINSTANCE instance = GetModuleHandleA(nullptr);

  WNDCLASSEXA wc = {};
  wc.cbSize = sizeof(WNDCLASSEXA);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = "AdvanceWarsWindowClass";

  RegisterClassExA(&wc);

  // Consolidated Win32 Menu Bar
  HMENU hMenuBar = CreateMenu();
  HMENU hFileMenu = CreatePopupMenu();
  HMENU hDisplayMenu = CreatePopupMenu();
  HMENU hAspectSubMenu = CreatePopupMenu();
  HMENU hResSubMenu = CreatePopupMenu();
  HMENU hFilterSubMenu = CreatePopupMenu();
  HMENU hSettingsMenu = CreatePopupMenu();
  HMENU hHelpMenu = CreatePopupMenu();

  // File Menu
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_OPEN, "&Open ROM...\tCtrl+O");
  AppendMenuA(hFileMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_EXIT, "E&xit");

  // Display Submenus
  AppendMenuA(hAspectSubMenu, MF_STRING, IDM_ASPECT_3_2, "&3:2 Window Mode (960x640)");
  AppendMenuA(hAspectSubMenu, MF_STRING, IDM_ASPECT_4_3, "&4:3 Window Mode (960x720)");
  AppendMenuA(hAspectSubMenu, MF_STRING, IDM_ASPECT_16_9, "1&6:9 Window Mode (1152x648)");
  AppendMenuA(hAspectSubMenu, MF_STRING, IDM_ASPECT_21_9, "&21:9 Window Mode (1260x540)");
  AppendMenuA(hAspectSubMenu, MF_STRING, IDM_ASPECT_21_10, "21:10 Window Mode (1134x540)");
  AppendMenuA(hAspectSubMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuA(hAspectSubMenu, MF_STRING, IDM_ASPECT_STRETCH, "&Stretch to Window");

  AppendMenuA(hResSubMenu, MF_STRING, IDM_RES_NATIVE, "&Native (240x160)");
  AppendMenuA(hResSubMenu, MF_STRING, IDM_RES_720P,   "&720p (1280x720)");
  AppendMenuA(hResSubMenu, MF_STRING, IDM_RES_1080P,  "1&080p (1920x1080)");
  AppendMenuA(hResSubMenu, MF_STRING, IDM_RES_4K,     "&4K (3840x2160)");

  AppendMenuA(hFilterSubMenu, MF_STRING, IDM_FILTER_NEAREST,  "&Nearest Neighbor (Crisp Pixels)");
  AppendMenuA(hFilterSubMenu, MF_STRING, IDM_FILTER_BILINEAR, "&Bilinear Smooth (Anti-Aliased)");
  AppendMenuA(hFilterSubMenu, MF_STRING, IDM_FILTER_SCALE2X,   "&Scale2x HD Filter");

  // Consolidated Display Menu
  AppendMenuA(hDisplayMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hAspectSubMenu), "&Aspect Ratio");
  AppendMenuA(hDisplayMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hResSubMenu), "&Internal Resolution");
  AppendMenuA(hDisplayMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFilterSubMenu), "&Video Filter");

  // Settings Menu
  AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_SELECT_ROM, "Select &GBA ROM Path...");
  AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_CONTROLS, "&Configure Controls & Gamepad...");

  // Help Menu
  AppendMenuA(hHelpMenu, MF_STRING, IDM_HELP_CONTROLS, "&Controls Info...");
  AppendMenuA(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, "&About AW-Recompiled...");

  // Main Bar
  AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hFileMenu), "&File");
  AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hDisplayMenu), "&Display");
  AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hSettingsMenu), "&Settings");
  AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hHelpMenu), "&Help");

  menu_ = static_cast<void*>(hMenuBar);

  scale2x_buffer_.resize((kGbaWidth * 2) * (kGbaHeight * 2));

  RECT rect = {0, 0, width, height};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, TRUE);

  HWND hwnd = CreateWindowExA(
      0,
      "AdvanceWarsWindowClass",
      title,
      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      rect.right - rect.left,
      rect.bottom - rect.top,
      nullptr,
      hMenuBar,
      instance,
      this);

  if (hwnd != nullptr) {
    hwnd_ = static_cast<void*>(hwnd);
    hdc_ = static_cast<void*>(GetDC(hwnd));
    is_open_ = true;
    update_menu_checks();
  } else {
    std::cerr << "CreateWindowExA failed, error code: " << GetLastError() << std::endl;
  }
}

Window::~Window() {
  if (hwnd_ != nullptr) {
    if (hdc_ != nullptr) {
      ReleaseDC(static_cast<HWND>(hwnd_), static_cast<HDC>(hdc_));
    }
    DestroyWindow(static_cast<HWND>(hwnd_));
  }
}

std::string Window::consume_pending_rom() {
  std::string rom = pending_rom_path_;
  pending_rom_path_.clear();
  return rom;
}

void Window::set_aspect_ratio(AspectRatio ratio) {
  aspect_ratio_ = ratio;
  update_menu_checks();

  // Resize window frame to match chosen aspect ratio
  switch (ratio) {
    case AspectRatio::Original_3_2:  resize_client(960, 640); break;
    case AspectRatio::Ratio_4_3:     resize_client(960, 720); break;
    case AspectRatio::Ratio_16_9:    resize_client(1152, 648); break;
    case AspectRatio::Ratio_21_9:    resize_client(1260, 540); break;
    case AspectRatio::Ratio_21_10:   resize_client(1134, 540); break;
    case AspectRatio::Stretch:      break;
  }

  if (hwnd_ != nullptr) {
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, TRUE);
  }
}

void Window::set_internal_resolution(InternalResolution res) {
  internal_resolution_ = res;
  update_menu_checks();

  switch (res) {
    case InternalResolution::Native:   resize_client(960, 640); break;
    case InternalResolution::Res_720p:  resize_client(1280, 720); break;
    case InternalResolution::Res_1080p: resize_client(1920, 1080); break;
    case InternalResolution::Res_4K:    resize_client(3840, 2160); break;
  }

  if (hwnd_ != nullptr) {
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, TRUE);
  }
}

void Window::set_video_filter(VideoFilter filter) {
  video_filter_ = filter;
  update_menu_checks();

  if (hwnd_ != nullptr) {
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, TRUE);
  }
}

void Window::load_config(const ConfigFile& config) {
  const int aspect = config.get_int("Display", "aspect_ratio", 0);
  const int res = config.get_int("Display", "internal_resolution", 0);
  const int filter = config.get_int("Display", "video_filter", 1);

  set_aspect_ratio(static_cast<AspectRatio>(std::clamp(aspect, 0, 5)));
  set_internal_resolution(static_cast<InternalResolution>(std::clamp(res, 0, 3)));
  set_video_filter(static_cast<VideoFilter>(std::clamp(filter, 0, 2)));
}

void Window::save_config(ConfigFile& config) const {
  config.set_int("Display", "aspect_ratio", static_cast<int>(aspect_ratio_));
  config.set_int("Display", "internal_resolution", static_cast<int>(internal_resolution_));
  config.set_int("Display", "video_filter", static_cast<int>(video_filter_));
  if (!pending_rom_path_.empty()) {
    config.set_string("Paths", "rom_path", pending_rom_path_);
  }
  config.save("config.ini");
}

void Window::resize_client(int width, int height) {
  if (hwnd_ == nullptr) return;
  HWND hwnd = static_cast<HWND>(hwnd_);

  RECT rect = {0, 0, width, height};
  DWORD style = static_cast<DWORD>(GetWindowLongA(hwnd, GWL_STYLE));
  BOOL has_menu = GetMenu(hwnd) != nullptr ? TRUE : FALSE;
  AdjustWindowRect(&rect, style, has_menu);

  SetWindowPos(hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::update_menu_checks() {
  if (hwnd_ == nullptr) return;
  HMENU hMenuBar = GetMenu(static_cast<HWND>(hwnd_));
  if (hMenuBar == nullptr) return;

  CheckMenuItem(hMenuBar, IDM_ASPECT_3_2,     aspect_ratio_ == AspectRatio::Original_3_2 ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_ASPECT_4_3,     aspect_ratio_ == AspectRatio::Ratio_4_3     ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_ASPECT_16_9,    aspect_ratio_ == AspectRatio::Ratio_16_9    ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_ASPECT_21_9,    aspect_ratio_ == AspectRatio::Ratio_21_9    ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_ASPECT_21_10,   aspect_ratio_ == AspectRatio::Ratio_21_10   ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_ASPECT_STRETCH, aspect_ratio_ == AspectRatio::Stretch      ? MF_CHECKED : MF_UNCHECKED);

  CheckMenuItem(hMenuBar, IDM_RES_NATIVE, internal_resolution_ == InternalResolution::Native    ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_RES_720P,   internal_resolution_ == InternalResolution::Res_720p  ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_RES_1080P,  internal_resolution_ == InternalResolution::Res_1080p ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_RES_4K,     internal_resolution_ == InternalResolution::Res_4K    ? MF_CHECKED : MF_UNCHECKED);

  CheckMenuItem(hMenuBar, IDM_FILTER_NEAREST,  video_filter_ == VideoFilter::NearestNeighbor ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_FILTER_BILINEAR, video_filter_ == VideoFilter::Bilinear        ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_FILTER_SCALE2X,   video_filter_ == VideoFilter::Scale2x         ? MF_CHECKED : MF_UNCHECKED);
}

std::string Window::open_file_dialog(void* parent_hwnd) {
  char file_name[MAX_PATH] = "";
  OPENFILENAMEA ofn = {};
  ofn.lStructSize = sizeof(OPENFILENAMEA);
  ofn.hwndOwner = static_cast<HWND>(parent_hwnd);
  ofn.lpstrFilter = "Game Boy Advance ROMs (*.gba)\0*.gba\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = file_name;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  ofn.lpstrTitle = "Select Advance Wars ROM File";

  if (GetOpenFileNameA(&ofn) == TRUE) {
    return std::string(file_name);
  }
  return "";
}

bool Window::process_events(Hardware& hardware) {
  if (!is_open_) return false;

  MSG msg;
  while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      is_open_ = false;
      return false;
    }

    if (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP) {
      const bool down = (msg.message == WM_KEYDOWN);
      const WPARAM vk = msg.wParam;

      std::uint16_t key_bit = 0;
      if (vk == VK_UP || vk == 'W') key_bit = kKeyUp;
      else if (vk == VK_DOWN || vk == 'S') key_bit = kKeyDown;
      else if (vk == VK_LEFT || vk == 'A') key_bit = kKeyLeft;
      else if (vk == VK_RIGHT || vk == 'D') key_bit = kKeyRight;
      else if (vk == 'Z' || vk == 'J') key_bit = kKeyA;
      else if (vk == 'X' || vk == 'K') key_bit = kKeyB;
      else if (vk == VK_RETURN) key_bit = kKeyStart;
      else if (vk == VK_BACK || vk == VK_SHIFT) key_bit = kKeySelect;
      else if (vk == 'Q') key_bit = kKeyL;
      else if (vk == 'E') key_bit = kKeyR;

      if (key_bit != 0) {
        if (down) hardware.keys_pressed |= key_bit;
        else hardware.keys_pressed &= ~key_bit;
      }
    }

    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }

  // Poll XInput USB Controllers
  XINPUT_STATE xstate;
  std::memset(&xstate, 0, sizeof(XINPUT_STATE));
  if (XInputGetState(0, &xstate) == ERROR_SUCCESS) {
    const WORD btns = xstate.Gamepad.wButtons;

    if (btns & XINPUT_GAMEPAD_DPAD_UP) hardware.keys_pressed |= kKeyUp;
    if (btns & XINPUT_GAMEPAD_DPAD_DOWN) hardware.keys_pressed |= kKeyDown;
    if (btns & XINPUT_GAMEPAD_DPAD_LEFT) hardware.keys_pressed |= kKeyLeft;
    if (btns & XINPUT_GAMEPAD_DPAD_RIGHT) hardware.keys_pressed |= kKeyRight;

    // Analog Thumbstick to D-Pad mapping with deadzone
    constexpr SHORT kDeadZone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    if (xstate.Gamepad.sThumbLY > kDeadZone) hardware.keys_pressed |= kKeyUp;
    if (xstate.Gamepad.sThumbLY < -kDeadZone) hardware.keys_pressed |= kKeyDown;
    if (xstate.Gamepad.sThumbLX < -kDeadZone) hardware.keys_pressed |= kKeyLeft;
    if (xstate.Gamepad.sThumbLX > kDeadZone) hardware.keys_pressed |= kKeyRight;

    if ((btns & XINPUT_GAMEPAD_A) || (btns & XINPUT_GAMEPAD_X)) hardware.keys_pressed |= kKeyA;
    if ((btns & XINPUT_GAMEPAD_B) || (btns & XINPUT_GAMEPAD_Y)) hardware.keys_pressed |= kKeyB;
    if (btns & XINPUT_GAMEPAD_START) hardware.keys_pressed |= kKeyStart;
    if (btns & XINPUT_GAMEPAD_BACK) hardware.keys_pressed |= kKeySelect;
    if ((btns & XINPUT_GAMEPAD_LEFT_SHOULDER) || (xstate.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)) hardware.keys_pressed |= kKeyL;
    if ((btns & XINPUT_GAMEPAD_RIGHT_SHOULDER) || (xstate.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)) hardware.keys_pressed |= kKeyR;
  }

  return is_open_;
}

void Window::render(const Ppu& ppu) {
  if (!is_open_ || hdc_ == nullptr) return;

  HWND hwnd = static_cast<HWND>(hwnd_);
  HDC hdc = static_cast<HDC>(hdc_);

  RECT client_rect;
  GetClientRect(hwnd, &client_rect);
  const int client_w = client_rect.right - client_rect.left;
  const int client_h = client_rect.bottom - client_rect.top;

  if (client_w <= 0 || client_h <= 0) return;

  // Recalculate viewport and clear letterbox padding if dimensions or ratio changed
  if (client_w != last_client_w_ || client_h != last_client_h_ || aspect_ratio_ != last_aspect_ratio_) {
    last_client_w_ = client_w;
    last_client_h_ = client_h;
    last_aspect_ratio_ = aspect_ratio_;
    cached_viewport_ = calculate_viewport_rect(client_w, client_h, aspect_ratio_);

    // Clear entire window background to black to remove leftover frames
    HBRUSH black_brush = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    FillRect(hdc, &client_rect, black_brush);
  }

  const ViewportRect vp = cached_viewport_;

  // Ensure pillarbox / letterbox margins remain solid black
  if (vp.x > 0) {
    RECT left_rect = {0, 0, vp.x, client_h};
    RECT right_rect = {vp.x + vp.width, 0, client_w, client_h};
    HBRUSH black_brush = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    FillRect(hdc, &left_rect, black_brush);
    FillRect(hdc, &right_rect, black_brush);
  }
  if (vp.y > 0) {
    RECT top_rect = {0, 0, client_w, vp.y};
    RECT bottom_rect = {0, vp.y + vp.height, client_w, client_h};
    HBRUSH black_brush = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    FillRect(hdc, &top_rect, black_brush);
    FillRect(hdc, &bottom_rect, black_brush);
  }

  // Use COLORONCOLOR for hardware-fast blitting at 1080p and 4K (eliminating slow GDI HALFTONE CPU lag)
  SetStretchBltMode(hdc, COLORONCOLOR);

  const std::uint32_t* render_data = ppu.framebuffer.data();
  int src_w = kGbaWidth;
  int src_h = kGbaHeight;

  if (video_filter_ == VideoFilter::Scale2x && !scale2x_buffer_.empty()) {
    apply_scale2x(ppu.framebuffer.data(), scale2x_buffer_.data(), kGbaWidth, kGbaHeight);
    render_data = scale2x_buffer_.data();
    src_w = kGbaWidth * 2;
    src_h = kGbaHeight * 2;
  } else if (video_filter_ == VideoFilter::Bilinear && !scale2x_buffer_.empty()) {
    apply_bilinear_2x(ppu.framebuffer.data(), scale2x_buffer_.data(), kGbaWidth, kGbaHeight);
    render_data = scale2x_buffer_.data();
    src_w = kGbaWidth * 2;
    src_h = kGbaHeight * 2;
  }

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = src_w;
  bmi.bmiHeader.biHeight = -src_h; // Top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  StretchDIBits(
      hdc,
      vp.x,
      vp.y,
      vp.width,
      vp.height,
      0,
      0,
      src_w,
      src_h,
      render_data,
      &bmi,
      DIB_RGB_COLORS,
      SRCCOPY);
}

#else

ViewportRect calculate_viewport_rect(int client_width, int client_height, AspectRatio ratio) {
  if (client_width <= 0 || client_height <= 0) return {0, 0, 0, 0};
  if (ratio == AspectRatio::Stretch) return {0, 0, client_width, client_height};
  double target_aspect = 3.0 / 2.0;
  switch (ratio) {
    case AspectRatio::Original_3_2:  target_aspect = 3.0 / 2.0; break;
    case AspectRatio::Ratio_4_3:     target_aspect = 4.0 / 3.0; break;
    case AspectRatio::Ratio_16_9:    target_aspect = 16.0 / 9.0; break;
    case AspectRatio::Ratio_21_9:    target_aspect = 21.0 / 9.0; break;
    case AspectRatio::Ratio_21_10:   target_aspect = 21.0 / 10.0; break;
    case AspectRatio::Stretch:      break;
  }
  const double client_aspect = static_cast<double>(client_width) / static_cast<double>(client_height);
  if (client_aspect > target_aspect) {
    int vp_h = client_height;
    int vp_w = static_cast<int>(client_height * target_aspect + 0.5);
    return {(client_width - vp_w) / 2, 0, vp_w, vp_h};
  } else {
    int vp_w = client_width;
    int vp_h = static_cast<int>(client_width / target_aspect + 0.5);
    return {0, (client_height - vp_h) / 2, vp_w, vp_h};
  }
}

Window::Window(int width, int height, const char* /*title*/)
    : is_open_(false), width_(width), height_(height) {}

Window::~Window() {}

bool Window::process_events(Hardware& /*hardware*/) { return false; }
void Window::render(const Ppu& /*ppu*/) {}
void Window::set_aspect_ratio(AspectRatio ratio) { aspect_ratio_ = ratio; }
void Window::set_internal_resolution(InternalResolution res) { internal_resolution_ = res; }
void Window::set_video_filter(VideoFilter filter) { video_filter_ = filter; }
void Window::load_config(const ConfigFile& /*config*/) {}
void Window::save_config(ConfigFile& /*config*/) const {}
void Window::resize_client(int /*width*/, int /*height*/) {}
std::string Window::consume_pending_rom() { return ""; }
std::string Window::open_file_dialog(void* /*parent_hwnd*/) { return ""; }
void Window::update_menu_checks() {}

#endif

}  // namespace aw
