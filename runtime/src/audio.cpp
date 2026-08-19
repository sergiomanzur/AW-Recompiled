#include "aw/audio.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmeapi.h>
#include <timeapi.h>

#include <cstring>

namespace aw {

Audio::Audio() {
  // Set Windows system timer resolution to 1ms to eliminate frame sleep quantization
  timeBeginPeriod(1);

  ring_buffer_.resize(kRingSize, 0);

  WAVEFORMATEX wfx = {};
  wfx.wFormatTag = WAVE_FORMAT_PCM;
  wfx.nChannels = 2; // Stereo
  wfx.nSamplesPerSec = sample_rate_;
  wfx.wBitsPerSample = 16;
  wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
  wfx.cbSize = 0;

  HWAVEOUT hwave = nullptr;
  if (waveOutOpen(&hwave, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
    hwaveout_ = static_cast<void*>(hwave);
    is_active_ = true;

    // Allocate wave headers for smooth multi-buffer ring system
    for (int i = 0; i < kNumBuffers; ++i) {
      auto* hdr = new WAVEHDR{};
      hdr->lpData = reinterpret_cast<LPSTR>(wave_buffers_[i].data());
      hdr->dwBufferLength = 0;
      hdr->dwFlags = 0;
      wave_headers_[i] = hdr;
    }
  }
}

Audio::~Audio() {
  if (hwaveout_ != nullptr) {
    waveOutReset(static_cast<HWAVEOUT>(hwaveout_));
    for (int i = 0; i < kNumBuffers; ++i) {
      if (wave_headers_[i] != nullptr) {
        auto* hdr = static_cast<WAVEHDR*>(wave_headers_[i]);
        if (hdr->dwFlags & WHDR_PREPARED) {
          waveOutUnprepareHeader(static_cast<HWAVEOUT>(hwaveout_), hdr, sizeof(WAVEHDR));
        }
        delete hdr;
      }
    }
    waveOutClose(static_cast<HWAVEOUT>(hwaveout_));
  }

  timeEndPeriod(1);
}

void Audio::update(Memory& memory, std::uint32_t cycles) {
  if (!is_active_) return;

  const std::uint8_t soundcnt_x = read8(memory, 0x04000084);
  if (!(soundcnt_x & 0x80)) return;

  const std::uint16_t soundcnt_h = read16(memory, 0x04000082);
  const bool ds_a_enable_right = (soundcnt_h & (1 << 8)) != 0;
  const bool ds_a_enable_left  = (soundcnt_h & (1 << 9)) != 0;
  const bool ds_b_enable_right = (soundcnt_h & (1 << 12)) != 0;
  const bool ds_b_enable_left  = (soundcnt_h & (1 << 13)) != 0;
  const bool ds_a_full_vol     = (soundcnt_h & (1 << 2)) != 0;
  const bool ds_b_full_vol     = (soundcnt_h & (1 << 3)) != 0;

  cycle_accumulator_ += cycles;
  while (cycle_accumulator_ >= kCyclesPerSample && sample_count_ < kMaxSamplesPerFrame) {
    cycle_accumulator_ -= kCyclesPerSample;

    std::int32_t left = 0;
    std::int32_t right = 0;

    const std::int32_t a_sample = static_cast<std::int32_t>(fifo_a_sample_) * (ds_a_full_vol ? 4 : 2);
    if (ds_a_enable_left)  left  += a_sample;
    if (ds_a_enable_right) right += a_sample;

    const std::int32_t b_sample = static_cast<std::int32_t>(fifo_b_sample_) * (ds_b_full_vol ? 4 : 2);
    if (ds_b_enable_left)  left  += b_sample;
    if (ds_b_enable_right) right += b_sample;

    if (left > 32767) left = 32767;
    if (left < -32768) left = -32768;
    if (right > 32767) right = 32767;
    if (right < -32768) right = -32768;

    sample_buffer_[sample_count_ * 2]     = static_cast<std::int16_t>(left);
    sample_buffer_[sample_count_ * 2 + 1] = static_cast<std::int16_t>(right);
    ++sample_count_;
  }
}

void Audio::push_samples(const std::int16_t* samples, int count) {
  if (!is_active_ || samples == nullptr || count <= 0) return;

  // Append incoming stereo sample frames to the ring buffer
  for (int i = 0; i < count; ++i) {
    ring_buffer_[ring_head_]     = samples[i * 2];
    ring_buffer_[ring_head_ + 1] = samples[i * 2 + 1];
    ring_head_ = (ring_head_ + 2) % kRingSize;
  }

  pump_waveout();
}

void Audio::pump_waveout() {
  if (!is_active_ || hwaveout_ == nullptr) return;

  auto hwave = static_cast<HWAVEOUT>(hwaveout_);

  // Determine available sample frames in ring buffer
  const std::size_t queued_samples = (ring_head_ >= ring_tail_)
      ? (ring_head_ - ring_tail_) / 2
      : (kRingSize - ring_tail_ + ring_head_) / 2;

  std::size_t avail_frames = queued_samples;

  // Feed available blocks into waveOut driver
  while (avail_frames >= static_cast<std::size_t>(kBlockSamples)) {
    // Find a free waveOut buffer (not currently queued for playback)
    int target = -1;
    for (int i = 0; i < kNumBuffers; ++i) {
      auto* test_hdr = static_cast<WAVEHDR*>(wave_headers_[(current_buffer_ + i) % kNumBuffers]);
      if (!(test_hdr->dwFlags & WHDR_INQUEUE)) {
        target = (current_buffer_ + i) % kNumBuffers;
        break;
      }
    }

    if (target < 0) {
      // All waveOut headers are currently playing; wait for next tick
      break;
    }

    auto* hdr = static_cast<WAVEHDR*>(wave_headers_[target]);
    if (hdr->dwFlags & WHDR_PREPARED) {
      waveOutUnprepareHeader(hwave, hdr, sizeof(WAVEHDR));
    }

    // Copy kBlockSamples from ring_buffer_ into wave_buffers_[target]
    for (int s = 0; s < kBlockSamples; ++s) {
      wave_buffers_[target][s * 2]     = ring_buffer_[ring_tail_];
      wave_buffers_[target][s * 2 + 1] = ring_buffer_[ring_tail_ + 1];
      ring_tail_ = (ring_tail_ + 2) % kRingSize;
    }
    avail_frames -= kBlockSamples;

    const DWORD byte_count = static_cast<DWORD>(kBlockSamples) * 2 * sizeof(std::int16_t);
    hdr->lpData = reinterpret_cast<LPSTR>(wave_buffers_[target].data());
    hdr->dwBufferLength = byte_count;
    hdr->dwFlags = 0;

    if (waveOutPrepareHeader(hwave, hdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
      waveOutWrite(hwave, hdr, sizeof(WAVEHDR));
    }

    current_buffer_ = (target + 1) % kNumBuffers;
  }
}

void Audio::flush_frame(Memory& /*memory*/) {
  if (!is_active_ || sample_count_ == 0) return;
  push_samples(sample_buffer_.data(), sample_count_);
  sample_count_ = 0;
}

}  // namespace aw

#else

namespace aw {

Audio::Audio() {}
Audio::~Audio() {}
void Audio::update(Memory& /*memory*/, std::uint32_t /*cycles*/) {}
void Audio::flush_frame(Memory& /*memory*/) {}
void Audio::push_samples(const std::int16_t* /*samples*/, int /*count*/) {}
void Audio::pump_waveout() {}

}  // namespace aw

#endif