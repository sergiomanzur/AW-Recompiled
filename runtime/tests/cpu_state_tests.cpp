#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "aw/cpu_state.hpp"

namespace {

template <typename T, typename U>
void require_equal(const T& actual, const U& expected, const char* label) {
  if (!(actual == expected)) {
    std::ostringstream out;
    out << label << ": expected `" << expected << "`, got `" << actual << "`";
    throw std::runtime_error(out.str());
  }
}

void records_trace_only_when_enabled() {
  aw::CpuState state;

  aw::trace(state, "hidden");
  require_equal(state.trace_lines.size(), std::size_t{0}, "disabled trace count");

  state.trace_enabled = true;
  aw::trace(state, "visible");
  require_equal(state.trace_lines.size(), std::size_t{1}, "enabled trace count");
  require_equal(state.trace_lines.front(), std::string("visible"), "trace line");
}

void records_unresolved_stop_target() {
  aw::CpuState state;
  aw::stop_at(state, 0x0807AD11);

  require_equal(state.stop_target, 0x0807AD11u, "stop target");
}

}  // namespace

int main() {
  try {
    records_trace_only_when_enabled();
    records_unresolved_stop_target();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }

  return 0;
}
