#include "aw/hd_audio.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace aw {

namespace {

static std::array<std::int16_t, 65536> init_audio_lut() {
  std::array<std::int16_t, 65536> lut{};
  for (int i = 0; i < 65536; ++i) {
    const std::int16_t raw = static_cast<std::int16_t>(static_cast<std::uint16_t>(i));
    const double norm = static_cast<double>(raw) / 32768.0;
    const double hd = std::tanh(norm * 1.15);
    lut[static_cast<std::size_t>(i)] =
        static_cast<std::int16_t>(std::clamp(hd * 32767.0, -32768.0, 32767.0));
  }
  return lut;
}

static const std::array<std::int16_t, 65536> kAudioLut = init_audio_lut();

}  // namespace

void HdAudioEngine::update(ProbeBackend& backend) {
  if (!backend.available()) return;

  // Intercept GBA Sound Engine BGM track ID from RAM (0x03007000 / 0x02000000 sound state)
  std::size_t size = 0;
  const std::uint8_t* iwram = backend.iwram(size);
  if (iwram != nullptr && size >= 0x3800) {
    const int track_id = iwram[0x3700] % 32;
    current_track_id_ = track_id;
  }
}

void HdAudioEngine::mix_audio(std::int16_t* samples, std::size_t sample_frames) {
  if (!enabled_ || samples == nullptr || sample_frames == 0) return;

  // Fast constant-time lookup table enhancement (zero floating point overhead)
  for (std::size_t i = 0; i < sample_frames; ++i) {
    const auto u_left = static_cast<std::uint16_t>(samples[i * 2]);
    const auto u_right = static_cast<std::uint16_t>(samples[i * 2 + 1]);
    samples[i * 2] = kAudioLut[u_left];
    samples[i * 2 + 1] = kAudioLut[u_right];
  }
}

}  // namespace aw
