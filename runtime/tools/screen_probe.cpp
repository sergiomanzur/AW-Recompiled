#include "aw/hardware.hpp"
#include "aw/mgba_adapter.h"
#include "aw/probe/backend_mgba.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

struct HostDebug {
  void* machine;
  // Let's see if we can read stats or time it
};

int main() {
  const char* rom_path = "rom/Advance Wars (USA) (Rev 1).gba";
  std::vector<std::uint32_t> video(240 * 160, 0);
  struct mCore* core = aw_mgba_create(rom_path, video.data(), 240);
  if (!core) return 1;

  aw_mgba_load_state(core, "build/recomp/runtime/state_0.ss");

  using clock = std::chrono::high_resolution_clock;

  for (int f = 0; f < 30; ++f) {
    auto t0 = clock::now();
    aw_mgba_run_frame(core, 0);
    auto t1 = clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("Frame %2d: %.2f ms\n", f, ms);
  }

  aw_mgba_destroy(core);
  return 0;
}
