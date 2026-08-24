#pragma once

#include "aw/probe/backend.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace aw {

class HdAudioEngine {
public:
  void update(ProbeBackend& backend);

  // Mixes HD audio synthesis/track stereo samples over the raw GBA PCM buffer
  void mix_audio(std::int16_t* samples, std::size_t sample_frames);

  bool enabled() const { return enabled_; }
  void set_enabled(bool enabled) { enabled_ = enabled; }

private:
  bool enabled_ = true;
  int current_track_id_ = -1;
  double phase_left_ = 0.0;
  double phase_right_ = 0.0;
};

}  // namespace aw
