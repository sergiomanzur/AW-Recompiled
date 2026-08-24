#pragma once

#include "aw/probe/backend.hpp"

namespace aw {

// ProbeBackend over a live mGBA core. Block pointers are resolved once and
// cached; call set_core() again after a ROM switch to invalidate them.
class MgbaProbeBackend final : public ProbeBackend {
public:
  MgbaProbeBackend() = default;
  explicit MgbaProbeBackend(void* core) { set_core(core); }

  void set_core(void* core);

  bool available() override;
  const std::uint8_t* oam() override;
  const std::uint8_t* ewram(std::size_t& size_out) override;
  const std::uint8_t* iwram(std::size_t& size_out) override;
  std::uint16_t read_io16(std::uint32_t addr) override;

private:
  void resolve();

  void* core_ = nullptr;
  bool resolved_ = false;
  const std::uint8_t* oam_ = nullptr;
  const std::uint8_t* ewram_ = nullptr;
  std::size_t ewram_size_ = 0;
  const std::uint8_t* iwram_ = nullptr;
  std::size_t iwram_size_ = 0;
};

}  // namespace aw
