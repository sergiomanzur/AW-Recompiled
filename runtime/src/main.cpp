#include "aw/arm_decode.hpp"
#include "aw/audio.hpp"
#include "aw/config.hpp"
#include "aw/cpu_interpreter.hpp"
#include "aw/cpu_state.hpp"
#include "aw/generated_blocks.hpp"
#include "aw/hardware.hpp"
#include "aw/ppu.hpp"
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
      options.max_frames = std::max(0, std::stoi(arg.substr(9)));
    } else if (arg == "--frames" && i + 1 < argc) {
      options.max_frames = std::max(0, std::stoi(argv[++i]));
    } else {
      positional.push_back(arg);
    }
  }

  if (positional.size() > 1) {
    throw std::runtime_error("usage: advance-wars-native [path-to-gba-rom] [--trace] [--play] [--frames N]");
  }

  if (positional.empty()) {
    options.rom_path = AW_DEFAULT_ROM_PATH;
    options.pause_on_exit = true;
    options.is_double_click = true;
  } else {
    options.rom_path = positional.front();
  }

  return options;
}

void run_game_loop(const std::filesystem::path& rom_path, const aw::RomImage& rom, int max_frames) {
  try {
    auto ppu_ptr = std::make_unique<aw::Ppu>();
    auto& ppu = *ppu_ptr;
    aw::Hardware hardware;
    aw::Window window(960, 640, "Advance Wars (Native Recomp)");
    aw::Audio audio;

    std::vector<std::uint32_t> mgba_buffer(240 * 160, 0);
    struct mCore* core = aw_mgba_create(rom_path.string().c_str(), mgba_buffer.data(), 240);
    if (!core) {
      throw std::runtime_error("aw_mgba_create failed for ROM: " + rom_path.string());
    }

    const unsigned core_sample_rate = aw_mgba_audio_sample_rate(core);
    std::cout << "Core audio sample rate: " << core_sample_rate << " Hz (audio backend "
              << (audio.is_active() ? "active" : "inactive") << ")\n";

    // GBA runs at 32768 Hz; the waveOut backend is opened at the same rate, so
    // no resampling is required. Pull a few frames' worth of samples per tick.
    std::vector<std::int16_t> audio_samples(2048 * 2);
    std::uint64_t total_audio_samples = 0;
    std::uint64_t frames_run = 0;

    using clock = std::chrono::steady_clock;
    constexpr auto frame_duration = std::chrono::duration_cast<clock::duration>(std::chrono::microseconds(1000000 / 60));
    auto next_frame_time = clock::now();

    while (window.is_open()) {
      if (!window.process_events(hardware)) {
        break;
      }

      aw_mgba_run_frame(core, hardware.keys_pressed);

      // Convert the core's XBGR8 framebuffer to the BGRA layout StretchDIBits expects.
      for (int y = 0; y < 160; ++y) {
        for (int x = 0; x < 240; ++x) {
          const std::uint32_t c = mgba_buffer[y * 240 + x];
          const std::uint8_t r = (c >> 0) & 0xFF;
          const std::uint8_t g = (c >> 8) & 0xFF;
          const std::uint8_t b = (c >> 16) & 0xFF;
          ppu.framebuffer[y * 240 + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
      }

      // Drain the core's audio buffer into the waveOut backend.
      const std::size_t samples_read = aw_mgba_read_audio(core, audio_samples.data(), 2048);
      if (samples_read > 0) {
        audio.push_samples(audio_samples.data(), static_cast<int>(samples_read));
        total_audio_samples += samples_read;
      }

      window.render(ppu);
      ++frames_run;
      if (max_frames > 0 && frames_run >= static_cast<std::uint64_t>(max_frames)) {
        break;
      }

      // Throttle to ~60 FPS.
      next_frame_time += frame_duration;
      const auto now = clock::now();
      if (now < next_frame_time) {
        std::this_thread::sleep_until(next_frame_time);
      } else {
        // Running behind; reset the deadline to avoid a death spiral.
        next_frame_time = now;
      }
    }

    aw_mgba_destroy(core);
    std::cout << "Ran " << frames_run << " frames; audio samples emitted: " << total_audio_samples << '\n';
  } catch (const std::exception& ex) {
    std::ofstream err_out("run_error.log");
    err_out << "run_game_loop error: " << ex.what() << std::endl;
    std::cerr << "run_game_loop error: " << ex.what() << '\n';
  }
}

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS* ep) {
  std::ofstream crash_out("crash.log");
  crash_out << "CRASH DETECTED! ExceptionCode: 0x" << std::hex << ep->ExceptionRecord->ExceptionCode
            << " at address: 0x" << ep->ExceptionRecord->ExceptionAddress << std::endl;
  std::cerr << "\nCRASH DETECTED! ExceptionCode: 0x" << std::hex << ep->ExceptionRecord->ExceptionCode
            << " at address: 0x" << ep->ExceptionRecord->ExceptionAddress << std::endl;

  void* stack[64];
  const WORD frames = CaptureStackBackTrace(0, 64, stack, nullptr);
  crash_out << "Stack frames (" << frames << "):" << std::endl;
  for (WORD i = 0; i < frames; ++i) {
    crash_out << "  [" << i << "] 0x" << std::hex << reinterpret_cast<std::uintptr_t>(stack[i]) << std::endl;
    std::cerr << "  [" << i << "] 0x" << std::hex << reinterpret_cast<std::uintptr_t>(stack[i]) << std::endl;
  }
  return EXCEPTION_EXECUTE_HANDLER;
}
#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  SetUnhandledExceptionFilter(unhandled_exception_handler);
#endif
  std::cout << "Starting advance-wars-native...\n";
  std::cout.flush();
  bool pause_on_exit = false;

  try {
    const auto options = parse_options(argc, argv);
    pause_on_exit = options.pause_on_exit;

    const auto rom = aw::load_rom_file(options.rom_path);
    const auto header = aw::parse_header(rom.bytes);
    const auto hash = aw::sha1_hex(rom.bytes);

    if (!aw::is_expected_advance_wars_rev1(rom)) {
      std::cerr << "unsupported ROM: expected Advance Wars USA Rev 1 SHA1 "
                << "15053499D5B3F49128A941D7F2D84876F5424D0C\n";
      std::cerr << "actual SHA1: " << hash << '\n';
      return 1;
    }

    constexpr std::uint32_t reset_pc = 0x08000000;
    const auto instruction = aw::read_le32(rom.bytes, 0);
    const auto reset_branch = aw::decode_arm_branch(instruction, reset_pc);

    std::cout << "Advance Wars native recomp milestone 1\n";
    std::cout << "ROM: " << header.title << ' ' << header.game_code << header.maker_code
              << " Rev " << static_cast<int>(header.version) << '\n';
    std::cout << "SHA1: " << hash << '\n';
    std::cout << "Reset branch: " << hex32(reset_pc) << " -> " << hex32(reset_branch.target) << '\n';

    if (options.trace_enabled) {
      auto state_ptr = std::make_unique<aw::CpuState>();
      auto& state = *state_ptr;
      state.trace_enabled = true;
      aw::generated::block_080000C0(state);
      for (int i = 0; i < 136; ++i) {
        aw::generated::dispatch_one(state);
      }

      for (const auto& line : state.trace_lines) {
        std::cout << line << '\n';
      }
      std::cout << "Stopped at unresolved target " << hex32(state.stop_target) << '\n';
    } else {
      run_game_loop(options.rom_path, rom, options.max_frames);
    }

    wait_before_close(pause_on_exit);
  } catch (const std::exception& ex) {
    std::ofstream err_out("run_error.log");
    err_out << "Exception caught: " << ex.what() << std::endl;
    std::cerr << ex.what() << '\n';
    wait_before_close(pause_on_exit);
    return 1;
  }

  return 0;
}