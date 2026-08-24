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

  // Reopen the audio device at a new sample rate. The mGBA GBA core can change
  // its rate at runtime (games raising SOUNDBIAS resolution switch from
  // 32768 Hz to 65536 Hz), and playback must follow or audio plays at the
  // wrong speed and overflows the buffers.
  void set_sample_rate(int sample_rate);

  // Silently discard everything queued for playback. Used when the timeline
  // jumps (time travel rewind, savestate load) so audio from the abandoned
  // future never plays, and when fast-forward ends so playback resumes clean.
  void drop_pending();

  bool is_active() const { return is_active_; }
  int sample_rate() const { return sample_rate_; }

  // Total number of stereo sample frames currently buffered (ring buffer
  // plus blocks queued in waveOut). Used for audio-driven frame pacing.
  int queued_frames() const;

private:
  void pump_waveout();
  void reset_streams();
  std::size_t ring_queued_frames() const;

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
  static constexpr int kBlockSamples = 512;  // ~15.6ms per block at 32768 Hz
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