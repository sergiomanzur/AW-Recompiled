#include "aw/window.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <iostream>

namespace aw {

#ifdef _WIN32

namespace {

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_DESTROY || msg == WM_CLOSE) {
    PostQuitMessage(0);
    return 0;
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

  RECT rect = {0, 0, width, height};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

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
      nullptr,
      instance,
      nullptr);

  if (hwnd != nullptr) {
    hwnd_ = static_cast<void*>(hwnd);
    hdc_ = static_cast<void*>(GetDC(hwnd));
    is_open_ = true;
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

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = kGbaWidth;
  bmi.bmiHeader.biHeight = -kGbaHeight; // Top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  RECT client_rect;
  GetClientRect(static_cast<HWND>(hwnd_), &client_rect);

  StretchDIBits(
      static_cast<HDC>(hdc_),
      0,
      0,
      client_rect.right - client_rect.left,
      client_rect.bottom - client_rect.top,
      0,
      0,
      kGbaWidth,
      kGbaHeight,
      ppu.framebuffer.data(),
      &bmi,
      DIB_RGB_COLORS,
      SRCCOPY);
}

#else

Window::Window(int width, int height, const char* /*title*/)
    : is_open_(false), width_(width), height_(height) {}

Window::~Window() {}

bool Window::process_events(Hardware& /*hardware*/) {
  return false;
}

void Window::render(const Ppu& /*ppu*/) {}

#endif

}  // namespace aw
