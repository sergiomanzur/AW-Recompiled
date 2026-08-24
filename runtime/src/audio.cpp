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

void Audio::set_sample_rate(int sample_rate) {
  if (!is_active_ || sample_rate <= 0 || sample_rate == sample_rate_) return;

  // Stop playback and drop everything queued at the old rate.
  reset_streams();
  waveOutClose(static_cast<HWAVEOUT>(hwaveout_));
  hwaveout_ = nullptr;
  is_active_ = false;

  WAVEFORMATEX wfx = {};
  wfx.wFormatTag = WAVE_FORMAT_PCM;
  wfx.nChannels = 2; // Stereo
  wfx.nSamplesPerSec = static_cast<DWORD>(sample_rate);
  wfx.wBitsPerSample = 16;
  wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
  wfx.cbSize = 0;

  HWAVEOUT new_hwave = nullptr;
  if (waveOutOpen(&new_hwave, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
    hwaveout_ = static_cast<void*>(new_hwave);
    is_active_ = true;
    sample_rate_ = sample_rate;
  }
}

void Audio::drop_pending() {
  if (!is_active_) return;
  reset_streams();
}

void Audio::reset_streams() {
  if (hwaveout_ == nullptr) return;
  auto hwave = static_cast<HWAVEOUT>(hwaveout_);

  // Stop playback and return all queued blocks to us.
  waveOutReset(hwave);
  for (int i = 0; i < kNumBuffers; ++i) {
    if (wave_headers_[i] == nullptr) continue;
    auto* hdr = static_cast<WAVEHDR*>(wave_headers_[i]);
    if (hdr->dwFlags & WHDR_PREPARED) {
      waveOutUnprepareHeader(hwave, hdr, sizeof(WAVEHDR));
    }
    hdr->dwFlags = 0;
    hdr->dwBufferLength = 0;
  }

  ring_head_ = 0;
  ring_tail_ = 0;
  current_buffer_ = 0;
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

std::size_t Audio::ring_queued_frames() const {
  const std::size_t used_int16 = (ring_head_ >= ring_tail_)
      ? (ring_head_ - ring_tail_)
      : (kRingSize - ring_tail_ + ring_head_);
  return used_int16 / 2;
}

int Audio::queued_frames() const {
  if (!is_active_) return 0;

  // Samples still sitting in the ring buffer...
  std::size_t frames = ring_queued_frames();

  // ...plus samples handed to waveOut that have not finished playing yet.
  for (int i = 0; i < kNumBuffers; ++i) {
    if (wave_headers_[i] == nullptr) continue;
    const auto* hdr = static_cast<const WAVEHDR*>(wave_headers_[i]);
    if ((hdr->dwFlags & WHDR_INQUEUE) && !(hdr->dwFlags & WHDR_DONE)) {
      frames += hdr->dwBufferLength / (2 * sizeof(std::int16_t));
    }
  }

  return static_cast<int>(frames);
}

void Audio::push_samples(const std::int16_t* samples, int count) {
  if (!is_active_ || samples == nullptr || count <= 0) return;

  // If the ring buffer is (nearly) full, drop the oldest samples so the head
  // never overtakes the tail. Overwriting un-read data corrupts playback
  // (crackle/garbage); dropping stale audio is the lesser evil.
  const std::size_t incoming = static_cast<std::size_t>(count) * 2;
  std::size_t used = (ring_head_ >= ring_tail_)
      ? (ring_head_ - ring_tail_)
      : (kRingSize - ring_tail_ + ring_head_);
  if (used + incoming > kRingSize) {
    std::size_t to_drop = used + incoming - kRingSize;
    // Keep the ring an even number of int16s (whole stereo frames).
    if (to_drop & 1) ++to_drop;
    ring_tail_ = (ring_tail_ + to_drop) % kRingSize;
  }

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
  std::size_t avail_frames = ring_queued_frames();

  // Feed available blocks into waveOut driver
  while (avail_frames >= static_cast<std::size_t>(kBlockSamples)) {
    // Find a free waveOut buffer. A buffer is reusable once it has finished
    // playing (WHDR_DONE), even if the driver has not yet cleared
    // WHDR_INQUEUE — relying on INQUEUE alone stalls recycling on Windows and
    // kills audio after the first 16 blocks.
    int target = -1;
    for (int i = 0; i < kNumBuffers; ++i) {
      auto* test_hdr = static_cast<WAVEHDR*>(wave_headers_[(current_buffer_ + i) % kNumBuffers]);
      const DWORD flags = test_hdr->dwFlags;
      if (!(flags & WHDR_INQUEUE) || (flags & WHDR_DONE)) {
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
void Audio::set_sample_rate(int /*sample_rate*/) {}
void Audio::drop_pending() {}
void Audio::pump_waveout() {}
void Audio::reset_streams() {}
int Audio::queued_frames() const { return 0; }
std::size_t Audio::ring_queued_frames() const { return 0; }

}  // namespace aw

#endif