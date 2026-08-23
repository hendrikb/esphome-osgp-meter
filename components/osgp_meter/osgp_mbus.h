#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace osgp_meter {
namespace mbus {

enum class Medium : uint8_t {
  AUTO = 0,
  GAS = 3,
  HEAT = 4,
  HOT_WATER = 6,
  WATER = 7,
  COLD_WATER = 0x16,
};

enum class Quantity : uint8_t {
  UNKNOWN,
  ENERGY_WH,
  ENERGY_J,
  VOLUME,
  MASS,
  DURATION,
  POWER_W,
  POWER_J_PER_HOUR,
  VOLUME_FLOW_HOUR,
  VOLUME_FLOW_MINUTE,
  VOLUME_FLOW_SECOND,
  MASS_FLOW,
  FLOW_TEMPERATURE,
  RETURN_TEMPERATURE,
  TEMPERATURE_DIFFERENCE,
  EXTERNAL_TEMPERATURE,
  PRESSURE,
};

struct Record {
  uint8_t dif{0};
  uint8_t vif{0};
  uint8_t function{0};
  uint32_t storage{0};
  uint32_t tariff{0};
  uint32_t subunit{0};
  std::vector<uint8_t> vife{};
  Quantity quantity{Quantity::UNKNOWN};
  double value{0.0};
  bool numeric{false};
};

struct Reading {
  std::string timestamp{};
  std::string serial_number{};
  uint8_t medium{0};
  uint8_t access_number{0};
  uint8_t status{0};
  bool security_ok{false};
  bool fixed_data{false};
  std::vector<Record> records{};
};

bool parse_reading(const uint8_t *data, size_t length, Reading &reading, std::string *error = nullptr);
const char *quantity_name(Quantity quantity);
bool normalized_value(const Record &record, Quantity wanted, double &value);
bool canonical_value(const Record &record, double &value);
bool medium_matches(Medium configured, uint8_t received);
bool valid_slot_hint(uint8_t slot);
uint16_t previous_ring_index(uint16_t index, uint16_t count);

}  // namespace mbus
}  // namespace osgp_meter
}  // namespace esphome
