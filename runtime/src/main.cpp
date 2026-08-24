#include "aw/arm_decode.hpp"
#include "aw/audio.hpp"
#include "aw/config.hpp"
#include "aw/config_file.hpp"
#include "aw/cpu_interpreter.hpp"
#include "aw/cpu_state.hpp"
#include "aw/generated_blocks.hpp"
#include "aw/hardware.hpp"
#include "aw/nav/nav_controller.hpp"
#include "aw/ppu.hpp"
#include "aw/probe/backend_mgba.hpp"
#include "aw/probe/oam.hpp"
#include "aw/rom.hpp"
#include "aw/window.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <timeapi.h>
#endif

#include "aw/mgba_adapter.h"

namespace {

std::string hex32(std::uint32_t value) {
  std::ostringstream out;
  out << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << value;
  return out.str();
}

bool pause_disabled() {
#ifdef _WIN32
  char* value = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&value, &length, "AW_NATIVE_NO_PAUSE") != 0 || value == nullptr) {
    return false;
  }

  const std::string text(value);
  std::free(value);
  return text == "1";
#else
  const auto* value = std::getenv("AW_NATIVE_NO_PAUSE");
  return value != nullptr && std::string(value) == "1";
#endif
}

void wait_before_close(bool enabled) {
  if (!enabled || pause_disabled()) {
    return;
  }

  std::cout << "Press Enter to exit.\n";
  std::cin.get();
}

struct Options {
  std::filesystem::path rom_path;
  bool pause_on_exit = false;
  bool trace_enabled = false;
  bool play_enabled = true;
  bool is_double_click = false;
  int max_frames = 0;  // 0 = unlimited (interactive play)
  std::string oam_log_path;  // Non-empty enables per-frame OAM delta logging
};

Options parse_options(int argc, char** argv) {
  Options options;
  std::vector<std::string> positional;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--trace") {
      options.trace_enabled = true;
      options.play_enabled = false;
    } else if (arg == "--play") {
      options.play_enabled = true;
    } else if (arg.rfind("--frames=", 0) == 0) {
      options.max_frames = (std::max)(0, std::stoi(arg.substr(9)));
    } else if (arg == "--frames" && i + 1 < argc) {
      options.max_frames = (std::max)(0, std::stoi(argv[++i]));
    } else if (arg.rfind("--oam-log=", 0) == 0) {
      options.oam_log_path = arg.substr(10);
    } else if (arg == "--oam-log" && i + 1 < argc) {
      options.oam_log_path = argv[++i];
    } else {
      positional.push_back(arg);
    }
  }

  if (positional.size() > 1) {
    throw std::runtime_error("usage: advance-wars-native [path-to-gba-rom] [--trace] [--play] [--frames N]");
  }

  aw::ConfigFile config;
  config.load("config.ini");

  if (!positional.empty()) {
    options.rom_path = positional.front();
    config.set_string("Paths", "rom_path", options.rom_path.string());
    config.save("config.ini");
  } else {
    std::string saved_rom = config.get_string("Paths", "rom_path", "");
    if (!saved_rom.empty() && std::filesystem::exists(saved_rom)) {
      options.rom_path = saved_rom;
    } else if (std::filesystem::exists(AW_DEFAULT_ROM_PATH)) {
      options.rom_path = AW_DEFAULT_ROM_PATH;
      config.set_string("Paths", "rom_path", options.rom_path.string());
      config.save("config.ini");
    } else {
#ifdef _WIN32
      MessageBoxA(
          nullptr,
          "No Advance Wars ROM file path configured.\n\nPlease select your Advance Wars (USA) GBA ROM file to continue.",
          "AW-Recompiled Startup",
          MB_OK | MB_ICONINFORMATION);
#endif
      const std::string selected_rom = aw::Window::open_file_dialog();
      if (!selected_rom.empty() && std::filesystem::exists(selected_rom)) {
        options.rom_path = selected_rom;
        config.set_string("Paths", "rom_path", selected_rom);
        config.save("config.ini");
      } else {
        throw std::runtime_error("No valid Advance Wars ROM file selected. AW-Recompiled requires a ROM file to run.");
      }
    }
    options.pause_on_exit = true;
    options.is_double_click = true;
  }

  return options;
}

void run_game_loop(std::filesystem::path rom_path, aw::RomImage rom, int max_frames,
                   const std::string& oam_log_path) {
  try {
    auto ppu_ptr = std::make_unique<aw::Ppu>();
    auto& ppu = *ppu_ptr;
    aw::Hardware hardware;
    aw::Window window(960, 640, "Advance Wars (Native Recomp)");
    aw::Audio audio;

    // Load config.ini settings into window (Aspect Ratio, Internal Res, Video Filter)
    aw::ConfigFile config;
    if (config.load("config.ini")) {
      window.load_config(config);
    }

    std::vector<std::uint32_t> mgba_buffer(240 * 160, 0);
    struct mCore* core = aw_mgba_create(rom_path.string().c_str(), mgba_buffer.data(), 240);
    if (!core) {
      throw std::runtime_error("aw_mgba_create failed for ROM: " + rom_path.string());
    }

    aw::MgbaProbeBackend probe(core);

    // NavController stays free of mGBA/Windows dependencies (Spec 2/3 reuse
    // it on SDL and Android), so main.cpp owns the concrete backend and
    // wires it in explicitly rather than the controller constructing one
    // itself.
    aw::NavController nav;
    nav.set_backend(probe);
    nav.load_symbols(aw::sha1_hex(rom.bytes));

    std::ofstream oam_log;
    std::vector<aw::OamEntry> oam_prev(aw::kOamEntryCount);
    if (!oam_log_path.empty()) {
      oam_log.open(oam_log_path);
      oam_log << "# frame keys index dx dy x y tile palette\n";
    }

    const unsigned core_sample_rate = aw_mgba_audio_sample_rate(core);
    std::cout << "Core audio sample rate: " << core_sample_rate << " Hz (audio backend "
              << (audio.is_active() ? "active" : "inactive") << ")\n";

    std::vector<std::int16_t> audio_samples(4096 * 2);
    std::uint64_t total_audio_samples = 0;
    std::uint64_t frames_run = 0;

    using clock = std::chrono::steady_clock;
    const double gba_fps = 16777216.0 / 280896.0; // ~59.7275 FPS
    const std::chrono::nanoseconds frame_duration_ns(static_cast<std::int64_t>(1000000000.0 / gba_fps));
    auto next_frame_time = clock::now();

    while (window.is_open()) {
      if (window.has_pending_rom()) {
        const std::string new_rom_path = window.consume_pending_rom();
        std::cout << "Switching to new ROM: " << new_rom_path << std::endl;
        aw_mgba_destroy(core);
        rom_path = new_rom_path;
        core = aw_mgba_create(rom_path.string().c_str(), mgba_buffer.data(), 240);
        if (!core) {
          throw std::runtime_error("aw_mgba_create failed for new ROM: " + rom_path.string());
        }
        // The backend caches raw block pointers that belong to the old core;
        // re-resolve them, then drop any tracking state that referred to the
        // previous ROM's OAM/context.
        probe.set_core(core);
        nav.reset();
      }

      hardware.keys_pressed = 0;
      if (!window.process_events(hardware)) {
        break;
      }

      hardware.keys_pressed |= nav.update(window.input_frame());

      aw_mgba_run_frame(core, hardware.keys_pressed);
      frames_run++;

      // Check for audio sample rate changes from core (e.g. SOUNDBIAS 32768 Hz <-> 65536 Hz)
      const unsigned current_rate = aw_mgba_audio_sample_rate(core);
      if (current_rate != 0 && current_rate != audio.sample_rate()) {
        audio.set_sample_rate(current_rate);
      }

      // Read audio samples from mGBA core
      const std::size_t samples_read = aw_mgba_read_audio(core, audio_samples.data(), audio_samples.size() / 2);
      if (samples_read > 0) {
        audio.push_samples(audio_samples.data(), samples_read);
        total_audio_samples += samples_read;
      }

      // Update PPU framebuffer from mGBA video buffer with proper R/B channel mapping for Win32 GDI
      const std::uint32_t* src_buf = mgba_buffer.data();
      std::uint32_t* dst_buf = ppu.framebuffer.data();
      for (std::size_t i = 0; i < 240 * 160; ++i) {
        const std::uint32_t c = src_buf[i];
        dst_buf[i] = ((c & 0x000000FF) << 16) | (c & 0x0000FF00) | ((c & 0x00FF0000) >> 16);
      }

      // Render frame to window
      window.render(ppu);

      if (oam_log.is_open()) {
        const std::uint8_t* oam_bytes = probe.oam();
        if (oam_bytes != nullptr) {
          for (std::size_t i = 0; i < aw::kOamEntryCount; ++i) {
            const aw::OamEntry cur = aw::decode_oam_entry(oam_bytes, i);
            const aw::OamEntry& prev = oam_prev[i];
            const int dx = cur.x - prev.x;
            const int dy = cur.y - prev.y;
            if (cur.on_screen() && prev.on_screen() && (dx != 0 || dy != 0)) {
              oam_log << frames_run << ' ' << hardware.keys_pressed << ' ' << i << ' '
                      << dx << ' ' << dy << ' ' << cur.x << ' ' << cur.y << ' '
                      << cur.tile << ' ' << cur.palette << '\n';
            }
            oam_prev[i] = cur;
          }
        }
      }

      // High-precision steady frame pacing for locked 60 FPS
      next_frame_time += frame_duration_ns;
      const auto now = clock::now();
      if (next_frame_time > now) {
        std::this_thread::sleep_for(next_frame_time - now);
      } else if (now - next_frame_time > std::chrono::milliseconds(100)) {
        next_frame_time = now;
      }

      if (max_frames > 0 && frames_run >= static_cast<std::uint64_t>(max_frames)) {
        break;
      }
    }

    std::cout << "Ran " << frames_run << " frames; audio samples emitted: " << total_audio_samples << std::endl;
    aw_mgba_destroy(core);
  } catch (const std::exception& e) {
    std::cerr << "Error in run_game_loop: " << e.what() << std::endl;
  }
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  // High-precision multimedia timer (1 ms accuracy for smooth frame pacing)
  timeBeginPeriod(1);
#endif

  int exit_code = 0;
  bool pause_on_exit = false;

  try {
    Options options = parse_options(argc, argv);
    pause_on_exit = options.pause_on_exit;

    aw::RomImage rom = aw::load_rom_file(options.rom_path);

    if (options.play_enabled) {
      run_game_loop(options.rom_path, rom, options.max_frames, options.oam_log_path);
    }
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    exit_code = 1;
  }

#ifdef _WIN32
  timeEndPeriod(1);
#endif

  wait_before_close(pause_on_exit);
  return exit_code;
}