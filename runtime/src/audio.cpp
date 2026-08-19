#include "aw/audio.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmeapi.h>

#include <cstring>

namespace aw {

Audio::Audio() {
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

    // Allocate wave headers for ring buffer
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
}

void Audio::update(Memory& memory, std::uint32_t cycles) {
  if (!is_active_) return;

  // Check if master sound is enabled
  const std::uint8_t soundcnt_x = read8(memory, 0x04000084);
  if (!(soundcnt_x & 0x80)) return;

  // Read SOUNDCNT_H for Direct Sound configuration
  const std::uint16_t soundcnt_h = read16(memory, 0x04000082);
  const bool ds_a_enable_right = (soundcnt_h & (1 << 8)) != 0;
  const bool ds_a_enable_left  = (soundcnt_h & (1 << 9)) != 0;
  const bool ds_a_timer        = (soundcnt_h & (1 << 10)) != 0;  // 0=Timer0, 1=Timer1
  const bool ds_b_enable_right = (soundcnt_h & (1 << 12)) != 0;
  const bool ds_b_enable_left  = (soundcnt_h & (1 << 13)) != 0;
  const bool ds_b_timer        = (soundcnt_h & (1 << 14)) != 0;

  // Volume: 0=50%, 1=100%
  const bool ds_a_full_vol = (soundcnt_h & (1 << 2)) != 0;
  const bool ds_b_full_vol = (soundcnt_h & (1 << 3)) != 0;

  // Accumulate cycles and generate samples at target sample rate
  cycle_accumulator_ += cycles;
  while (cycle_accumulator_ >= kCyclesPerSample && sample_count_ < kMaxSamplesPerFrame) {
    cycle_accumulator_ -= kCyclesPerSample;

    // Mix Direct Sound channels
    std::int32_t left = 0;
    std::int32_t right = 0;

    // Channel A
    const std::int32_t a_sample = static_cast<std::int32_t>(fifo_a_sample_) * (ds_a_full_vol ? 4 : 2);
    if (ds_a_enable_left)  left  += a_sample;
    if (ds_a_enable_right) right += a_sample;

    // Channel B
    const std::int32_t b_sample = static_cast<std::int32_t>(fifo_b_sample_) * (ds_b_full_vol ? 4 : 2);
    if (ds_b_enable_left)  left  += b_sample;
    if (ds_b_enable_right) right += b_sample;

    // Clamp to 16-bit range
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

  auto hwave = static_cast<HWAVEOUT>(hwaveout_);

  // Feed samples into the waveOut ring buffer, splitting into chunks that
  // fit within a single wave buffer if needed.
  int offset = 0;
  while (offset < count) {
    // Find a free buffer (not currently queued for playback).
    int target = -1;
    for (int i = 0; i < kNumBuffers; ++i) {
      auto* test_hdr = static_cast<WAVEHDR*>(wave_headers_[(current_buffer_ + i) % kNumBuffers]);
      if (!(test_hdr->dwFlags & WHDR_INQUEUE)) {
        target = (current_buffer_ + i) % kNumBuffers;
        break;
      }
    }
    if (target < 0) {
      // All buffers busy; drop the remaining samples to avoid blocking.
      return;
    }

    auto* hdr = static_cast<WAVEHDR*>(wave_headers_[target]);
    if (hdr->dwFlags & WHDR_PREPARED) {
      waveOutUnprepareHeader(hwave, hdr, sizeof(WAVEHDR));
    }

    const int chunk = (count - offset) > kMaxSamplesPerFrame ? kMaxSamplesPerFrame : (count - offset);
    const DWORD byte_count = static_cast<DWORD>(chunk) * 2 * sizeof(std::int16_t);
    std::memcpy(wave_buffers_[target].data(), samples + offset * 2, byte_count);

    hdr->lpData = reinterpret_cast<LPSTR>(wave_buffers_[target].data());
    hdr->dwBufferLength = byte_count;
    hdr->dwFlags = 0;

    if (waveOutPrepareHeader(hwave, hdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
      waveOutWrite(hwave, hdr, sizeof(WAVEHDR));
    }

    current_buffer_ = (target + 1) % kNumBuffers;
    offset += chunk;
  }
}

void Audio::flush_frame(Memory& /*memory*/) {
  if (!is_active_ || sample_count_ == 0) return;

  auto hwave = static_cast<HWAVEOUT>(hwaveout_);
  auto* hdr = static_cast<WAVEHDR*>(wave_headers_[current_buffer_]);

  // Find an available buffer that is not currently playing (WHDR_INQUEUE)
  if (hdr->dwFlags & WHDR_INQUEUE) {
    int found = -1;
    for (int i = 0; i < kNumBuffers; ++i) {
      auto* test_hdr = static_cast<WAVEHDR*>(wave_headers_[i]);
      if (!(test_hdr->dwFlags & WHDR_INQUEUE)) {
        found = i;
        break;
      }
    }
    if (found < 0) {
      sample_count_ = 0;
      return;  // All buffers busy playing, skip this frame to prevent driver crash
    }
    current_buffer_ = found;
    hdr = static_cast<WAVEHDR*>(wave_headers_[current_buffer_]);
  }

  if (hdr->dwFlags & WHDR_PREPARED) {
    waveOutUnprepareHeader(hwave, hdr, sizeof(WAVEHDR));
  }

  const DWORD byte_count = sample_count_ * 2 * sizeof(std::int16_t);
  std::memcpy(wave_buffers_[current_buffer_].data(), sample_buffer_.data(), byte_count);

  hdr->lpData = reinterpret_cast<LPSTR>(wave_buffers_[current_buffer_].data());
  hdr->dwBufferLength = byte_count;
  hdr->dwFlags = 0;

  if (waveOutPrepareHeader(hwave, hdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
    waveOutWrite(hwave, hdr, sizeof(WAVEHDR));
  }

  current_buffer_ = (current_buffer_ + 1) % kNumBuffers;
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

}  // namespace aw

#endif