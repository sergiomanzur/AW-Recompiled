#include "aw/probe/oam.hpp"

namespace aw {

namespace {

std::uint16_t read16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

}  // namespace

OamEntry decode_oam_entry(const std::uint8_t* oam, std::size_t index) {
  OamEntry entry;
  if (oam == nullptr || index >= kOamEntryCount) return entry;

  const std::uint8_t* p = oam + index * 8;
  const std::uint16_t attr0 = read16(p + 0);
  const std::uint16_t attr1 = read16(p + 2);
  const std::uint16_t attr2 = read16(p + 4);

  // attr0 bits 8-9 select the object mode; 0b10 means the object is disabled.
  entry.visible = ((attr0 >> 8) & 0x3) != 0x2;

  entry.y = attr0 & 0xFF;

  const int raw_x = attr1 & 0x1FF;
  entry.x = (raw_x >= 256) ? raw_x - 512 : raw_x;

  entry.tile = attr2 & 0x3FF;
  entry.priority = (attr2 >> 10) & 0x3;
  entry.palette = (attr2 >> 12) & 0xF;
  return entry;
}

}  // namespace aw
