#include "aw/arm_decode.hpp"
#include "aw/audio.hpp"
#include "aw/config.hpp"
#include "aw/cheats.hpp"
#include "aw/config_file.hpp"
#include "aw/cpu_interpreter.hpp"
#include "aw/cpu_state.hpp"
#include "aw/generated_blocks.hpp"
#include "aw/hardware.hpp"
#include "aw/ips.hpp"
#include "aw/map_sensor.hpp"
#include "aw/nav/nav_controller.hpp"
#include "aw/hd_audio.hpp"
#include "aw/order_stack.hpp"
#include "aw/ppu.hpp"
#include "aw/probe/backend_mgba.hpp"
#include "aw/probe/cursor_probe.hpp"
#include "aw/probe/oam.hpp"
#include "aw/render/hud_overlay.hpp"
#include "aw/render/pointer_overlay.hpp"
#include "aw/render/sidebar.hpp"
#include "aw/replay.hpp"
#include "aw/rewind.hpp"
#include "aw/rom.hpp"
#include "aw/tactical_intel.hpp"
#include "aw/window.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
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

// ---------------------------------------------------------------------------
// Temporary frame-phase profiler (AW_PERF=1). Measures where the per-frame
// wall clock actually goes so optimization targets are evidence, not guesses.
// ---------------------------------------------------------------------------
enum PerfSlot {
  PF_EVENTS, PF_PROBE, PF_EMU, PF_REWIND, PF_AUDIO, PF_COPY, PF_OVERLAY, PF_RENDER, PF_VSYNC, PF_PACE, PF_COUNT
};
static const char* kPerfNames[PF_COUNT] = {
  "events", "probe", "emu", "rewind", "audio", "copy", "overlay", "render", "vsync", "pace"
};

struct PerfCollector {
  bool enabled = false;
  double acc[PF_COUNT] = {};
  double worst[PF_COUNT] = {};
  double work_worst = 0.0;
  double work_acc = 0.0;
  std::uint64_t frames = 0;
  std::uint64_t over_budget = 0;
  std::chrono::steady_clock::time_point mark;
  std::chrono::steady_clock::time_point frame_start;
  std::chrono::steady_clock::time_point window_start;
  bool window_started = false;

  void init() {
#ifdef _WIN32
    char* v = nullptr; std::size_t n = 0;
    if (_dupenv_s(&v, &n, "AW_PERF") == 0 && v != nullptr) {
      enabled = (std::string(v) == "1");
      std::free(v);
    }
#else
    const char* v = std::getenv("AW_PERF");
    enabled = (v != nullptr && std::string(v) == "1");
#endif
  }

  void begin() {
    if (!enabled) return;
    frame_start = std::chrono::steady_clock::now();
    mark = frame_start;
    if (!window_started) {
      window_start = frame_start;
      window_started = true;
    }
  }

  void lap(int slot) {
    if (!enabled) return;
    const auto now = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(now - mark).count();
    mark = now;
    acc[slot] += ms;
    if (ms > worst[slot]) worst[slot] = ms;
  }

  // Called after the render lap, before pacing: everything that must fit in
  // the 16.74 ms budget.
  void end_work() {
    if (!enabled) return;
    const double ms =
        std::chrono::duration<double, std::milli>(mark - frame_start).count();
    work_acc += ms;
    if (ms > work_worst) work_worst = ms;
    if (ms > 16.743) ++over_budget;
    ++frames;
  }

  int audio_queued = 0;

  void report_if_due() {
    if (!enabled || frames < 120) return;
    const double f = static_cast<double>(frames);
    const double elapsed_s = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - window_start).count();
    std::fprintf(stderr,
                 "[perf] %llu frames | ACHIEVED %.3f fps | queued %d | work avg %.2f ms peak %.2f ms | late %llu\n",
                 static_cast<unsigned long long>(frames),
                 elapsed_s > 0.0 ? f / elapsed_s : 0.0,
                 audio_queued,
                 work_acc / f, work_worst,
                 static_cast<unsigned long long>(over_budget));
    for (int i = 0; i < PF_COUNT; ++i) {
      std::fprintf(stderr, "[perf]   %-8s avg %6.3f ms  peak %7.3f ms\n",
                   kPerfNames[i], acc[i] / f, worst[i]);
    }
    std::fflush(stderr);
    for (int i = 0; i < PF_COUNT; ++i) { acc[i] = 0.0; worst[i] = 0.0; }
    work_acc = 0.0; work_worst = 0.0; frames = 0; over_budget = 0;
    window_started = false;
  }
};

bool is_battlefield_map(aw::ProbeBackend& backend, const aw::CursorAddresses& addrs) {
  if (!addrs.valid()) return false;

  const std::uint16_t dispcnt = backend.read_io16(0x04000000);
  const std::uint16_t bg2cnt = backend.read_io16(0x0400000C);

  // Advance Wars live map engine runs in Mode 0 (tiled text mode) with BG2 enabled (0x0400).
  // On title screen and main menus, DISPCNT is Mode 4 bitmap (0x1F44 / 0x1C44) and BG2CNT is 0.
  if ((dispcnt & 7) != 0 || (dispcnt & 0x0400) == 0 || bg2cnt == 0) {
    return false;
  }

  std::size_t iw_size = 0;
  const std::uint8_t* iwram = backend.iwram(iw_size);
  if (iwram == nullptr) return false;

  constexpr std::uint32_t kBase = 0x03000000;
  if (addrs.x_addr < kBase || addrs.y_addr + 2 > kBase + iw_size) return false;

  const std::size_t cur_x_off = addrs.x_addr - kBase;
  const std::size_t cur_y_off = addrs.y_addr - kBase;
  const std::size_t map_active_off = (addrs.x_addr + 8) - kBase;

  if (cur_x_off + 2 > iw_size || cur_y_off + 2 > iw_size || map_active_off >= iw_size) {
    return false;
  }

  // Active match flag (0x030036AC is strictly 1 during active battlefield gameplay).
  // On Name Entry, Main Menu, and Title screens, this byte is 0.
  if (iwram[map_active_off] != 1) {
    return false;
  }

  const std::uint16_t tx = static_cast<std::uint16_t>(iwram[cur_x_off]) |
                           (static_cast<std::uint16_t>(iwram[cur_x_off + 1]) << 8);
  const std::uint16_t ty = static_cast<std::uint16_t>(iwram[cur_y_off]) |
                           (static_cast<std::uint16_t>(iwram[cur_y_off + 1]) << 8);

  if (tx >= 50 || ty >= 50) return false;

  return true;
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
  bool rewind_smoke = false;  // Exercise the rewind ring headlessly
  std::string ips_path;       // IPS patch applied at boot (randomizer hook)
  std::string replay_path;    // Replay played automatically at boot
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
    } else if (arg == "--rewind-smoke") {
      options.rewind_smoke = true;
    } else if (arg == "--ips" && i + 1 < argc) {
      options.ips_path = argv[++i];
    } else if (arg.rfind("--ips=", 0) == 0) {
      options.ips_path = arg.substr(6);
    } else if (arg == "--replay" && i + 1 < argc) {
      options.replay_path = argv[++i];
    } else if (arg.rfind("--replay=", 0) == 0) {
      options.replay_path = arg.substr(9);
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

// Sleeps until a deadline without burning a core. The old pacing loop spun on
// YieldProcessor for the last 2 ms of every frame, which kept one core pinned
// ~12% of the time for nothing. A high-resolution waitable timer (Win10 1803+)
// gets within ~0.1 ms, so only a very short spin is left for the tail.
class FramePacer {
public:
  FramePacer() {
#ifdef _WIN32
    timer_ = CreateWaitableTimerExW(nullptr, nullptr,
                                    CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                    TIMER_ALL_ACCESS);
    if (timer_ == nullptr) {
      // Pre-1803: fall back to a plain timer, still better than spinning.
      timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
#endif
  }

  ~FramePacer() {
#ifdef _WIN32
    if (timer_ != nullptr) CloseHandle(timer_);
#endif
  }

  FramePacer(const FramePacer&) = delete;
  FramePacer& operator=(const FramePacer&) = delete;

  void wait_until(std::chrono::steady_clock::time_point deadline) {
    using clock = std::chrono::steady_clock;
#ifdef _WIN32
    if (timer_ != nullptr) {
      // Leave a small margin for timer granularity, then spin out the tail.
      constexpr auto kSpinMargin = std::chrono::microseconds(400);
      const auto now = clock::now();
      if (deadline - now > kSpinMargin) {
        const auto sleep_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            (deadline - kSpinMargin) - now);
        LARGE_INTEGER due;
        // Negative = relative, in 100 ns units.
        due.QuadPart = -(sleep_ns.count() / 100);
        if (due.QuadPart < 0 &&
            SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE)) {
          WaitForSingleObject(timer_, INFINITE);
        }
      }
    }
#endif
    while (clock::now() < deadline) {
#if defined(_MSC_VER) || defined(__x86_64__) || defined(_M_X64)
      YieldProcessor();
#endif
    }
  }

private:
#ifdef _WIN32
  HANDLE timer_ = nullptr;
#endif
};

aw::RewindIo make_rewind_io(struct mCore* core) {
  aw::RewindIo io;
  io.user = core;
  io.capture = [](void* user) -> void* {
    return aw_mgba_capture_snapshot(static_cast<struct mCore*>(user));
  };
  io.restore = [](void* user, void* snapshot) -> bool {
    return aw_mgba_restore_snapshot(static_cast<struct mCore*>(user), snapshot) != 0;
  };
  io.release = [](void* user, void* snapshot) {
    static_cast<void>(user);
    aw_mgba_free_snapshot(snapshot);
  };
  io.size = [](void* user, void* snapshot) -> std::uint64_t {
    static_cast<void>(user);
    return aw_mgba_snapshot_size(snapshot);
  };
  return io;
}

std::string timestamp_string() {
  const std::time_t now = std::time(nullptr);
  std::tm tm_value{};
#ifdef _WIN32
  localtime_s(&tm_value, &now);
#else
  localtime_r(&now, &tm_value);
#endif
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &tm_value);
  return buffer;
}

void run_game_loop(std::filesystem::path rom_path, aw::RomImage rom, int max_frames,
                   const std::string& oam_log_path, bool rewind_smoke,
                   const std::string& boot_ips, const std::string& boot_replay) {
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

    // Randomizer hook #1: an IPS patch applied to the ROM image before the
    // core ever sees it. Any community randomizer patch drops in via
    // File > Apply IPS Patch (persisted) or --ips on the command line; the
    // patched image exists only as a temp file while playing.
    std::vector<std::uint32_t> mgba_buffer(240 * 160, 0);
    std::filesystem::path patched_rom_temp;
    const auto create_core_for = [&](const std::string& path) -> struct mCore* {
      aw::RomImage image = aw::load_rom_file(path);
      std::string effective_path = path;
      std::string ips = !boot_ips.empty() ? boot_ips : config.get_string("Patches", "ips_path", "");
      if (!ips.empty()) {
        std::ifstream patch_in(ips, std::ios::binary);
        if (!patch_in.is_open()) {
          std::cerr << "IPS patch not found: " << ips << " (booting unpatched)" << std::endl;
        } else {
          const std::vector<std::uint8_t> patch_bytes((std::istreambuf_iterator<char>(patch_in)),
                                                      std::istreambuf_iterator<char>());
          std::string err;
          if (!aw::apply_ips(image.bytes, patch_bytes, err)) {
            std::cerr << "IPS patch rejected: " << err << " (booting unpatched)" << std::endl;
          } else {
            const std::string patched_sha = aw::sha1_hex(image.bytes);
            patched_rom_temp = std::filesystem::temp_directory_path() /
                               ("aw_" + patched_sha.substr(0, 8) + ".gba");
            std::ofstream out(patched_rom_temp, std::ios::binary);
            out.write(reinterpret_cast<const char*>(image.bytes.data()),
                      static_cast<std::streamsize>(image.bytes.size()));
            std::cout << "IPS patch applied: " << ips << " (patched sha1 "
                      << patched_sha.substr(0, 12) << "...)" << std::endl;
            effective_path = patched_rom_temp.string();
          }
        }
      }
      return aw_mgba_create(effective_path.c_str(), mgba_buffer.data(), 240);
    };

    struct mCore* core = create_core_for(rom_path.string());
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

    // Time travel: an in-RAM savestate ring. Snapshots are zlib-compressed
    // memory blobs, so several captures per second never touch the disk.
    // History is capped at max_seconds (default 5): older snapshots are
    // evicted as the game runs, so rewinding cannot reach past that window.
    const double gba_fps = 16777216.0 / 280896.0; // ~59.7275 FPS
    const bool rewind_enabled = config.get_int("Rewind", "enabled", 1) != 0;
    const int rewind_interval = std::clamp(config.get_int("Rewind", "snapshot_interval",
                                                          aw::RewindBuffer::kDefaultInterval), 5, 120);
    const int rewind_capacity = std::clamp(config.get_int("Rewind", "capacity",
                                                          aw::RewindBuffer::kDefaultCapacity), 20, 1200);
    const int rewind_max_seconds = std::clamp(config.get_int("Rewind", "max_seconds", 5), 1, 300);
    const int rewind_window_frames = std::max(
        1, static_cast<int>(static_cast<double>(rewind_max_seconds) * gba_fps));
    aw::RewindBuffer rewind_buffer(rewind_capacity, rewind_interval, rewind_window_frames);
    if (rewind_enabled) {
      rewind_buffer.set_io(make_rewind_io(core));
    }

    // Undo Last Order: snapshots taken at the A press that confirms an
    // order while the map sensor sees live cursor control. Ten deep.
    aw::OrderStack order_stack;
    order_stack.set_io(make_rewind_io(core));

    // Map sensor: "the player is commanding the map" is decided by the
    // mined cursor coordinates answering the D-pad - the one verified
    // game-state read this project has.
    aw::MapSensor map_sensor;

    // RAM-write cheats: parsed from [Cheats], applied every frame while
    // enabled so the game's own writes cannot win.
    aw::CheatEngine cheats;
    cheats.load(config);
    if (cheats.enabled() && !cheats.codes().empty()) {
      std::cout << "Cheats: " << cheats.codes().size() << " code(s) armed" << std::endl;
    }

    // Replays: record the exact keys_pressed stream from power-on (F6), and
    // play it back (File > Play Replay). The core is deterministic, so the
    // stream alone reproduces the run; the ROM sha1 travels in the header so
    // mismatches are caught up front.
    const std::string rom_sha = aw::sha1_hex(rom.bytes);
    aw::ReplayRecorder replay_recorder;
    aw::ReplayPlayer replay_player;
    bool replay_playing = false;

    // Randomizer hook #2: one-shot RAM writes shortly after boot, driven by
    // config so a mined address becomes a tweak without a rebuild:
    //   [Randomize]
    //   apply_frame = 90
    //   write1 = 50345636:7
    // (decimal address:value). See data/symbols/README.md for mining.
    struct RamWrite {
      std::uint32_t addr;
      std::uint8_t value;
    };
    std::vector<RamWrite> randomize_writes;
    const int randomize_frame = std::clamp(config.get_int("Randomize", "apply_frame", 90), 1, 100000);
    for (int slot = 1; slot <= 64; ++slot) {
      const std::string spec = config.get_string("Randomize", "write" + std::to_string(slot), "");
      if (spec.empty()) continue;
      const std::size_t colon = spec.find(':');
      if (colon == std::string::npos) continue;
      randomize_writes.push_back({static_cast<std::uint32_t>(std::stoul(spec.substr(0, colon))),
                                  static_cast<std::uint8_t>(std::stoul(spec.substr(colon + 1)))});
    }
    bool randomize_applied = false;

    // Fast-forward runs core frames in a time-bounded burst between renders
    // so the CPU spends its budget emulating instead of blitting. The frame
    // minimum keeps GDI render + event-pump overhead below ~5% of the burst.
    constexpr auto kFastForwardBurstBudget = std::chrono::milliseconds(12);
    constexpr int kFastForwardBurstMinFrames = 24;
    constexpr int kFastForwardBurstMaxFrames = 128;
    // While Backspace is held, step back one snapshot every N loop frames
    // (~20 steps/s, i.e. scrubbing backward far faster than real time).
    constexpr int kRewindRepeatFrames = 3;

    aw::TacticalIntel intel;
    aw::HdAudioEngine hd_audio;

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

    // Presenting on the compositor's clock advances the emulator at the
    // display's rate, not the GBA's 59.7275 Hz, so the core produces samples
    // slightly faster than a 32768 Hz output drains them. Left alone the audio
    // queue grows without bound (measured: ~450 ms of latency in under a
    // minute) and then starts dropping samples. Scaling the output rate by the
    // same ratio keeps production and consumption matched.
    const double display_hz = window.display_refresh_hz();
    int vsync_paced_streak = 0;
    bool audio_follows_display = false;

    bool rewind_was_held = false;
    int rewind_repeat_counter = 0;
    bool ff_was_active = false;
    int rewind_smoke_restores = 0;
    std::uint16_t prev_keys = 0;
    std::uint64_t undo_points_taken = 0;
    bool undo_capture_logged = false;

    using clock = std::chrono::steady_clock;
    const std::chrono::nanoseconds frame_duration_ns(static_cast<std::int64_t>(1000000000.0 / gba_fps));
    auto next_frame_time = clock::now();

    // Telemetry for the sidebar: emulated frames vs wall time, refreshed
    // every ~500 ms.
    auto telemetry_start = clock::now();
    std::uint64_t telemetry_frames_base = 0;
    double telemetry_fps = 0.0;
    double telemetry_speed_pct = 100.0;

    // Return the core to frame 0 and drop every per-timeline subsystem.
    // Replay record and playback both start here; without it the input
    // stream alone could not reproduce the run.
    const auto power_cycle = [&]() {
      aw_mgba_reset(core);
      frames_run = 0;
      telemetry_frames_base = 0;
      telemetry_start = clock::now();
      prev_keys = 0;
      audio.drop_pending();
      rewind_buffer.reset();
      order_stack.reset();
      map_sensor.reset();
      randomize_applied = false;
    };

    // Boot-time replay (--replay): start playback immediately.
    if (!boot_replay.empty()) {
      std::string err;
      if (replay_player.load(boot_replay, err) && replay_player.rom_matches(rom_sha)) {
        power_cycle();
        replay_playing = true;
        std::cout << "Replay: playing " << boot_replay << " ("
                  << replay_player.info().frame_count << " frames)" << std::endl;
      } else {
        std::cerr << "Replay: cannot play " << boot_replay
                  << (err.empty() ? " (ROM sha1 mismatch)" : (" (" + err + ")")) << std::endl;
      }
    }

    auto copy_video_frame = [&mgba_buffer, &ppu]() {
      // Copy mGBA video buffer to PPU framebuffer with R/B channel swapping for Win32 GDI
      const std::uint32_t* src_buf = mgba_buffer.data();
      std::uint32_t* dst_buf = ppu.framebuffer.data();
      for (std::size_t i = 0; i < 240 * 160; ++i) {
        const std::uint32_t c = src_buf[i];
        dst_buf[i] = ((c & 0x000000FF) << 16) | (c & 0x0000FF00) | ((c & 0x00FF0000) >> 16);
      }
    };

    PerfCollector perf;
    perf.init();
    FramePacer pacer;

    while (window.is_open()) {
      perf.begin();
      if (window.has_pending_rom()) {
        const std::string new_rom_path = window.consume_pending_rom();
        std::cout << "Switching to new ROM: " << new_rom_path << std::endl;
        if (replay_recorder.active()) {
          replay_recorder.stop();
          window.set_recording_ui(false);
          std::cout << "Replay: recording finalized (ROM switch)" << std::endl;
        }
        replay_playing = false;
        aw_mgba_destroy(core);
        rom_path = new_rom_path;
        core = create_core_for(new_rom_path);
        if (!core) {
          throw std::runtime_error("aw_mgba_create failed for new ROM: " + rom_path.string());
        }
        // The backend caches raw block pointers that belong to the old core;
        // re-resolve them, then drop any tracking state that referred to the
        // previous ROM's OAM/context.
        probe.set_core(core);
        nav.reset();
        // Rewind history and undo points belong to the destroyed core's
        // timeline.
        rewind_buffer.set_io(make_rewind_io(core));
        rewind_buffer.reset();
        order_stack.set_io(make_rewind_io(core));
        order_stack.reset();
        map_sensor.reset();
      }

      hardware.keys_pressed = 0;
      if (!window.process_events(hardware)) {
        break;
      }
      perf.lap(PF_EVENTS);
      cheats.set_enabled(window.cheats_enabled());

      // Robust detection of battlefield map gameplay vs menus (Title, Menus, Name Entry, Options)
      const bool in_gameplay = is_battlefield_map(probe, nav.cursor_addresses());

      // Update Native RTS Steering & Touch/Pointer Navigation
      nav.set_in_map(in_gameplay);
      const std::uint16_t nav_keys = nav.update(window.input_frame());
      hardware.keys_pressed |= nav_keys;
      perf.lap(PF_PROBE);

      // --- Replay control (F6 record toggle, F7 stop, File > Play).
      if (window.consume_record_toggle()) {
        if (replay_recorder.active()) {
          replay_recorder.stop();
          window.set_recording_ui(false);
          std::cout << "Replay: saved " << replay_recorder.frames() << " frames to "
                    << replay_recorder.path() << std::endl;
        } else {
          replay_playing = false;
          power_cycle();
          const std::string name = "replay_" + timestamp_string() + ".awr";
          if (replay_recorder.start(name, rom_sha)) {
            window.set_recording_ui(true);
            std::cout << "Replay: recording from power-on -> " << name
                      << " (F6 again to finish)" << std::endl;
          }
        }
      }
      if (window.consume_playback_stop() && replay_playing) {
        replay_playing = false;
        std::cout << "Replay: playback stopped at frame " << replay_player.frame_index() << "/"
                  << replay_player.info().frame_count << std::endl;
      }
      if (window.has_pending_replay()) {
        const std::string replay_path_req = window.consume_pending_replay();
        std::string err;
        if (!replay_player.load(replay_path_req, err)) {
          std::cerr << "Replay: " << err << std::endl;
        } else if (!replay_player.rom_matches(rom_sha)) {
          std::cerr << "Replay: recorded for a different ROM revision - refusing to play"
                    << std::endl;
        } else {
          if (replay_recorder.active()) {
            replay_recorder.stop();
            window.set_recording_ui(false);
          }
          power_cycle();
          replay_playing = true;
          std::cout << "Replay: playing " << replay_path_req << " ("
                    << replay_player.info().frame_count << " frames, Tab fast-forwards)"
                    << std::endl;
        }
      }

      // Process pending Save State request
      const std::string save_request = window.consume_pending_save_state();
      if (!save_request.empty()) {
        if (aw_mgba_save_state(core, save_request.c_str())) {
          std::cout << "Successfully saved state to: " << save_request << std::endl;
        } else {
          std::cerr << "Failed to save state to: " << save_request << std::endl;
        }
      }

      // Process pending Load State request
      const std::string load_request = window.consume_pending_load_state();
      if (!load_request.empty()) {
        if (replay_playing) {
          std::cout << "Replay: load state ignored during playback (it would desync)"
                    << std::endl;
        } else if (aw_mgba_load_state(core, load_request.c_str())) {
          std::cout << "Successfully loaded state from: " << load_request << std::endl;
          // Audio queued before the load belongs to the abandoned timeline.
          audio.drop_pending();
          rewind_buffer.reset();
          order_stack.reset();
          map_sensor.reset();
        } else {
          std::cerr << "Failed to load state from: " << load_request << std::endl;
        }
      }

      const bool ff_active = window.fast_forward_active();
      const bool rewind_held = window.rewind_held();
      const bool rewind_menu_step = window.consume_rewind_step();

      // --- Time travel: hold Backspace (or Y/LT) to step back through the
      // snapshot ring. The game never advances while rewinding.
      bool did_rewind = false;
      if (rewind_held || rewind_menu_step) {
        if (!rewind_was_held) {
          std::cout << "Time travel: rewinding (history depth " << rewind_buffer.size() << ")"
                    << std::endl;
        }
        const bool should_step = rewind_menu_step ||
                                 !rewind_was_held ||
                                 ++rewind_repeat_counter >= kRewindRepeatFrames;
        if (should_step) {
          rewind_repeat_counter = 0;
          did_rewind = rewind_buffer.rewind_step();
          if (did_rewind) {
            // Flush the future timeline's audio, then re-render one frame so
            // the screen shows the rewound-to moment.
            audio.drop_pending();
            aw_mgba_run_frame(core, 0);
            ++frames_run;
            aw_mgba_read_audio(core, audio_samples.data(), audio_samples.size() / 2);
          } else if (!rewind_buffer.disabled() && rewind_menu_step) {
            std::cout << "Rewind: no history yet - keep playing to build some." << std::endl;
          }
        }
      } else {
        if (rewind_was_held) {
          std::cout << "Time travel: resumed play" << std::endl;
        }
        rewind_repeat_counter = 0;
      }
      rewind_was_held = rewind_held;

      if (!did_rewind && !rewind_held) {
        // --- Undo Last Order: restore the snapshot taken at the A press
        // that confirmed the order (Ctrl+Z, pad L3, or the File menu).
        // Meaningless during replay playback: the input stream, not the
        // player, owns the timeline.
        if (!replay_playing && window.consume_undo_press()) {
          if (order_stack.pop()) {
            ++undo_points_taken;
            audio.drop_pending();
            rewind_buffer.reset();
            map_sensor.reset();
            aw_mgba_run_frame(core, 0);  // Refresh the framebuffer from the restored state.
            ++frames_run;
            aw_mgba_read_audio(core, audio_samples.data(), audio_samples.size() / 2);
            std::cout << "Undo: restored order point (" << order_stack.size()
                      << " remaining)" << std::endl;
          } else if (!order_stack.disabled()) {
            std::cout << "Undo: no order points captured yet" << std::endl;
          }
        }

        // Replay playback owns this frame's keys; the recorder logs whatever
        // keys are actually fed to the core.
        if (replay_playing) {
          std::uint16_t replay_keys = 0;
          if (replay_player.next(replay_keys)) {
            hardware.keys_pressed = replay_keys;
          } else {
            replay_playing = false;
            std::cout << "Replay: playback finished ("
                      << replay_player.info().frame_count << " frames)" << std::endl;
          }
        }

        // --- Normal / fast-forward emulation.
        if (ff_active) {
          if (!ff_was_active) {
            // Entering fast-forward: stop 1x playback; samples generated while
            // fast-forwarding are drained and discarded so nothing ever plays
            // pitched up or falls behind.
            audio.drop_pending();
          }
          const auto burst_start = clock::now();
          int burst_frames = 0;
          std::uint16_t burst_keys = hardware.keys_pressed;
          do {
            if (replay_playing) {
              // Fast-forwarding a replay still consumes one input record per
              // emulated frame.
              if (!replay_player.next(burst_keys)) {
                replay_playing = false;
                std::cout << "Replay: playback finished ("
                          << replay_player.info().frame_count << " frames)" << std::endl;
                burst_keys = 0;
              }
            }
            if (replay_recorder.active()) replay_recorder.record(burst_keys);
            aw_mgba_run_frame(core, burst_keys);
            ++frames_run;
            ++burst_frames;
            aw_mgba_read_audio(core, audio_samples.data(), audio_samples.size() / 2);
            rewind_buffer.on_frame();
          } while (burst_frames < kFastForwardBurstMaxFrames &&
                   (burst_frames < kFastForwardBurstMinFrames ||
                    clock::now() - burst_start < kFastForwardBurstBudget));
          prev_keys = burst_keys;
        } else {
          if (ff_was_active) {
            std::cout << "Fast-forward: back to 1x" << std::endl;
          }

          // Update native C++ Tactical Intel & HD Audio Engine
          intel.update(probe, nav.context());
          hd_audio.update(probe);
          perf.lap(PF_PROBE);

          // Capture an undo point at the moment an order is confirmed: an
          // A press while in live battlefield map gameplay. The
          // snapshot precedes the frame, so undo lands exactly before the
          // unit moved / the menu opened.
          const bool a_edge = (hardware.keys_pressed & aw::kKeyA) != 0 &&
                              (prev_keys & aw::kKeyA) == 0;
          if (a_edge && in_gameplay && !replay_playing) {
            if (order_stack.push() && !undo_capture_logged) {
              undo_capture_logged = true;
              std::cout << "Undo: order point captured (Ctrl+Z to restore)" << std::endl;
            }
          }

          if (replay_recorder.active()) replay_recorder.record(hardware.keys_pressed);
          aw_mgba_run_frame(core, hardware.keys_pressed);
          perf.lap(PF_EMU);

          if (cheats.active_count() > 0) {
            for (const aw::CheatCode& c : cheats.codes()) {
              if (c.width == 1) {
                aw_mgba_write8(core, c.address, static_cast<std::uint8_t>(c.value));
              } else if (c.width == 2) {
                aw_mgba_write16(core, c.address, static_cast<std::uint16_t>(c.value));
              } else {
                aw_mgba_write16(core, c.address, static_cast<std::uint16_t>(c.value));
                aw_mgba_write16(core, c.address + 2,
                                static_cast<std::uint16_t>(c.value >> 16));
              }
            }
          }
          frames_run++;
          rewind_buffer.on_frame();
          perf.lap(PF_REWIND);
          map_sensor.on_frame(hardware.keys_pressed,
                              read_cursor_tile(probe, nav.cursor_addresses()));
          prev_keys = hardware.keys_pressed;

          // Randomizer hook #2: one-shot RAM writes shortly after boot.
          if (!randomize_writes.empty() && !randomize_applied &&
              frames_run >= static_cast<std::uint64_t>(randomize_frame)) {
            for (const auto& write : randomize_writes) {
              aw_mgba_write8(core, write.addr, write.value);
            }
            randomize_applied = true;
            std::cout << "Randomize: applied " << randomize_writes.size()
                      << " RAM write(s) at frame " << frames_run << std::endl;
          }

          // Check for audio sample rate changes from core (e.g. SOUNDBIAS 32768 Hz <-> 65536 Hz),
          // scaled to the rate frames are actually being presented at.
          const unsigned current_rate = aw_mgba_audio_sample_rate(core);
          if (current_rate != 0) {
            int target_rate = static_cast<int>(current_rate);
            if (audio_follows_display) {
              target_rate = static_cast<int>(
                  static_cast<double>(current_rate) * display_hz / gba_fps + 0.5);
            }
            if (target_rate != audio.sample_rate()) {
              audio.set_sample_rate(target_rate);
            }
          }

          // Read audio samples from mGBA core and process through HD Audio Engine
          const int samples_read =
              aw_mgba_read_audio(core, audio_samples.data(), audio_samples.size() / 2);
          if (samples_read > 0) {
            hd_audio.mix_audio(audio_samples.data(), samples_read);
            audio.push_samples(audio_samples.data(), samples_read);
            total_audio_samples += samples_read;
          }
          perf.lap(PF_AUDIO);
        }
      }
      ff_was_active = ff_active;

      window.set_playback_indicator(rewind_held ? -1 : (ff_active ? 1 : 0));

      // Telemetry: emulated frames per wall-clock second, as a speed
      // percentage against the GBA's real frame rate.
      {
        const auto now = clock::now();
        const double elapsed_s = std::chrono::duration<double>(now - telemetry_start).count();
        if (elapsed_s >= 0.5) {
          const double frames = static_cast<double>(frames_run - telemetry_frames_base);
          telemetry_fps = frames / elapsed_s;
          telemetry_speed_pct = telemetry_fps / gba_fps * 100.0;
          telemetry_start = now;
          telemetry_frames_base = frames_run;
        }
      }

      copy_video_frame();
      perf.lap(PF_COPY);

      // Draw Tactical RTS Tile Reticle & Pointer Overlay ONLY during gameplay on the map
      if (in_gameplay) {
        if (const auto* p = window.input_frame().primary_pointer()) {
          if (p->in_viewport) {
            const int scroll_x = probe.read_io16(aw::bg_hofs_reg(2));
            const int scroll_y = probe.read_io16(aw::bg_vofs_reg(2));
            const int tile_x = (p->gba_x + scroll_x) / 16;
            const int tile_y = (p->gba_y + scroll_y) / 16;
            aw::draw_rts_tile_reticle(ppu.framebuffer.data(), ppu.width, ppu.height,
                                      tile_x, tile_y, scroll_x, scroll_y, 0x00F5A623u);
            aw::draw_pointer(ppu.framebuffer.data(), ppu.width, ppu.height, p->gba_x, p->gba_y);
          }
        }
      }

      // Draw pixel-perfect C++ Tactical Intel HUD overlay if enabled
      if (window.show_hud()) {
        aw::draw_hud_overlay(ppu.framebuffer.data(), ppu.width, ppu.height, intel, false);
      }

      // Speedrun status strip: frame counter + held buttons (+ REC/PLAY).
      if (window.input_display()) {
        aw::draw_status_overlay(ppu.framebuffer.data(), ppu.width, ppu.height,
                                hardware.keys_pressed, frames_run,
                                replay_recorder.active(), replay_playing);
      }

      perf.lap(PF_OVERLAY);

      // Render frame to window, with the tactical sidebar beside the game.
      aw::SidebarData sidebar;
      sidebar.fast_forward = ff_active;
      sidebar.rewinding = rewind_held;
      sidebar.in_map = in_gameplay;
      sidebar.cursor_valid = intel.cursor_valid();
      sidebar.cursor_x = intel.cursor_x();
      sidebar.cursor_y = intel.cursor_y();
      sidebar.undo_depth = order_stack.size();
      sidebar.undo_capacity = order_stack.capacity();
      sidebar.rewind_snapshots = rewind_buffer.size();
      sidebar.rewind_capacity = rewind_buffer.capacity();
      sidebar.rewind_window_seconds = rewind_window_frames / gba_fps;
      sidebar.fps = telemetry_fps;
      sidebar.emu_speed_pct = telemetry_speed_pct;
      sidebar.frames_run = frames_run;
      sidebar.replay_recording = replay_recorder.active();
      sidebar.replay_playing = replay_playing;
      sidebar.replay_frame = replay_playing ? replay_player.frame_index()
                                            : replay_recorder.frames();
      sidebar.replay_total = replay_player.info().frame_count;
      sidebar.live_keys = hardware.keys_pressed;
      sidebar.cheat_count = cheats.active_count();
      sidebar.forecast = intel.forecast();
      window.render(ppu, sidebar);
#ifdef _WIN32
      // GDI batches drawing calls, so timing render() alone would measure only
      // queue time. Flush so the lap reflects real execution cost.
      if (perf.enabled) GdiFlush();
#endif
      perf.lap(PF_RENDER);
      perf.end_work();

      // Align with the compositor before pacing, so the wait is attributed to
      // presentation rather than to frame work.
      const bool vsync_paced = window.present_wait();
      perf.lap(PF_VSYNC);

      // Decide "the compositor is pacing us" from a run of frames, not a
      // single one, so an isolated late frame cannot flip the audio rate.
      vsync_paced_streak = vsync_paced ? std::min(vsync_paced_streak + 1, 240)
                                       : std::max(vsync_paced_streak - 1, 0);
      if (display_hz > 0.0) {
        if (!audio_follows_display && vsync_paced_streak >= 120) {
          audio_follows_display = true;
        } else if (audio_follows_display && vsync_paced_streak == 0) {
          audio_follows_display = false;
        }
      }

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

      // Headless self-test: restore two snapshots mid-run and report.
      if (rewind_smoke && frames_run >= 120 && rewind_smoke_restores == 0) {
        did_rewind = rewind_buffer.rewind_step();
        rewind_smoke_restores += did_rewind ? 1 : 0;
        did_rewind = rewind_buffer.rewind_step();
        rewind_smoke_restores += did_rewind ? 1 : 0;
        std::cout << "rewind smoke: restored " << rewind_smoke_restores << "/2 snapshots, ring size "
                  << rewind_buffer.size() << ", held "
                  << (rewind_buffer.total_bytes_held() / 1024) << " KB" << std::endl;
      }

      // Steady frame pacing at the GBA's native rate. Skipped when the
      // compositor already paced the frame (see present_wait); the clock is
      // re-anchored in that case so 1x resumes without a stall.
      if (ff_active || vsync_paced) {
        // Fast-forward runs unpaced; with vsync the compositor already spent
        // the frame interval, so pacing again here would double-wait and drag
        // the rate below the target. Re-anchor either way.
        next_frame_time = clock::now() + frame_duration_ns;
      } else {
        const auto now = clock::now();
        if (now < next_frame_time) {
          pacer.wait_until(next_frame_time);
          next_frame_time += frame_duration_ns;
        } else {
          // Frame took slightly longer than target: re-anchor immediately so
          // subsequent frames are not squeezed into shorter intervals.
          next_frame_time = now + frame_duration_ns;
        }
      }

      perf.lap(PF_PACE);
      if (perf.enabled) perf.audio_queued = audio.queued_frames();
      perf.report_if_due();

      if (max_frames > 0 && frames_run >= static_cast<std::uint64_t>(max_frames)) {
        break;
      }
      if (rewind_smoke && rewind_smoke_restores > 0 && frames_run >= 150) {
        break;
      }
    }

    if (replay_recorder.active()) {
      replay_recorder.stop();
      std::cout << "Replay: saved " << replay_recorder.frames() << " frames to "
                << replay_recorder.path() << std::endl;
    }

    std::cout << "Ran " << frames_run << " frames; audio samples emitted: " << total_audio_samples << std::endl;
    aw_mgba_destroy(core);
  } catch (const std::exception& e) {
    std::cerr << "Error in run_game_loop: " << e.what() << std::endl;
  }
}

}  // namespace

// Double-clicking the executable starts it in whatever directory the exe
// lives in, not the project root, so every relative path the runtime uses
// (config.ini, the ROM, data/symbols, savestates, replays) would miss. Anchor
// the working directory on the real data root before anything reads it.
//
// A directory only counts as the data root if it holds config.ini *and* a
// data/ folder. Requiring both matters: the runtime writes config.ini into
// whatever directory it was started in, so stale copies accumulate inside the
// build tree, and matching on config.ini alone would happily latch onto one of
// those and silently run with the wrong settings.
bool is_data_root(const std::filesystem::path& dir) {
  std::error_code ec;
  return std::filesystem::exists(dir / "config.ini", ec) &&
         std::filesystem::is_directory(dir / "data", ec);
}

void anchor_working_directory() {
#ifdef _WIN32
  if (is_data_root(std::filesystem::current_path())) return;

  wchar_t buffer[MAX_PATH * 4];
  const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
  if (length == 0 || length >= std::size(buffer)) return;

  std::filesystem::path dir = std::filesystem::path(buffer).parent_path();
  for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
    if (is_data_root(dir)) {
      std::error_code ec;
      std::filesystem::current_path(dir, ec);
      if (!ec) {
        std::cout << "Working directory: " << dir.string() << std::endl;
      }
      return;
    }
    const std::filesystem::path parent = dir.parent_path();
    if (parent == dir) break;
    dir = parent;
  }

  std::cerr << "Warning: could not locate config.ini and data/ from the executable "
               "location; relative paths may not resolve." << std::endl;
#endif
}

int main(int argc, char** argv) {
#ifdef _WIN32
  // High-precision multimedia timer (1 ms accuracy for smooth frame pacing)
  timeBeginPeriod(1);
#endif
  // The runtime prints little; unbuffered stdout keeps diagnostics alive
  // past hard crashes (savestate bring-up debugging depends on it).
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  anchor_working_directory();

  std::cout << "AW-Recompiled backend: RECOMP (native static recompilation)"
            << std::endl;

  int exit_code = 0;
  bool pause_on_exit = false;

  try {
    Options options = parse_options(argc, argv);
    pause_on_exit = options.pause_on_exit;

    aw::RomImage rom = aw::load_rom_file(options.rom_path);
    const auto header = aw::parse_header(rom.bytes);
    std::cout << "ROM: " << header.title << ' ' << header.game_code << header.maker_code
              << " Rev " << static_cast<int>(header.version) << '\n';

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
    } else if (options.play_enabled) {
      run_game_loop(options.rom_path, rom, options.max_frames, options.oam_log_path,
                    options.rewind_smoke, options.ips_path, options.replay_path);
    }
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
#ifdef _WIN32
    // Double-clicked launches may not keep a console around long enough
    // to read stderr; mirror the fatal error into a dialog.
    MessageBoxA(nullptr, e.what(), "AW-Recompiled - Fatal error",
                MB_OK | MB_ICONERROR);
#endif
    exit_code = 1;
  }

#ifdef _WIN32
  timeEndPeriod(1);
#endif

  wait_before_close(pause_on_exit);
  return exit_code;
}