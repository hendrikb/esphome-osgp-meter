#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace osgp_meter {

class CRC16 {
 public:
  CRC16() { this->init_table_(); }

  uint16_t calculate(const uint8_t *data, size_t length, uint16_t initial) const {
    uint16_t crc = initial;
    for (size_t i = 0; i < length; i++) {
      uint8_t idx = (crc ^ data[i]) & 0xFF;
      crc = (crc >> 8) ^ this->table_[idx];
    }
    return crc;
  }

 private:
  uint16_t table_[256];

  void init_table_() {
    constexpr uint16_t polynom = 0x8408;
    for (uint16_t x = 0; x < 256; x++) {
      uint16_t w = x;
      for (uint8_t i = 0; i < 8; i++) {
        if ((w & 1) != 0) {
          w = (w >> 1) ^ polynom;
        } else {
          w = w >> 1;
        }
      }
      this->table_[x] = w;
    }
  }
};

}  // namespace osgp_meter
}  // namespace esphome
