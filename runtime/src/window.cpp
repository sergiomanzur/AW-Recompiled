#include "aw/window.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#include <algorithm>
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

constexpr UINT IDM_FILE_OPEN       = 1001;
constexpr UINT IDM_FILE_EXIT       = 1002;
constexpr UINT IDM_ASPECT_3_2      = 2001;
constexpr UINT IDM_ASPECT_4_3      = 2002;
constexpr UINT IDM_ASPECT_16_9     = 2003;
constexpr UINT IDM_ASPECT_21_9     = 2004;
constexpr UINT IDM_ASPECT_21_10    = 2005;
constexpr UINT IDM_ASPECT_STRETCH  = 2006;
constexpr UINT IDM_RES_NATIVE      = 2101;
constexpr UINT IDM_RES_720P        = 2102;
constexpr UINT IDM_RES_1080P       = 2103;
constexpr UINT IDM_RES_4K          = 2104;
constexpr UINT IDM_HELP_CONTROLS   = 3001;
constexpr UINT IDM_HELP_ABOUT      = 3002;

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
      if (id == IDM_FILE_OPEN) {
        std::string rom_path = Window::open_file_dialog(hwnd);
        if (!rom_path.empty() && win != nullptr) {
          win->set_pending_rom(rom_path);
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
        }
      } else if (id >= IDM_RES_NATIVE && id <= IDM_RES_4K) {
        if (win != nullptr) {
          switch (id) {
            case IDM_RES_NATIVE: win->set_internal_resolution(InternalResolution::Native); break;
            case IDM_RES_720P:   win->set_internal_resolution(InternalResolution::Res_720p); break;
            case IDM_RES_1080P:  win->set_internal_resolution(InternalResolution::Res_1080p); break;
            case IDM_RES_4K:     win->set_internal_resolution(InternalResolution::Res_4K); break;
          }
        }
      } else if (id == IDM_HELP_CONTROLS) {
        MessageBoxA(hwnd,
          "Advance Wars (Native Recomp) Controls:\n\n"
          "D-Pad:  Arrow Keys / W A S D\n"
          "A Button:  Z / J\n"
          "B Button:  X / K\n"
          "Start:  Enter\n"
          "Select:  Backspace / Shift\n"
          "L Shoulder:  Q\n"
          "R Shoulder:  E\n",
          "Controls - AW-Recompiled",
          MB_OK | MB_ICONINFORMATION);
      } else if (id == IDM_HELP_ABOUT) {
        MessageBoxA(hwnd,
          "AW-Recompiled v0.1 Alpha\n\n"
          "A native C++ static recompilation engine for Advance Wars (GBA).\n"
          "Features native ARM translation, GDI software rendering, and 16-bit PCM stereo audio.\n",
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

  // Build Win32 Menu Bar
  HMENU hMenuBar = CreateMenu();
  HMENU hFileMenu = CreatePopupMenu();
  HMENU hAspectMenu = CreatePopupMenu();
  HMENU hResMenu = CreatePopupMenu();
  HMENU hHelpMenu = CreatePopupMenu();

  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_OPEN, "&Open ROM...\tCtrl+O");
  AppendMenuA(hFileMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_EXIT, "E&xit");

  AppendMenuA(hAspectMenu, MF_STRING, IDM_ASPECT_3_2, "&3:2 Window Mode (960x640)");
  AppendMenuA(hAspectMenu, MF_STRING, IDM_ASPECT_4_3, "&4:3 Window Mode (960x720)");
  AppendMenuA(hAspectMenu, MF_STRING, IDM_ASPECT_16_9, "1&6:9 Window Mode (1152x648)");
  AppendMenuA(hAspectMenu, MF_STRING, IDM_ASPECT_21_9, "&21:9 Window Mode (1260x540)");
  AppendMenuA(hAspectMenu, MF_STRING, IDM_ASPECT_21_10, "21:10 Window Mode (1134x540)");
  AppendMenuA(hAspectMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuA(hAspectMenu, MF_STRING, IDM_ASPECT_STRETCH, "&Stretch to Window");

  AppendMenuA(hResMenu, MF_STRING, IDM_RES_NATIVE, "&Native (240x160)");
  AppendMenuA(hResMenu, MF_STRING, IDM_RES_720P,   "&720p (1280x720)");
  AppendMenuA(hResMenu, MF_STRING, IDM_RES_1080P,  "1&080p (1920x1080)");
  AppendMenuA(hResMenu, MF_STRING, IDM_RES_4K,     "&4K (3840x2160)");

  AppendMenuA(hHelpMenu, MF_STRING, IDM_HELP_CONTROLS, "&Controls...");
  AppendMenuA(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, "&About AW-Recompiled...");

  AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hFileMenu), "&File");
  AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hAspectMenu), "&Aspect Ratio");
  AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hResMenu), "&Internal Resolution");
  AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hHelpMenu), "&Help");

  menu_ = static_cast<void*>(hMenuBar);

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
    case InternalResolution::Native:
      internal_width_ = kGbaWidth;
      internal_height_ = kGbaHeight;
      resize_client(960, 640);
      break;
    case InternalResolution::Res_720p:
      internal_width_ = 1280;
      internal_height_ = 720;
      resize_client(1280, 720);
      break;
    case InternalResolution::Res_1080p:
      internal_width_ = 1920;
      internal_height_ = 1080;
      resize_client(1920, 1080);
      break;
    case InternalResolution::Res_4K:
      internal_width_ = 3840;
      internal_height_ = 2160;
      resize_client(3840, 2160);
      break;
  }

  if (internal_width_ > 0 && internal_height_ > 0) {
    internal_buffer_.resize(internal_width_ * internal_height_);
  }

  if (hwnd_ != nullptr) {
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, TRUE);
  }
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

  if (internal_resolution_ != InternalResolution::Native && !internal_buffer_.empty()) {
    const int target_w = internal_width_;
    const int target_h = internal_height_;

    for (int y = 0; y < target_h; ++y) {
      const int src_y = (y * kGbaHeight) / target_h;
      const std::uint32_t* src_row = &ppu.framebuffer[src_y * kGbaWidth];
      std::uint32_t* dst_row = &internal_buffer_[y * target_w];
      for (int x = 0; x < target_w; ++x) {
        const int src_x = (x * kGbaWidth) / target_w;
        dst_row[x] = src_row[src_x];
      }
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = target_w;
    bmi.bmiHeader.biHeight = -target_h; // Top-down
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
        target_w,
        target_h,
        internal_buffer_.data(),
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);
  } else {
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kGbaWidth;
    bmi.bmiHeader.biHeight = -kGbaHeight; // Top-down
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
        kGbaWidth,
        kGbaHeight,
        ppu.framebuffer.data(),
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);
  }
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
void Window::resize_client(int /*width*/, int /*height*/) {}
std::string Window::consume_pending_rom() { return ""; }
std::string Window::open_file_dialog(void* /*parent_hwnd*/) { return ""; }
void Window::update_menu_checks() {}

#endif

}  // namespace aw
