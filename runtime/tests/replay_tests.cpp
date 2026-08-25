#include "aw/replay.hpp"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

void require(bool condition, const std::string& label) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label.c_str());
  }
}

const char* kTestPath = "test_replay.awr";
const char* kSha = "15053499D5B3F49128A941D7F2D84876F5424D0C";

void test_round_trip() {
  {
    aw::ReplayRecorder rec;
    require(rec.start(kTestPath, kSha), "start recording");
    rec.record(0x0001);
    rec.record(0x0311);
    rec.record(0x0000);
    require(rec.stop(), "stop recording");
    require(rec.frames() == 3, "counted frames");
  }

  aw::ReplayPlayer player;
  std::string err;
  require(player.load(kTestPath, err), "load replay: " + err);
  require(player.info().frame_count == 3, "frame count survived");
  require(player.rom_matches(kSha), "sha matches");
  require(!player.rom_matches("deadbeef"), "sha mismatch detected");

  std::uint16_t keys = 0;
  require(player.next(keys) && keys == 0x0001, "frame 0 keys");
  require(player.next(keys) && keys == 0x0311, "frame 1 keys");
  require(player.next(keys) && keys == 0x0000, "frame 2 keys");
  require(!player.next(keys), "exhausted at frame 3");
  require(player.finished(), "finished flag");

  player.rewind_to_start();
  require(player.next(keys) && keys == 0x0001, "rewind_to_start replays");
}

void test_rejects_garbage() {
  FILE* f = std::fopen("test_bad.awr", "wb");
  std::fputs("not a replay at all, just bytes", f);
  std::fclose(f);

  aw::ReplayPlayer player;
  std::string err;
  require(!player.load("test_bad.awr", err), "garbage rejected");
  require(!err.empty(), "error explains itself");

  require(!player.load("test_missing.awr", err), "missing file rejected");
}

void test_recorder_noop_without_start() {
  aw::ReplayRecorder rec;
  rec.record(0xFFFF);  // Must be a no-op, not a crash.
  require(rec.stop(), "stop without start is fine");
  require(!rec.active(), "never active");
}

}  // namespace

int main() {
  test_round_trip();
  test_rejects_garbage();
  test_recorder_noop_without_start();
  std::remove(kTestPath);
  std::remove("test_bad.awr");

  if (failures == 0) {
    std::printf("replay_tests: all tests passed\n");
    return 0;
  }
  std::fprintf(stderr, "replay_tests: %d failure(s)\n", failures);
  return 1;
}
