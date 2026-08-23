#pragma once

#include "aw/input/input_frame.hpp"

namespace aw {

// A platform binding that contributes to the current frame. Implementations
// OR into `frame` rather than overwriting it, so several sources compose.
class InputSource {
public:
  virtual ~InputSource() = default;

  // Called once per frame, before the emulator runs.
  virtual void poll(InputFrame& frame) = 0;
};

}  // namespace aw
