#include "aw/mgba_adapter.h"
#include "aw/test_config.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void require(bool condition, const std::string& label) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label.c_str());
  }
}

}  // namespace

int main() {
  std::vector<std::uint32_t> video_buffer(240 * 160, 0);
  struct mCore* core = aw_mgba_create(AW_TEST_ROM_PATH, video_buffer.data(), 240);
  if (!core) {
    std::cerr << "Cannot create core for " << AW_TEST_ROM_PATH << "\n";
    return 1;
  }

  // Run 60 frames
  for (int i = 0; i < 60; ++i) {
    aw_mgba_run_frame(core, 0);
  }

  // Write a marker byte into EWRAM and save state to disk
  aw_mgba_write8(core, 0x02001000, 0x42);
  aw_mgba_write16(core, 0x02001002, 0x1234);

  const std::filesystem::path state_path = std::filesystem::temp_directory_path() / "test_savestate.ss";
  if (std::filesystem::exists(state_path)) {
    std::filesystem::remove(state_path);
  }

  int save_res = aw_mgba_save_state(core, state_path.string().c_str());
  require(save_res == 1, "aw_mgba_save_state succeeded");
  require(std::filesystem::exists(state_path), "savestate file exists on disk");
  require(std::filesystem::file_size(state_path) > 1000, "savestate file has non-trivial size");

  // Mutate the state in the live core
  aw_mgba_write8(core, 0x02001000, 0x99);
  aw_mgba_write16(core, 0x02001002, 0x5678);
  require(aw_mgba_read8(core, 0x02001000) == 0x99, "memory mutated to 0x99");

  // Run another 30 frames
  for (int i = 0; i < 30; ++i) {
    aw_mgba_run_frame(core, 0);
  }

  // Load state back from disk
  int load_res = aw_mgba_load_state(core, state_path.string().c_str());
  require(load_res == 1, "aw_mgba_load_state succeeded");

  // Verify memory reverted to the saved state
  require(aw_mgba_read8(core, 0x02001000) == 0x42, "memory restored to 0x42");
  require(aw_mgba_read16(core, 0x02001002) == 0x1234, "memory restored to 0x1234");

  // Clean up
  std::filesystem::remove(state_path);
  aw_mgba_destroy(core);

  if (failures > 0) {
    std::fprintf(stderr, "%d savestate tests failed\n", failures);
    return 1;
  }
  std::cout << "All savestate file tests passed.\n";
  return 0;
}
