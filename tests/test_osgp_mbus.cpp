#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "osgp_mbus.h"

using esphome::osgp_meter::mbus::Medium;
using esphome::osgp_meter::mbus::Quantity;
using esphome::osgp_meter::mbus::Reading;
using esphome::osgp_meter::mbus::Record;

namespace {

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void require_near(double actual, double expected, double tolerance, const char *message) {
  require(std::fabs(actual - expected) <= tolerance, message);
}

const Record &find_record(const Reading &reading, Quantity quantity, uint32_t storage = 0) {
  for (const Record &record : reading.records) {
    if (record.quantity == quantity && record.storage == storage) {
      return record;
    }
  }
  std::cerr << "FAIL: expected record was not found\n";
  std::exit(1);
}

std::vector<uint8_t> water_fixture() {
  return {
      26, 8, 23, 0, 0, 1, 0x04,  // timestamp and response info
      34,                          // telegram length
      0x78, 0x56, 0x34, 0x12,     // sanitized serial 12345678
      0x00, 0x00, 0x01, 0x16, 0x01, 0x00, 0x00, 0x00,
      0x04, 0x13, 0x40, 0xE2, 0x01, 0x00,              // 123.456 m3
      0x82, 0x01, 0x27, 0x6D, 0x01,                    // storage 2, 365 days
      0x44, 0x13, 0xC0, 0xD4, 0x01, 0x00,              // storage 1, 120.000 m3
      0x02, 0x3B, 0xFA, 0x00,                          // 0.250 m3/h
      0x00,
  };
}

std::vector<uint8_t> heat_fixture() {
  return {
      26, 8, 23, 0, 0, 2, 0x04,
      42,
      0x21, 0x43, 0x65, 0x87,  // sanitized serial 87654321
      0x00, 0x00, 0x01, 0x04, 0x02, 0x00, 0x00, 0x00,
      0x04, 0x06, 0xE1, 0x10, 0x00, 0x00,  // 4321 kWh after normalization
      0x0B, 0x2D, 0x15, 0x00, 0x00,        // 1500 W
      0x0B, 0x3B, 0x50, 0x02, 0x00,        // 0.250 m3/h
      0x0A, 0x5A, 0x56, 0x04,              // 45.6 C
      0x0A, 0x5E, 0x21, 0x03,              // 32.1 C
      0x0B, 0x61, 0x50, 0x13, 0x00,        // 13.50 K
      0x00,
  };
}

void test_water_fixture() {
  Reading reading;
  std::string error;
  const auto fixture = water_fixture();
  require(esphome::osgp_meter::mbus::parse_reading(fixture.data(), fixture.size(), reading, &error), error.c_str());
  require(reading.serial_number == "12345678", "water serial");
  require(reading.medium == 0x16, "cold-water medium");
  require(reading.timestamp == "2026-08-23T00:00:01", "water timestamp");
  require_near(find_record(reading, Quantity::VOLUME).value, 123.456, 0.000001, "water total");
  require_near(find_record(reading, Quantity::VOLUME, 1).value, 120.0, 0.000001, "stored water total");
  require(find_record(reading, Quantity::DURATION, 2).storage == 2, "DIFE storage decoding");
  require_near(find_record(reading, Quantity::VOLUME_FLOW_HOUR).value, 0.25, 0.000001, "water flow");
}

void test_heat_fixture_and_normalization() {
  Reading reading;
  std::string error;
  const auto fixture = heat_fixture();
  require(esphome::osgp_meter::mbus::parse_reading(fixture.data(), fixture.size(), reading, &error), error.c_str());
  require(reading.serial_number == "87654321", "heat serial");
  double value = 0.0;
  require(esphome::osgp_meter::mbus::normalized_value(find_record(reading, Quantity::ENERGY_WH),
                                                       Quantity::ENERGY_WH, value),
          "normalize heat energy");
  require_near(value, 4321.0, 0.000001, "heat energy in kWh");
  require_near(find_record(reading, Quantity::POWER_W).value, 1500.0, 0.000001, "thermal power");
  require_near(find_record(reading, Quantity::FLOW_TEMPERATURE).value, 45.6, 0.000001, "flow temperature");
  require_near(find_record(reading, Quantity::RETURN_TEMPERATURE).value, 32.1, 0.000001, "return temperature");
  require_near(find_record(reading, Quantity::TEMPERATURE_DIFFERENCE).value, 13.5, 0.000001,
               "temperature difference");

  Record per_second;
  per_second.numeric = true;
  per_second.quantity = Quantity::VOLUME_FLOW_SECOND;
  per_second.value = 0.001;
  require(esphome::osgp_meter::mbus::canonical_value(per_second, value), "canonical flow");
  require_near(value, 3.6, 0.000001, "m3/s to m3/h");

  Record joules_per_hour;
  joules_per_hour.numeric = true;
  joules_per_hour.quantity = Quantity::POWER_J_PER_HOUR;
  joules_per_hour.value = 3600.0;
  require(esphome::osgp_meter::mbus::canonical_value(joules_per_hour, value), "canonical power");
  require_near(value, 1.0, 0.000001, "J/h to W");
}

void test_malformed_and_security_data() {
  Reading reading;
  std::string error;
  auto truncated = water_fixture();
  truncated.pop_back();
  require(!esphome::osgp_meter::mbus::parse_reading(truncated.data(), truncated.size(), reading, &error),
          "truncated telegram must fail");

  auto unauthenticated = water_fixture();
  unauthenticated[6] = 0x0C;
  require(!esphome::osgp_meter::mbus::parse_reading(unauthenticated.data(), unauthenticated.size(), reading, &error),
          "authentication failure must fail");

  auto bad_bcd = water_fixture();
  bad_bcd[8] = 0xFA;
  require(!esphome::osgp_meter::mbus::parse_reading(bad_bcd.data(), bad_bcd.size(), reading, &error),
          "invalid serial BCD must fail");
}

void test_fixed_data() {
  const std::vector<uint8_t> fixture = {
      26, 8, 23, 12, 34, 56, 0x06,  // timestamp, fixed data, key available
      16,
      0x78, 0x56, 0x34, 0x12,  // sanitized serial 12345678
      0x07, 0x00, 0xEC, 0x7E,  // access, status, water, m3 / same historic unit
      0x56, 0x34, 0x12, 0x00,  // BCD counter 1: 123456 m3
      0x21, 0x43, 0x65, 0x00,  // BCD counter 2: 654321 m3
  };
  Reading reading;
  std::string error;
  require(esphome::osgp_meter::mbus::parse_reading(fixture.data(), fixture.size(), reading, &error), error.c_str());
  require(reading.fixed_data, "fixed-data marker");
  require(reading.serial_number == "12345678", "fixed-data serial");
  require(reading.access_number == 7, "fixed-data access number");
  require(reading.medium == static_cast<uint8_t>(Medium::WATER), "fixed-data medium");
  require(reading.records.size() == 2, "fixed-data counter count");
  require(reading.records[0].quantity == Quantity::VOLUME, "fixed-data counter 1 unit");
  require(reading.records[1].quantity == Quantity::VOLUME, "fixed-data historic unit");
  require_near(reading.records[0].value, 123456.0, 0.000001, "fixed-data counter 1");
  require_near(reading.records[1].value, 654321.0, 0.000001, "fixed-data counter 2");
}

void test_matching_and_ring_helpers() {
  using esphome::osgp_meter::mbus::medium_matches;
  require(medium_matches(Medium::WATER, static_cast<uint8_t>(Medium::WATER)), "generic water medium");
  require(medium_matches(Medium::WATER, static_cast<uint8_t>(Medium::HOT_WATER)), "hot water compatibility");
  require(medium_matches(Medium::WATER, static_cast<uint8_t>(Medium::COLD_WATER)), "cold water compatibility");
  require(!medium_matches(Medium::HEAT, static_cast<uint8_t>(Medium::WATER)), "medium mismatch");
  require(esphome::osgp_meter::mbus::valid_slot_hint(0), "automatic slot");
  require(esphome::osgp_meter::mbus::valid_slot_hint(4), "last valid slot");
  require(!esphome::osgp_meter::mbus::valid_slot_hint(5), "invalid slot");
  require(esphome::osgp_meter::mbus::previous_ring_index(0, 36) == 35, "ET45 wraparound");
  require(esphome::osgp_meter::mbus::previous_ring_index(12, 36) == 11, "ET45 previous entry");
  require(esphome::osgp_meter::mbus::previous_ring_index(12, 0) == 0, "empty ET45 ring");
}

}  // namespace

int main() {
  test_water_fixture();
  test_heat_fixture_and_normalization();
  test_malformed_and_security_data();
  test_fixed_data();
  test_matching_and_ring_helpers();
  std::cout << "M-Bus parser tests passed\n";
  return 0;
}
