#pragma once

#include "aw/memory.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace aw {

class Audio {
public:
  Audio();
  ~Audio();

  void update(Memory& memory, std::uint32_t cycles);

  // Called once per frame to flush accumulated samples to the audio device
  void flush_frame(Memory& memory);

  // Push interleaved stereo samples (from the emulator core) to the audio device.
  // `count` is the number of stereo sample frames (each 2 int16_t values).
  void push_samples(const std::int16_t* samples, int count);

  bool is_active() const { return is_active_; }
  int sample_rate() const { return sample_rate_; }

private:
  void pump_waveout();

  void* hwaveout_ = nullptr;
  int sample_rate_ = 32768;
  bool is_active_ = false;

  // Direct Sound FIFOs
  std::array<std::int8_t, 32> fifo_a_{};
  std::array<std::int8_t, 32> fifo_b_{};
  int fifo_a_pos_ = 0;
  int fifo_a_count_ = 0;
  int fifo_b_pos_ = 0;
  int fifo_b_count_ = 0;
  std::int8_t fifo_a_sample_ = 0;
  std::int8_t fifo_b_sample_ = 0;

  // Sample accumulation buffer
  static constexpr int kMaxSamplesPerFrame = 1024;
  std::array<std::int16_t, kMaxSamplesPerFrame * 2> sample_buffer_{};  // stereo interleaved
  int sample_count_ = 0;

  // Cycle accumulator for sample generation
  std::uint32_t cycle_accumulator_ = 0;
  static constexpr std::uint32_t kCyclesPerSample = 512;  // ~16.78MHz / 32768Hz

  // WaveOut multi-buffer ring system for smooth crackle-free playback
  static constexpr int kBlockSamples = 512;  // ~15.6ms per block
  static constexpr int kNumBuffers = 16;     // 16 blocks total capacity

  void* wave_headers_[kNumBuffers]{};
  std::array<std::int16_t, kBlockSamples * 2> wave_buffers_[kNumBuffers]{};
  int current_buffer_ = 0;

  // Ring buffer decoupling core sample generation from waveOut output
  static constexpr std::size_t kRingSize = 32768; // 16384 stereo frames
  std::vector<std::int16_t> ring_buffer_;
  std::size_t ring_head_ = 0;
  std::size_t ring_tail_ = 0;
};

}  // namespace aw