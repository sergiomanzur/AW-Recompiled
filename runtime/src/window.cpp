#include "aw/window.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>

#include <chrono>
#include <commctrl.h>
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
constexpr UINT IDM_FILE_SAVE_QUICK      = 1003;
constexpr UINT IDM_FILE_LOAD_QUICK      = 1004;
constexpr UINT IDM_FILE_SAVE_AS         = 1005;
constexpr UINT IDM_FILE_LOAD_FROM       = 1006;
constexpr UINT IDM_FILE_REWIND          = 1007;
constexpr UINT IDM_FILE_FASTFORWARD     = 1008;
constexpr UINT IDM_FILE_UNDO            = 1009;
constexpr UINT IDM_FILE_REPLAY_RECORD   = 1010;
constexpr UINT IDM_FILE_REPLAY_PLAY     = 1011;
constexpr UINT IDM_FILE_APPLY_PATCH     = 1012;
constexpr UINT IDM_SAVE_SLOT_1          = 1011;
constexpr UINT IDM_SAVE_SLOT_2          = 1012;
constexpr UINT IDM_SAVE_SLOT_3          = 1013;
constexpr UINT IDM_SAVE_SLOT_4          = 1014;
constexpr UINT IDM_SAVE_SLOT_5          = 1015;
constexpr UINT IDM_LOAD_SLOT_1          = 1021;
constexpr UINT IDM_LOAD_SLOT_2          = 1022;
constexpr UINT IDM_LOAD_SLOT_3          = 1023;
constexpr UINT IDM_LOAD_SLOT_4          = 1024;
constexpr UINT IDM_LOAD_SLOT_5          = 1025;
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
constexpr UINT IDM_SETTINGS_TOGGLE_HUD  = 3003;
constexpr UINT IDM_SETTINGS_SIDEBAR     = 3004;
constexpr UINT IDM_SETTINGS_INPUT_DISPLAY = 3005;
constexpr UINT IDM_SETTINGS_HD_TEXT     = 3006;
constexpr UINT IDM_SETTINGS_CHEATS    = 3007;
constexpr UINT IDM_HELP_CONTROLS        = 4001;
constexpr UINT IDM_HELP_ABOUT           = 4002;

struct DialogData {
  InputMapping temp_mapping;
  HWND key_buttons[Gba_Count]{};
  HWND pad_buttons[Gba_Count]{};
  HWND combo_controller = nullptr;
  HWND check_mouse = nullptr;
  int listening_btn_index = -1;
  bool listening_is_pad = false;
  bool saved = false;
};

void update_dialog_button_texts(DialogData* data) {
  for (int i = 0; i < Gba_Count; ++i) {
    if (data->listening_btn_index == i && !data->listening_is_pad) {
      SetWindowTextA(data->key_buttons[i], "<Press Key...>");
    } else {
      SetWindowTextA(data->key_buttons[i], vk_to_string(data->temp_mapping.bindings[i].key_vk).c_str());
    }

    if (data->listening_btn_index == i && data->listening_is_pad) {
      SetWindowTextA(data->pad_buttons[i], "<Press Button...>");
    } else {
      SetWindowTextA(data->pad_buttons[i], xinput_button_to_string(data->temp_mapping.bindings[i].pad_button).c_str());
    }
  }
}

LRESULT CALLBACK remap_dialog_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  DialogData* data = reinterpret_cast<DialogData*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

  switch (msg) {
    case WM_NCCREATE: {
      CREATESTRUCTA* cs = reinterpret_cast<CREATESTRUCTA*>(lparam);
      SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
      return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
    case WM_COMMAND: {
      const WORD id = LOWORD(wparam);
      if (data != nullptr) {
        if (id >= 500 && id < 500 + Gba_Count) {
          data->listening_btn_index = id - 500;
          data->listening_is_pad = false;
          update_dialog_button_texts(data);
          SetFocus(hwnd);
          return 0;
        } else if (id >= 600 && id < 600 + Gba_Count) {
          data->listening_btn_index = id - 600;
          data->listening_is_pad = true;
          update_dialog_button_texts(data);
          SetFocus(hwnd);
          return 0;
        } else if (id == 701) { // Restore Defaults
          data->temp_mapping.reset_to_defaults();
          data->listening_btn_index = -1;
          SendMessageA(data->combo_controller, CB_SETCURSEL, 0, 0);
          SendMessageA(data->check_mouse, BM_SETCHECK, BST_CHECKED, 0);
          update_dialog_button_texts(data);
          return 0;
        } else if (id == 702) { // Save / OK
          data->temp_mapping.controller_index = static_cast<int>(SendMessageA(data->combo_controller, CB_GETCURSEL, 0, 0));
          if (data->temp_mapping.controller_index == 4) {
            data->temp_mapping.controller_index = -1; // Disabled
          }
          data->temp_mapping.mouse_enabled = (SendMessageA(data->check_mouse, BM_GETCHECK, 0, 0) == BST_CHECKED);
          data->saved = true;
          DestroyWindow(hwnd);
          return 0;
        } else if (id == 703) { // Cancel
          data->saved = false;
          DestroyWindow(hwnd);
          return 0;
        }
      }
      break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
      if (data != nullptr && data->listening_btn_index >= 0 && !data->listening_is_pad) {
        const std::uint32_t vk = static_cast<std::uint32_t>(wparam);
        if (vk != VK_ESCAPE) {
          data->temp_mapping.bindings[data->listening_btn_index].key_vk = vk;
        }
        data->listening_btn_index = -1;
        update_dialog_button_texts(data);
        return 0;
      }
      break;
    }
    case WM_CLOSE: {
      DestroyWindow(hwnd);
      return 0;
    }
  }
  return DefWindowProcA(hwnd, msg, wparam, lparam);
}

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
  // Exact 2x bilinear in fixed point. At a scale of exactly two the sample
  // weights only ever take two values per axis (1/4 and 3/4), so the whole
  // filter reduces to a 2x2 blend with sixteenths -- no per-pixel float work.
  // The float version cost ~3.7 ms a frame, a quarter of the frame budget,
  // and its truncating source index gave the top/left edge negative weights.
  const int out_w = w * 2;
  const int out_h = h * 2;

  for (int y = 0; y < out_h; ++y) {
    const int ky = y >> 1;
    // Even output rows sample toward the previous source row, odd toward the
    // next; the nearer row always carries the 3/4 weight.
    const bool y_low = (y & 1) == 0;
    const int y0 = y_low ? (ky > 0 ? ky - 1 : 0) : ky;
    const int y1 = y_low ? ky : (ky + 1 < h ? ky + 1 : h - 1);
    const int wy0 = y_low ? 1 : 3;
    const int wy1 = 4 - wy0;

    const std::uint32_t* row0 = src + static_cast<std::size_t>(y0) * w;
    const std::uint32_t* row1 = src + static_cast<std::size_t>(y1) * w;
    std::uint32_t* out = dst + static_cast<std::size_t>(y) * out_w;

    for (int x = 0; x < out_w; ++x) {
      const int kx = x >> 1;
      const bool x_low = (x & 1) == 0;
      const int x0 = x_low ? (kx > 0 ? kx - 1 : 0) : kx;
      const int x1 = x_low ? kx : (kx + 1 < w ? kx + 1 : w - 1);
      const int wx0 = x_low ? 1 : 3;
      const int wx1 = 4 - wx0;

      const std::uint32_t c00 = row0[x0];
      const std::uint32_t c10 = row0[x1];
      const std::uint32_t c01 = row1[x0];
      const std::uint32_t c11 = row1[x1];

      // Weights are sixteenths and sum to 16, so the blend is a shift.
      const std::uint32_t w00 = static_cast<std::uint32_t>(wx0 * wy0);
      const std::uint32_t w10 = static_cast<std::uint32_t>(wx1 * wy0);
      const std::uint32_t w01 = static_cast<std::uint32_t>(wx0 * wy1);
      const std::uint32_t w11 = static_cast<std::uint32_t>(wx1 * wy1);

      // Red and blue occupy alternating byte lanes, so they blend together in
      // one set of operations; green is done on its own lane.
      const std::uint32_t rb = (((c00 & 0x00FF00FFu) * w00 +
                                 (c10 & 0x00FF00FFu) * w10 +
                                 (c01 & 0x00FF00FFu) * w01 +
                                 (c11 & 0x00FF00FFu) * w11 +
                                 0x00080008u) >> 4) & 0x00FF00FFu;
      const std::uint32_t g = (((c00 & 0x0000FF00u) * w00 +
                                (c10 & 0x0000FF00u) * w10 +
                                (c01 & 0x0000FF00u) * w01 +
                                (c11 & 0x0000FF00u) * w11 +
                                0x00000800u) >> 4) & 0x0000FF00u;

      out[x] = 0xFF000000u | rb | g;
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
      } else if (id == IDM_FILE_SAVE_QUICK) {
        if (win != nullptr) win->request_save_state("state_0.ss");
      } else if (id == IDM_FILE_LOAD_QUICK) {
        if (win != nullptr) win->request_load_state("state_0.ss");
      } else if (id == IDM_FILE_SAVE_AS) {
        if (win != nullptr) {
          std::string path = Window::save_savestate_dialog(hwnd);
          if (!path.empty()) win->request_save_state(path);
        }
      } else if (id == IDM_FILE_LOAD_FROM) {
        if (win != nullptr) {
          std::string path = Window::open_savestate_dialog(hwnd);
          if (!path.empty()) win->request_load_state(path);
        }
      } else if (id == IDM_FILE_REWIND) {
        if (win != nullptr) win->request_rewind_step();
      } else if (id == IDM_FILE_UNDO) {
        if (win != nullptr) win->request_undo();
      } else if (id == IDM_FILE_REPLAY_RECORD) {
        if (win != nullptr) win->request_record_toggle();
      } else if (id == IDM_FILE_REPLAY_PLAY) {
        if (win != nullptr) {
          std::string replay_path = Window::open_replay_dialog(hwnd);
          if (!replay_path.empty()) win->request_replay_playback(replay_path);
        }
      } else if (id == IDM_FILE_APPLY_PATCH) {
        if (win != nullptr) {
          std::string patch_path = Window::open_patch_dialog(hwnd);
          if (!patch_path.empty()) {
            ConfigFile config;
            config.load("config.ini");
            config.set_string("Patches", "ips_path", patch_path);
            const std::string rom = config.get_string("Paths", "rom_path", "");
            config.save("config.ini");
            if (!rom.empty()) {
              // Reboot the same ROM; the game loop applies the staged patch
              // at boot (see apply_boot_patch in main.cpp).
              win->set_pending_rom(rom);
              std::cout << "Patch staged: " << patch_path << " (rebooting)" << std::endl;
            }
          }
        }
      } else if (id == IDM_FILE_FASTFORWARD) {
        if (win != nullptr) win->toggle_fast_forward_latch();
      } else if (id >= IDM_SAVE_SLOT_1 && id <= IDM_SAVE_SLOT_5) {
        if (win != nullptr) win->request_save_state("state_" + std::to_string(id - IDM_SAVE_SLOT_1 + 1) + ".ss");
      } else if (id >= IDM_LOAD_SLOT_1 && id <= IDM_LOAD_SLOT_5) {
        if (win != nullptr) win->request_load_state("state_" + std::to_string(id - IDM_LOAD_SLOT_1 + 1) + ".ss");
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
        if (win != nullptr) {
          win->show_controls_dialog();
        }
      } else if (id == IDM_SETTINGS_TOGGLE_HUD) {
        if (win != nullptr) {
          win->toggle_hud();
          ConfigFile config;
          config.load("config.ini");
          win->save_config(config);
        }
      } else if (id == IDM_SETTINGS_SIDEBAR) {
        if (win != nullptr) {
          win->toggle_sidebar();
          ConfigFile config;
          config.load("config.ini");
          win->save_config(config);
          std::cout << "Tactical Sidebar: " << (win->sidebar_enabled() ? "ENABLED" : "DISABLED")
                    << std::endl;
        }
      } else if (id == IDM_SETTINGS_INPUT_DISPLAY) {
        if (win != nullptr) {
          win->set_input_display(!win->input_display());
          ConfigFile config;
          config.load("config.ini");
          win->save_config(config);
          std::cout << "Input Display: " << (win->input_display() ? "ENABLED" : "DISABLED") << std::endl;
        }
      } else if (id == IDM_SETTINGS_HD_TEXT) {
        if (win != nullptr) {
          win->toggle_hd_text();
          ConfigFile config;
          config.load("config.ini");
          win->save_config(config);
          std::cout << "HD Text Replacement: " << (win->hd_text_enabled() ? "ENABLED" : "DISABLED")
                    << std::endl;
        }
      } else if (id == IDM_SETTINGS_CHEATS) {
        if (win != nullptr) {
          win->toggle_cheats();
          ConfigFile config;
          config.load("config.ini");
          win->save_config(config);
          std::cout << "Cheats: " << (win->cheats_enabled() ? "ENABLED" : "DISABLED") << std::endl;
        }
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
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
      FillRect(hdc, &ps.rcPaint, black);
      EndPaint(hwnd, &ps);
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
  window_title_ = title != nullptr ? title : "Advance Wars (Native Recomp)";
  HINSTANCE instance = GetModuleHandleA(nullptr);

  WNDCLASSEXA wc = {};
  wc.cbSize = sizeof(WNDCLASSEXA);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = "AdvanceWarsWindowClass";

  RegisterClassExA(&wc);

  // Register Dialog Class
  WNDCLASSEXA dwc = {};
  dwc.cbSize = sizeof(WNDCLASSEXA);
  dwc.lpfnWndProc = remap_dialog_proc;
  dwc.hInstance = instance;
  dwc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  dwc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  dwc.lpszClassName = "AWControlRemapDialogClass";
  RegisterClassExA(&dwc);

  // Consolidated Win32 Menu Bar
  HMENU hMenuBar = CreateMenu();
  HMENU hFileMenu = CreatePopupMenu();
  HMENU hSaveSlotMenu = CreatePopupMenu();
  HMENU hLoadSlotMenu = CreatePopupMenu();
  HMENU hDisplayMenu = CreatePopupMenu();
  HMENU hAspectSubMenu = CreatePopupMenu();
  HMENU hResSubMenu = CreatePopupMenu();
  HMENU hFilterSubMenu = CreatePopupMenu();
  HMENU hSettingsMenu = CreatePopupMenu();
  HMENU hHelpMenu = CreatePopupMenu();

  // Save / Load State Submenus
  AppendMenuA(hSaveSlotMenu, MF_STRING, IDM_SAVE_SLOT_1, "Save Slot &1\tCtrl+F1");
  AppendMenuA(hSaveSlotMenu, MF_STRING, IDM_SAVE_SLOT_2, "Save Slot &2\tCtrl+F2");
  AppendMenuA(hSaveSlotMenu, MF_STRING, IDM_SAVE_SLOT_3, "Save Slot &3\tCtrl+F3");
  AppendMenuA(hSaveSlotMenu, MF_STRING, IDM_SAVE_SLOT_4, "Save Slot &4\tCtrl+F4");
  AppendMenuA(hSaveSlotMenu, MF_STRING, IDM_SAVE_SLOT_5, "Save Slot &5\tCtrl+F5");

  AppendMenuA(hLoadSlotMenu, MF_STRING, IDM_LOAD_SLOT_1, "Load Slot &1\tShift+F1");
  AppendMenuA(hLoadSlotMenu, MF_STRING, IDM_LOAD_SLOT_2, "Load Slot &2\tShift+F2");
  AppendMenuA(hLoadSlotMenu, MF_STRING, IDM_LOAD_SLOT_3, "Load Slot &3\tShift+F3");
  AppendMenuA(hLoadSlotMenu, MF_STRING, IDM_LOAD_SLOT_4, "Load Slot &4\tShift+F4");
  AppendMenuA(hLoadSlotMenu, MF_STRING, IDM_LOAD_SLOT_5, "Load Slot &5\tShift+F5");

  // File Menu
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_OPEN, "&Open GBA ROM...\tCtrl+O");
  AppendMenuA(hFileMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_SAVE_QUICK, "Quick &Save State\tF5");
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_LOAD_QUICK, "Quick &Load State\tF9");
  AppendMenuA(hFileMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSaveSlotMenu), "&Save State Slot");
  AppendMenuA(hFileMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hLoadSlotMenu), "&Load State Slot");
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_SAVE_AS, "Save State &As...");
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_LOAD_FROM, "Load State &From...");
  AppendMenuA(hFileMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_UNDO, "&Undo Last Order\tCtrl+Z");
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_REWIND, "&Rewind / Time Travel\tBackspace");
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_FASTFORWARD, "&Fast-Forward (toggle)\tTab");
  AppendMenuA(hFileMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_REPLAY_RECORD, "&Record Replay\tF6");
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_REPLAY_PLAY, "&Play Replay...");
  AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_APPLY_PATCH, "Apply &IPS Patch...");
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
  AppendMenuA(hSettingsMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_TOGGLE_HUD, "Show &Tactical Intel HUD Overlay\tF2");
  AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_SIDEBAR, "Show Tactical &Sidebar (widescreen)\tF4");
  AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_INPUT_DISPLAY, "Show &Input Display + Frame Counter\tF8");
  AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_HD_TEXT, "HD &Text Replacement (needs pack)");
  AppendMenuA(hSettingsMenu, MF_STRING, IDM_SETTINGS_CHEATS, "Enable &Cheats ([Cheats] in config.ini)");

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
  input_mapping_.reset_to_defaults();

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

    // Cache GDI drawing resources for zero-allocation rendering
    body_font_ = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             FIXED_PITCH | FF_MODERN, "Consolas");
    head_font_ = CreateFontA(-15, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             FIXED_PITCH | FF_MODERN, "Consolas");
    sidebar_bg_brush_ = CreateSolidBrush(RGB(13, 17, 23));
    sidebar_accent_brush_ = CreateSolidBrush(RGB(74, 144, 226));
    sidebar_rule_brush_ = CreateSolidBrush(RGB(45, 55, 70));
    black_brush_ = GetStockObject(BLACK_BRUSH);

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    RegisterTouchWindow(hwnd, 0);

    update_menu_checks();
  } else {
    std::cerr << "CreateWindowExA failed, error code: " << GetLastError() << std::endl;
  }
}

Window::~Window() {
  if (back_dc_ != nullptr) {
    if (back_bm_ != nullptr) {
      SelectObject(static_cast<HDC>(back_dc_), static_cast<HBITMAP>(back_bm_old_));
      DeleteObject(static_cast<HBITMAP>(back_bm_));
      back_bm_ = nullptr;
    }
    DeleteDC(static_cast<HDC>(back_dc_));
    back_dc_ = nullptr;
  }

  if (sidebar_dc_ != nullptr) {
    if (sidebar_bm_ != nullptr) {
      SelectObject(static_cast<HDC>(sidebar_dc_), static_cast<HBITMAP>(sidebar_bm_old_));
      DeleteObject(static_cast<HBITMAP>(sidebar_bm_));
      sidebar_bm_ = nullptr;
    }
    DeleteDC(static_cast<HDC>(sidebar_dc_));
    sidebar_dc_ = nullptr;
  }

  if (body_font_ != nullptr) {
    DeleteObject(static_cast<HFONT>(body_font_));
    body_font_ = nullptr;
  }
  if (head_font_ != nullptr) {
    DeleteObject(static_cast<HFONT>(head_font_));
    head_font_ = nullptr;
  }
  if (sidebar_bg_brush_ != nullptr) {
    DeleteObject(static_cast<HBRUSH>(sidebar_bg_brush_));
    sidebar_bg_brush_ = nullptr;
  }
  if (sidebar_accent_brush_ != nullptr) {
    DeleteObject(static_cast<HBRUSH>(sidebar_accent_brush_));
    sidebar_accent_brush_ = nullptr;
  }
  if (sidebar_rule_brush_ != nullptr) {
    DeleteObject(static_cast<HBRUSH>(sidebar_rule_brush_));
    sidebar_rule_brush_ = nullptr;
  }

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
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, FALSE);
  }
}

void Window::set_internal_resolution(InternalResolution res) {
  internal_resolution_ = res;
  update_menu_checks();

  switch (res) {
    case InternalResolution::Native:   resize_client(480, 320); break;
    case InternalResolution::Res_720p:  resize_client(960, 640); break;
    case InternalResolution::Res_1080p: resize_client(1200, 800); break;
    case InternalResolution::Res_4K:    resize_client(1440, 960); break;
  }

  if (hwnd_ != nullptr) {
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, FALSE);
  }
}

void Window::set_video_filter(VideoFilter filter) {
  video_filter_ = filter;
  update_menu_checks();

  if (hwnd_ != nullptr) {
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, FALSE);
  }
}

void Window::set_show_hud(bool show) {
  show_hud_ = show;
  update_menu_checks();
  if (hwnd_ != nullptr) {
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, TRUE);
  }
}

void Window::set_sidebar_enabled(bool enabled) {
  sidebar_enabled_ = enabled;
  update_menu_checks();
  if (hwnd_ != nullptr) {
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, TRUE);
  }
}

void Window::show_controls_dialog() {
  HINSTANCE instance = GetModuleHandleA(nullptr);
  HWND parent = static_cast<HWND>(hwnd_);

  DialogData data;
  data.temp_mapping = input_mapping_;

  RECT rect = {0, 0, 580, 540};
  AdjustWindowRect(&rect, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_DLGFRAME, FALSE);

  HWND dlg = CreateWindowExA(
      WS_EX_DLGMODALFRAME,
      "AWControlRemapDialogClass",
      "Configure Controls & Gamepad - AW-Recompiled",
      WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      rect.right - rect.left,
      rect.bottom - rect.top,
      parent,
      nullptr,
      instance,
      &data);

  if (dlg == nullptr) return;

  // Controller Selector Dropdown
  CreateWindowExA(0, "STATIC", "Active Gamepad Device:", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 15, 160, 20, dlg, nullptr, instance, nullptr);
  data.combo_controller = CreateWindowExA(0, "COMBOBOX", "",
                                          WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                          180, 12, 220, 140, dlg, nullptr, instance, nullptr);

  // Populate XInput Gamepads with connection status
  for (DWORD i = 0; i < 4; ++i) {
    XINPUT_STATE xs;
    std::string text = "Controller " + std::to_string(i + 1) + " (XInput)";
    if (XInputGetState(i, &xs) == ERROR_SUCCESS) {
      text += " [Connected]";
    }
    SendMessageA(data.combo_controller, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
  }
  SendMessageA(data.combo_controller, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Disabled (Keyboard Only)"));

  int sel_idx = data.temp_mapping.controller_index;
  if (sel_idx < 0 || sel_idx > 3) sel_idx = 4;
  SendMessageA(data.combo_controller, CB_SETCURSEL, sel_idx, 0);

  // Mouse Checkbox
  data.check_mouse = CreateWindowExA(0, "BUTTON", "Enable PC Mouse Navigation & Clicking",
                                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                      20, 42, 350, 20, dlg, nullptr, instance, nullptr);
  SendMessageA(data.check_mouse, BM_SETCHECK, data.temp_mapping.mouse_enabled ? BST_CHECKED : BST_UNCHECKED, 0);

  // Header Static Text
  CreateWindowExA(0, "STATIC", "Click any button to rebind Keyboard key or Gamepad button:",
                  WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 70, 520, 20, dlg, nullptr, instance, nullptr);

  // Column Headers
  CreateWindowExA(0, "STATIC", "GBA Control", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 95, 130, 20, dlg, nullptr, instance, nullptr);
  CreateWindowExA(0, "STATIC", "Keyboard Key", WS_CHILD | WS_VISIBLE | SS_CENTER, 160, 95, 170, 20, dlg, nullptr, instance, nullptr);
  CreateWindowExA(0, "STATIC", "Gamepad / XInput", WS_CHILD | WS_VISIBLE | SS_CENTER, 350, 95, 170, 20, dlg, nullptr, instance, nullptr);

  int y = 120;
  for (int i = 0; i < Gba_Count; ++i) {
    CreateWindowExA(0, "STATIC", gba_button_name(static_cast<GbaButton>(i)),
                    WS_CHILD | WS_VISIBLE | SS_LEFT, 20, y + 4, 130, 20, dlg, nullptr, instance, nullptr);

    data.key_buttons[i] = CreateWindowExA(0, "BUTTON", "",
                                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                          160, y, 170, 24, dlg, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(500 + i)), instance, nullptr);

    data.pad_buttons[i] = CreateWindowExA(0, "BUTTON", "",
                                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                          350, y, 170, 24, dlg, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(600 + i)), instance, nullptr);
    y += 30;
  }

  // Bottom Buttons
  CreateWindowExA(0, "BUTTON", "Restore Defaults", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  20, y + 10, 140, 30, dlg, reinterpret_cast<HMENU>(701), instance, nullptr);

  CreateWindowExA(0, "BUTTON", "OK / Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  280, y + 10, 110, 30, dlg, reinterpret_cast<HMENU>(702), instance, nullptr);

  CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  410, y + 10, 110, 30, dlg, reinterpret_cast<HMENU>(703), instance, nullptr);

  update_dialog_button_texts(&data);

  EnableWindow(parent, FALSE);

  MSG msg;
  XINPUT_STATE last_xstate;
  std::memset(&last_xstate, 0, sizeof(XINPUT_STATE));
  XInputGetState(0, &last_xstate);

  while (IsWindow(dlg)) {
    if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        PostQuitMessage(0);
        break;
      }
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    // Handle XInput button capture during listening mode
    if (data.listening_btn_index >= 0 && data.listening_is_pad) {
      XINPUT_STATE curr_state;
      if (XInputGetState(0, &curr_state) == ERROR_SUCCESS) {
        const WORD diff = curr_state.Gamepad.wButtons & ~last_xstate.Gamepad.wButtons;
        if (diff != 0) {
          data.temp_mapping.bindings[data.listening_btn_index].pad_button = diff;
          data.listening_btn_index = -1;
          update_dialog_button_texts(&data);
        }
        last_xstate = curr_state;
      }
    } else {
      XInputGetState(0, &last_xstate);
    }

    Sleep(10);
  }

  EnableWindow(parent, TRUE);
  SetForegroundWindow(parent);

  if (data.saved) {
    input_mapping_ = data.temp_mapping;
    ConfigFile config;
    config.load("config.ini");
    save_config(config);
  }
}

void Window::load_config(const ConfigFile& config) {
  const int aspect = config.get_int("Display", "aspect_ratio", 0);
  const int res = config.get_int("Display", "internal_resolution", 0);
  const int filter = config.get_int("Display", "video_filter", 1);
  const int hud = config.get_int("Display", "show_hud", 1);
  const int sidebar = config.get_int("Display", "sidebar", 1);
  const int input_display = config.get_int("Replay", "input_display", 0);
  const int hd_text = config.get_int("Display", "hd_text", 0);
  const int cheats = config.get_int("Cheats", "enabled", 0);
  const int vsync = config.get_int("Display", "vsync", 1);

  set_aspect_ratio(static_cast<AspectRatio>(std::clamp(aspect, 0, 5)));
  set_internal_resolution(static_cast<InternalResolution>(std::clamp(res, 0, 3)));
  set_video_filter(static_cast<VideoFilter>(std::clamp(filter, 0, 2)));
  set_show_hud(hud != 0);
  set_sidebar_enabled(sidebar != 0);
  set_input_display(input_display != 0);
  if (hd_text != 0) set_hd_text_enabled(true);
  set_cheats_enabled(cheats != 0);
  set_vsync(vsync != 0);

  input_mapping_.load_from_config(config);
}

void Window::save_config(ConfigFile& config) const {
  config.set_int("Display", "aspect_ratio", static_cast<int>(aspect_ratio_));
  config.set_int("Display", "internal_resolution", static_cast<int>(internal_resolution_));
  config.set_int("Display", "video_filter", static_cast<int>(video_filter_));
  config.set_int("Display", "show_hud", show_hud_ ? 1 : 0);
  config.set_int("Display", "sidebar", sidebar_enabled_ ? 1 : 0);
  config.set_int("Display", "vsync", vsync_ ? 1 : 0);
  config.set_int("Display", "hd_text", hd_text_enabled_ ? 1 : 0);
  config.set_int("Cheats", "enabled", cheats_enabled_ ? 1 : 0);
  config.set_int("Replay", "input_display", input_display_ ? 1 : 0);
  if (!pending_rom_path_.empty()) {
    config.set_string("Paths", "rom_path", pending_rom_path_);
  }
  input_mapping_.save_to_config(config);
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

  CheckMenuItem(hMenuBar, IDM_SETTINGS_TOGGLE_HUD, show_hud_ ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_SETTINGS_SIDEBAR, sidebar_enabled_ ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_SETTINGS_INPUT_DISPLAY, input_display_ ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_SETTINGS_HD_TEXT, hd_text_enabled_ ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_SETTINGS_CHEATS, cheats_enabled_ ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_FILE_REPLAY_RECORD, recording_ui_ ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenuBar, IDM_FILE_FASTFORWARD, fast_forward_latch_ ? MF_CHECKED : MF_UNCHECKED);
}



std::string Window::consume_pending_save_state() {
  std::string p = std::move(pending_save_state_path_);
  pending_save_state_path_.clear();
  return p;
}

std::string Window::consume_pending_load_state() {
  std::string p = std::move(pending_load_state_path_);
  pending_load_state_path_.clear();
  return p;
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

std::string Window::open_savestate_dialog(void* parent_hwnd) {
  char file_name[MAX_PATH] = "";
  OPENFILENAMEA ofn = {};
  ofn.lStructSize = sizeof(OPENFILENAMEA);
  ofn.hwndOwner = static_cast<HWND>(parent_hwnd);
  ofn.lpstrFilter = "Advance Wars Save States (*.ss)\0*.ss\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = file_name;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  ofn.lpstrTitle = "Select Save State File to Load";

  if (GetOpenFileNameA(&ofn) == TRUE) {
    return std::string(file_name);
  }
  return "";
}

std::string Window::save_savestate_dialog(void* parent_hwnd) {
  char file_name[MAX_PATH] = "state_0.ss";
  OPENFILENAMEA ofn = {};
  ofn.lStructSize = sizeof(OPENFILENAMEA);
  ofn.hwndOwner = static_cast<HWND>(parent_hwnd);
  ofn.lpstrFilter = "Advance Wars Save States (*.ss)\0*.ss\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = file_name;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
  ofn.lpstrTitle = "Save State As...";

  if (GetSaveFileNameA(&ofn) == TRUE) {
    return std::string(file_name);
  }
  return "";
}

std::string Window::open_replay_dialog(void* parent_hwnd) {
  char file_name[MAX_PATH] = "";
  OPENFILENAMEA ofn = {};
  ofn.lStructSize = sizeof(OPENFILENAMEA);
  ofn.hwndOwner = static_cast<HWND>(parent_hwnd);
  ofn.lpstrFilter = "AW-Recompiled Replays (*.awr)\0*.awr\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = file_name;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  ofn.lpstrTitle = "Select Replay File";

  if (GetOpenFileNameA(&ofn) == TRUE) {
    return std::string(file_name);
  }
  return "";
}

std::string Window::open_patch_dialog(void* parent_hwnd) {
  char file_name[MAX_PATH] = "";
  OPENFILENAMEA ofn = {};
  ofn.lStructSize = sizeof(OPENFILENAMEA);
  ofn.hwndOwner = static_cast<HWND>(parent_hwnd);
  ofn.lpstrFilter = "IPS Patches (*.ips)\0*.ips\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = file_name;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  ofn.lpstrTitle = "Select IPS Patch (applied on next boot)";

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
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }

  input_frame_.clear();
  RECT client_rect;
  GetClientRect(static_cast<HWND>(hwnd_), &client_rect);
  const int client_w = client_rect.right - client_rect.left;
  const int client_h = client_rect.bottom - client_rect.top;
  if (client_w > 0 && client_h > 0) {
    const SidebarLayout layout =
        calculate_sidebar_layout(client_w, client_h, sidebar_enabled_, aspect_ratio_);
    cached_viewport_ = layout.game;
  }

  win32_input_.set_window(hwnd_);
  win32_input_.set_mapping(&input_mapping_);
  win32_input_.set_viewport(cached_viewport_.x, cached_viewport_.y,
                            cached_viewport_.width, cached_viewport_.height);
  win32_input_.poll(input_frame_);

  hardware.keys_pressed |= input_frame_.gba_keys;

  // Time travel / fast-forward come through the polled input frame so both
  // keyboard (Backspace/Tab) and XInput (Y/X + triggers) drive them.
  rewind_held_ = (input_frame_.hotkeys & kHotkeyRewind) != 0;
  fast_forward_held_ = (input_frame_.hotkeys & kHotkeyFastForward) != 0;

  // Undo is edge-triggered: one press (Ctrl+Z or left-stick click) queues
  // exactly one restore.
  const bool undo_down = (input_frame_.hotkeys & kHotkeyUndo) != 0;
  if (undo_down && !undo_hotkey_was_down_) {
    undo_requested_ = true;
  }
  undo_hotkey_was_down_ = undo_down;

  // F5 hotkey: Quick Save State
  const bool f5_is_down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
  if (f5_is_down && !f5_key_was_down_) {
    int slot = 0;
    for (int digit = 0; digit <= 9; ++digit) {
      if (GetAsyncKeyState('0' + digit) & 0x8000) {
        slot = digit;
        break;
      }
    }
    request_save_state("state_" + std::to_string(slot) + ".ss");
    std::cout << "Quick Save State requested: state_" << slot << ".ss" << std::endl;
  }
  f5_key_was_down_ = f5_is_down;

  // F9 hotkey: Quick Load State
  const bool f9_is_down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
  if (f9_is_down && !f9_key_was_down_) {
    int slot = 0;
    for (int digit = 0; digit <= 9; ++digit) {
      if (GetAsyncKeyState('0' + digit) & 0x8000) {
        slot = digit;
        break;
      }
    }
    request_load_state("state_" + std::to_string(slot) + ".ss");
    std::cout << "Quick Load State requested: state_" << slot << ".ss" << std::endl;
  }
  f9_key_was_down_ = f9_is_down;

  // F2 hotkey: toggle Tactical Intel HUD overlay
  const bool f2_is_down = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
  if (f2_is_down && !f2_key_was_down_) {
    toggle_hud();
    ConfigFile config;
    config.load("config.ini");
    save_config(config);
    std::cout << "Tactical Intel HUD: " << (show_hud_ ? "ENABLED" : "DISABLED") << std::endl;
  }
  f2_key_was_down_ = f2_is_down;

  // F4 hotkey: toggle the tactical sidebar
  const bool f4_is_down = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
  if (f4_is_down && !f4_key_was_down_) {
    toggle_sidebar();
    ConfigFile config;
    config.load("config.ini");
    save_config(config);
    std::cout << "Tactical Sidebar: " << (sidebar_enabled_ ? "ENABLED" : "DISABLED") << std::endl;
  }
  f4_key_was_down_ = f4_is_down;

  // F6: toggle replay recording. F7: stop replay playback. F8: input display.
  const bool f6_is_down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
  if (f6_is_down && !f6_key_was_down_) record_toggle_requested_ = true;
  f6_key_was_down_ = f6_is_down;

  const bool f7_is_down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
  if (f7_is_down && !f7_key_was_down_) playback_stop_requested_ = true;
  f7_key_was_down_ = f7_is_down;

  const bool f8_is_down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
  if (f8_is_down && !f8_key_was_down_) {
    set_input_display(!input_display_);
    ConfigFile config;
    config.load("config.ini");
    save_config(config);
    std::cout << "Input Display: " << (input_display_ ? "ENABLED" : "DISABLED") << std::endl;
  }
  f8_key_was_down_ = f8_is_down;

  return is_open_;
}

bool Window::consume_undo_press() {
  if (!undo_requested_) return false;
  undo_requested_ = false;
  return true;
}

bool Window::consume_record_toggle() {
  if (!record_toggle_requested_) return false;
  record_toggle_requested_ = false;
  return true;
}

bool Window::consume_playback_stop() {
  if (!playback_stop_requested_) return false;
  playback_stop_requested_ = false;
  return true;
}

std::string Window::consume_pending_replay() {
  std::string path = std::move(pending_replay_path_);
  pending_replay_path_.clear();
  return path;
}

void Window::set_recording_ui(bool recording) {
  recording_ui_ = recording;
  update_menu_checks();
}

void Window::set_input_display(bool on) {
  input_display_ = on;
  update_menu_checks();
  if (hwnd_ != nullptr) {
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, TRUE);
  }
}

void Window::set_cheats_enabled(bool enabled) {
  cheats_enabled_ = enabled;
  update_menu_checks();
}

void Window::set_hd_text_enabled(bool enabled) {
  hd_text_enabled_ = enabled;
  if (enabled && !hd_text_pack_loaded_) {
    hd_text_pack_loaded_ = true;
    if (!hd_pack_.load("data/hd/tiles/tiles.ini")) {
      std::cout << "HD text: no pack at data/hd/tiles/tiles.ini (run aw-hd-capture to build one)"
                << std::endl;
    }
  }
  update_menu_checks();
  if (hwnd_ != nullptr) {
    InvalidateRect(static_cast<HWND>(hwnd_), nullptr, TRUE);
  }
}

bool Window::consume_rewind_step() {
  if (!rewind_step_requested_) return false;
  rewind_step_requested_ = false;
  return true;
}

void Window::toggle_fast_forward_latch() {
  fast_forward_latch_ = !fast_forward_latch_;
  update_menu_checks();
  std::cout << "Fast-forward " << (fast_forward_latch_ ? "ON (toggle)" : "OFF") << std::endl;
}

void Window::set_playback_indicator(int indicator) {
  if (indicator == playback_indicator_) return;
  playback_indicator_ = indicator;

  if (hwnd_ == nullptr) return;
  std::string title = window_title_;
  if (indicator < 0) {
    title += "  << TIME TRAVEL";
  } else if (indicator > 0) {
    title += "  >> FAST FORWARD";
  }
  SetWindowTextA(static_cast<HWND>(hwnd_), title.c_str());
}

void Window::draw_sidebar_panel(void* hdc_ptr, const SidebarData& data, const SidebarLayout& layout) {
  if (layout.sidebar.width <= 0 || layout.sidebar.height <= 0) return;
  auto* hdc = static_cast<HDC>(hdc_ptr);

  // Initialize/resize sidebar offscreen double buffer
  if (sidebar_dc_ == nullptr) {
    HDC screen_dc = GetDC(nullptr);
    sidebar_dc_ = CreateCompatibleDC(screen_dc);
    ReleaseDC(nullptr, screen_dc);
  }
  if (sidebar_bm_ == nullptr || sidebar_w_ != layout.sidebar.width || sidebar_h_ != layout.sidebar.height) {
    if (sidebar_bm_ != nullptr) {
      SelectObject(static_cast<HDC>(sidebar_dc_), static_cast<HBITMAP>(sidebar_bm_old_));
      DeleteObject(static_cast<HBITMAP>(sidebar_bm_));
    }
    HDC screen_dc = GetDC(nullptr);
    sidebar_bm_ = CreateCompatibleBitmap(screen_dc, layout.sidebar.width, layout.sidebar.height);
    ReleaseDC(nullptr, screen_dc);
    sidebar_bm_old_ = SelectObject(static_cast<HDC>(sidebar_dc_), static_cast<HBITMAP>(sidebar_bm_));
    sidebar_w_ = layout.sidebar.width;
    sidebar_h_ = layout.sidebar.height;
  }
  HDC target_dc = static_cast<HDC>(sidebar_dc_);

  // Panel background with an accent edge on the game side.
  RECT panel{0, 0, layout.sidebar.width, layout.sidebar.height};
  FillRect(target_dc, &panel, static_cast<HBRUSH>(sidebar_bg_brush_));

  RECT edge{0, 0, 2, layout.sidebar.height};
  FillRect(target_dc, &edge, static_cast<HBRUSH>(sidebar_accent_brush_));

  const auto lines = sidebar_panel_lines(data);
  int y = 14;
  const int x = 14;
  const int line_h = 20;
  for (const auto& [text, is_heading] : lines) {
    if (is_heading) {
      y += 8;  // Section break before headings.
      RECT rule_rect{x, y + line_h - 4, x + layout.sidebar.width - 28, y + line_h - 3};
      FillRect(target_dc, &rule_rect, static_cast<HBRUSH>(sidebar_rule_brush_));
    }
    RECT text_rect{x, y, x + layout.sidebar.width - 14, y + line_h + 4};
    SetTextColor(target_dc, is_heading ? RGB(245, 166, 35) : RGB(220, 228, 238));
    SetBkMode(target_dc, TRANSPARENT);
    SelectObject(target_dc, is_heading ? static_cast<HFONT>(head_font_) : static_cast<HFONT>(body_font_));
    DrawTextA(target_dc, text.c_str(), -1, &text_rect,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOCLIP | DT_PATH_ELLIPSIS);
    y += line_h;
  }

  // Blit the small sidebar directly into the window DC
  BitBlt(hdc, layout.sidebar.x, layout.sidebar.y, layout.sidebar.width, layout.sidebar.height,
         target_dc, 0, 0, SRCCOPY);
}

bool Window::ensure_back_buffer(int width, int height) {
  if (width <= 0 || height <= 0) return false;
  if (back_dc_ != nullptr && back_bm_ != nullptr && back_w_ == width && back_h_ == height) {
    return true;
  }

  if (back_dc_ == nullptr) {
    HDC screen_dc = GetDC(nullptr);
    back_dc_ = CreateCompatibleDC(screen_dc);
    ReleaseDC(nullptr, screen_dc);
    if (back_dc_ == nullptr) return false;
  }

  if (back_bm_ != nullptr) {
    SelectObject(static_cast<HDC>(back_dc_), static_cast<HBITMAP>(back_bm_old_));
    DeleteObject(static_cast<HBITMAP>(back_bm_));
    back_bm_ = nullptr;
  }

  // A DIB section (rather than a compatible bitmap) keeps StretchDIBits on
  // its fast path when the source is a 32-bit top-down DIB.
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;  // Top-down.
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP bm = CreateDIBSection(static_cast<HDC>(back_dc_), &bmi, DIB_RGB_COLORS,
                                &bits, nullptr, 0);
  if (bm == nullptr) return false;

  back_bm_ = static_cast<void*>(bm);
  back_bm_old_ = SelectObject(static_cast<HDC>(back_dc_), bm);
  back_w_ = width;
  back_h_ = height;
  back_needs_clear_ = true;
  return true;
}

void Window::render(const Ppu& ppu, const SidebarData& sidebar_data) {
  if (!is_open_ || hdc_ == nullptr) return;

  HWND hwnd = static_cast<HWND>(hwnd_);
  HDC window_dc = static_cast<HDC>(hdc_);

  RECT client_rect;
  GetClientRect(hwnd, &client_rect);
  const int client_w = client_rect.right - client_rect.left;
  const int client_h = client_rect.bottom - client_rect.top;

  if (client_w <= 0 || client_h <= 0) return;

  const SidebarLayout layout =
      calculate_sidebar_layout(client_w, client_h, sidebar_enabled_, aspect_ratio_);

  if (client_w != last_client_w_ || client_h != last_client_h_ ||
      aspect_ratio_ != last_aspect_ratio_ || layout.sidebar_shown != last_sidebar_enabled_) {
    last_client_w_ = client_w;
    last_client_h_ = client_h;
    last_aspect_ratio_ = aspect_ratio_;
    last_sidebar_enabled_ = layout.sidebar_shown;
    cached_viewport_ = layout.game;
    back_needs_clear_ = true;
  }
  if (cached_viewport_.width <= 0 || cached_viewport_.height <= 0) {
    cached_viewport_ = layout.game;
  }

  // Compose into the offscreen buffer, then present in one blit. Falling back
  // to the window DC keeps rendering alive if the buffer cannot be created.
  const bool buffered = ensure_back_buffer(client_w, client_h);
  HDC hdc = buffered ? static_cast<HDC>(back_dc_) : window_dc;

  // Only the letterbox bars need repainting, and only when the layout moved:
  // the game rect and sidebar together cover everything else every frame.
  if (back_needs_clear_) {
    RECT full{0, 0, client_w, client_h};
    FillRect(hdc, &full, static_cast<HBRUSH>(black_brush_));
    back_needs_clear_ = false;
  }

  SetStretchBltMode(hdc, COLORONCOLOR);

  const std::uint32_t* render_data = ppu.framebuffer.data();
  int src_w = ppu.width;
  int src_h = ppu.height;

  // HD text: upscale 2x, then let the pack overwrite matched glyph blocks
  // with 16x16 replacements. Works on top of any base filter.
  if (hd_text_enabled_ && hd_pack_.loaded()) {
    scale2x_buffer_.resize(static_cast<std::size_t>(src_w * 2) * (src_h * 2));
    for (int y = 0; y < src_h * 2; ++y) {
      for (int x = 0; x < src_w * 2; ++x) {
        scale2x_buffer_[static_cast<std::size_t>(y) * (src_w * 2) + x] =
            ppu.framebuffer[(y / 2) * src_w + (x / 2)];
      }
    }
    apply_hd_text(ppu.framebuffer.data(), src_w, src_h,
                  scale2x_buffer_.data(), src_w * 2, hd_pack_);
    render_data = scale2x_buffer_.data();
    src_w = src_w * 2;
    src_h = src_h * 2;
  } else if (video_filter_ == VideoFilter::Scale2x) {
    scale2x_buffer_.resize(static_cast<std::size_t>(src_w * 2) * (src_h * 2));
    apply_scale2x(ppu.framebuffer.data(), scale2x_buffer_.data(), src_w, src_h);
    render_data = scale2x_buffer_.data();
    src_w = src_w * 2;
    src_h = src_h * 2;
  } else if (video_filter_ == VideoFilter::Bilinear) {
    scale2x_buffer_.resize(static_cast<std::size_t>(src_w * 2) * (src_h * 2));
    apply_bilinear_2x(ppu.framebuffer.data(), scale2x_buffer_.data(), src_w, src_h);
    render_data = scale2x_buffer_.data();
    src_w = src_w * 2;
    src_h = src_h * 2;
  }

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = src_w;
  bmi.bmiHeader.biHeight = -src_h; // Top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  const ViewportRect& vp = cached_viewport_;

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

  if (layout.sidebar_shown) {
    draw_sidebar_panel(hdc, sidebar_data, layout);
  }

  if (buffered) {
    // One atomic present of the finished frame.
    BitBlt(window_dc, 0, 0, client_w, client_h, hdc, 0, 0, SRCCOPY);
  }

}

double Window::display_refresh_hz() const {
  if (hwnd_ == nullptr) return 0.0;

  MONITORINFOEXA info = {};
  info.cbSize = sizeof(MONITORINFOEXA);
  HMONITOR mon = MonitorFromWindow(static_cast<HWND>(hwnd_), MONITOR_DEFAULTTONEAREST);
  if (mon == nullptr || !GetMonitorInfoA(mon, &info)) return 0.0;

  DEVMODEA dm = {};
  dm.dmSize = sizeof(DEVMODEA);
  if (!EnumDisplaySettingsA(info.szDevice, ENUM_CURRENT_SETTINGS, &dm)) return 0.0;
  if (dm.dmDisplayFrequency <= 1) return 0.0;

  // Windows reports whole Hz, so the common 59.94 modes come back as 60.
  return static_cast<double>(dm.dmDisplayFrequency);
}

bool Window::present_wait() {
  if (!is_open_ || !vsync_) return false;
  // Hand the finished frame to the compositor and wait until it has been
  // picked up. Without this the loop free-runs against the refresh, so frames
  // land at a drifting phase and the cadence visibly stutters even though no
  // frame is ever late.
  GdiFlush();
  const auto before = std::chrono::steady_clock::now();
  const bool ok = SUCCEEDED(DwmFlush());
  const double waited_ms =
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - before).count();
  // A flush that returns immediately did not pace anything (compositor off,
  // window minimised); the caller must fall back to its own clock.
  return ok && waited_ms > 2.0;
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
void Window::render(const Ppu& /*ppu*/, const SidebarData& /*sidebar*/) {}
bool Window::ensure_back_buffer(int /*width*/, int /*height*/) { return false; }
bool Window::present_wait() { return false; }
double Window::display_refresh_hz() const { return 0.0; }
void Window::set_aspect_ratio(AspectRatio ratio) { aspect_ratio_ = ratio; }
void Window::set_internal_resolution(InternalResolution res) { internal_resolution_ = res; }
void Window::set_video_filter(VideoFilter filter) { video_filter_ = filter; }
void Window::show_controls_dialog() {}
void Window::load_config(const ConfigFile& /*config*/) {}
void Window::save_config(ConfigFile& /*config*/) const {}
void Window::resize_client(int /*width*/, int /*height*/) {}
std::string Window::consume_pending_rom() { return ""; }
std::string Window::consume_pending_save_state() { return ""; }
std::string Window::consume_pending_load_state() { return ""; }
bool Window::consume_rewind_step() { return false; }
bool Window::consume_undo_press() { return false; }
bool Window::consume_record_toggle() { return false; }
bool Window::consume_playback_stop() { return false; }
std::string Window::consume_pending_replay() { return ""; }
void Window::set_recording_ui(bool /*recording*/) {}
void Window::set_input_display(bool /*on*/) {}
void Window::set_hd_text_enabled(bool /*enabled*/) {}
void Window::set_cheats_enabled(bool /*enabled*/) {}
void Window::set_playback_indicator(int /*indicator*/) {}
void Window::draw_sidebar_panel(void* /*hdc*/, const SidebarData& /*data*/,
                                const SidebarLayout& /*layout*/) {}
void Window::set_sidebar_enabled(bool /*enabled*/) {}
std::string Window::open_file_dialog(void* /*parent_hwnd*/) { return ""; }
std::string Window::open_savestate_dialog(void* /*parent_hwnd*/) { return ""; }
std::string Window::save_savestate_dialog(void* /*parent_hwnd*/) { return ""; }
std::string Window::open_replay_dialog(void* /*parent_hwnd*/) { return ""; }
std::string Window::open_patch_dialog(void* /*parent_hwnd*/) { return ""; }
void Window::update_menu_checks() {}

#endif


void apply_nearest_k(const std::uint32_t* src, std::uint32_t* dst, int w, int h, int k) {
  if (src == nullptr || dst == nullptr || w <= 0 || h <= 0 || k < 1) return;
  const int out_w = w * k;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const std::uint32_t px = src[static_cast<std::size_t>(y) * w + x];
      std::uint32_t* row = &dst[static_cast<std::size_t>(y * k) * out_w + x * k];
      for (int dy = 0; dy < k; ++dy) {
        for (int dx = 0; dx < k; ++dx) {
          row[static_cast<std::size_t>(dy) * out_w + dx] = px;
        }
      }
    }
  }
}

}  // namespace aw
