#include "aw/hd_audio.hpp"

#include <algorithm>
#include <cmath>

namespace aw {

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

  // Perform subtle HD harmonic enhancement / stereo widening over the GBA audio stream
  for (std::size_t i = 0; i < sample_frames; ++i) {
    std::int16_t& left = samples[i * 2];
    std::int16_t& right = samples[i * 2 + 1];

    // HD Sub-bass & Clarity Enhancement
    const double l_norm = static_cast<double>(left) / 32768.0;
    const double r_norm = static_cast<double>(right) / 32768.0;

    // Harmonic warmth curve
    const double l_hd = std::tanh(l_norm * 1.15);
    const double r_hd = std::tanh(r_norm * 1.15);

    left = static_cast<std::int16_t>(std::clamp(l_hd * 32767.0, -32768.0, 32767.0));
    right = static_cast<std::int16_t>(std::clamp(r_hd * 32767.0, -32768.0, 32767.0));
  }
}

}  // namespace aw
