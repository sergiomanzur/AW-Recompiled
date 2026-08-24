#include "aw/probe/backend_mgba.hpp"

#include "aw/mgba_adapter.h"
#include "aw/probe/oam.hpp"

#include <iostream>

namespace aw {

void MgbaProbeBackend::set_core(void* core) {
  core_ = core;
  resolved_ = false;
  oam_ = nullptr;
  ewram_ = nullptr;
  ewram_size_ = 0;
  iwram_ = nullptr;
  iwram_size_ = 0;
}

void MgbaProbeBackend::resolve() {
  if (resolved_ || core_ == nullptr) return;
  resolved_ = true;

  auto* core = static_cast<struct mCore*>(core_);

  std::size_t oam_size = 0;
  void* oam_ptr = aw_mgba_memory_block(core, "oam", &oam_size);
  if (oam_ptr != nullptr && oam_size >= aw::kOamBytes) {
    oam_ = static_cast<const std::uint8_t*>(oam_ptr);
  }

  std::size_t ewram_size = 0;
  void* ewram_ptr = aw_mgba_memory_block(core, "wram", &ewram_size);
  if (ewram_ptr != nullptr && ewram_size > 0) {
    ewram_ = static_cast<const std::uint8_t*>(ewram_ptr);
    ewram_size_ = ewram_size;
  }

  // IWRAM is optional: unavailability doesn't disable pointer navigation
  // (which only needs OAM/EWRAM), but offline tooling wants it too.
  std::size_t iwram_size = 0;
  void* iwram_ptr = aw_mgba_memory_block(core, "iwram", &iwram_size);
  if (iwram_ptr != nullptr && iwram_size > 0) {
    iwram_ = static_cast<const std::uint8_t*>(iwram_ptr);
    iwram_size_ = iwram_size;
  }

  if (oam_ == nullptr || ewram_ == nullptr) {
    std::cerr << "[probe] mGBA memory blocks unavailable (oam="
              << (oam_ != nullptr) << ", wram=" << (ewram_ != nullptr)
              << "); pointer navigation disabled\n";
  }
}

bool MgbaProbeBackend::available() {
  resolve();
  return oam_ != nullptr && ewram_ != nullptr;
}

const std::uint8_t* MgbaProbeBackend::oam() {
  resolve();
  return oam_;
}

const std::uint8_t* MgbaProbeBackend::ewram(std::size_t& size_out) {
  resolve();
  size_out = ewram_size_;
  return ewram_;
}

const std::uint8_t* MgbaProbeBackend::iwram(std::size_t& size_out) {
  resolve();
  size_out = iwram_size_;
  return iwram_;
}

std::uint16_t MgbaProbeBackend::read_io16(std::uint32_t addr) {
  if (core_ == nullptr) return 0;
  return aw_mgba_read16(static_cast<struct mCore*>(core_), addr);
}

}  // namespace aw
