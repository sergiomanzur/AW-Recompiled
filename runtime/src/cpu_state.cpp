#include "aw/cpu_state.hpp"

#include <utility>

namespace aw {

void trace(CpuState& state, std::string line) {
  if (state.trace_enabled) {
    state.trace_lines.push_back(std::move(line));
  }
}

void stop_at(CpuState& state, std::uint32_t target) {
  state.stop_target = target;
}

}  // namespace aw
