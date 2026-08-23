#include "osgp_meter.h"

#include <algorithm>

#include "esphome/core/log.h"
#include "osgp_protocol_constants.h"
#include "osgp_utils.h"

namespace esphome {
namespace osgp_meter {

static const char *const TAG = "osgp_meter";

namespace {

uint16_t read_u16(const uint8_t *data, bool little) {
  if (little) {
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
  }
  return (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
}

const char *device_status_label(uint8_t status) {
  switch (status) {
    case 0:
      return "missing";
    case 1:
      return "active";
    case 2:
      return "down";
    default:
      return "unknown";
  }
}

bool is_current_record(const mbus::Record &record) {
  return record.numeric && record.function == 0 && record.storage == 0 && record.tariff == 0 && record.subunit == 0;
}

}  // namespace

void OSGPMeter::add_mbus_device(const std::string &serial_number, uint8_t slot_hint, uint8_t medium) {
  MBusDevice device;
  device.serial_number = serial_number;
  device.slot_hint = mbus::valid_slot_hint(slot_hint) ? slot_hint : 0;
  device.configured_medium = medium;
  this->mbus_devices_.push_back(std::move(device));
}

void OSGPMeter::set_mbus_total_volume_sensor(size_t index, sensor::Sensor *sensor) {
  if (index < this->mbus_devices_.size())
    this->mbus_devices_[index].total_volume_sensor = sensor;
}
void OSGPMeter::set_mbus_total_energy_sensor(size_t index, sensor::Sensor *sensor) {
  if (index < this->mbus_devices_.size())
    this->mbus_devices_[index].total_energy_sensor = sensor;
}
void OSGPMeter::set_mbus_volume_flow_rate_sensor(size_t index, sensor::Sensor *sensor) {
  if (index < this->mbus_devices_.size())
    this->mbus_devices_[index].volume_flow_rate_sensor = sensor;
}
void OSGPMeter::set_mbus_thermal_power_sensor(size_t index, sensor::Sensor *sensor) {
  if (index < this->mbus_devices_.size())
    this->mbus_devices_[index].thermal_power_sensor = sensor;
}
void OSGPMeter::set_mbus_flow_temperature_sensor(size_t index, sensor::Sensor *sensor) {
  if (index < this->mbus_devices_.size())
    this->mbus_devices_[index].flow_temperature_sensor = sensor;
}
void OSGPMeter::set_mbus_return_temperature_sensor(size_t index, sensor::Sensor *sensor) {
  if (index < this->mbus_devices_.size())
    this->mbus_devices_[index].return_temperature_sensor = sensor;
}
void OSGPMeter::set_mbus_temperature_difference_sensor(size_t index, sensor::Sensor *sensor) {
  if (index < this->mbus_devices_.size())
    this->mbus_devices_[index].temperature_difference_sensor = sensor;
}
#ifdef USE_TEXT_SENSOR
void OSGPMeter::set_mbus_last_read_text_sensor(size_t index, text_sensor::TextSensor *sensor) {
  if (index < this->mbus_devices_.size())
    this->mbus_devices_[index].last_read_text_sensor = sensor;
}
void OSGPMeter::set_mbus_status_text_sensor(size_t index, text_sensor::TextSensor *sensor) {
  if (index < this->mbus_devices_.size())
    this->mbus_devices_[index].status_text_sensor = sensor;
}
#endif

void OSGPMeter::add_mbus_record_sensor(size_t device_index, sensor::Sensor *sensor, uint8_t dif, uint8_t vif,
                                       uint8_t function, int64_t storage, int64_t tariff, int64_t subunit) {
  if (device_index >= this->mbus_devices_.size())
    return;
  MBusRecordSensor record;
  record.sensor = sensor;
  record.dif = dif;
  record.vif = vif;
  record.function = function;
  record.storage = storage;
  record.tariff = tariff;
  record.subunit = subunit;
  this->mbus_devices_[device_index].records.push_back(std::move(record));
}

void OSGPMeter::add_mbus_record_vife(size_t device_index, size_t record_index, uint8_t vife) {
  if (device_index < this->mbus_devices_.size() && record_index < this->mbus_devices_[device_index].records.size()) {
    this->mbus_devices_[device_index].records[record_index].vife.push_back(vife);
  }
}

bool OSGPMeter::consume_partial_table_reply_(std::vector<uint8_t> &data, const char *context) {
  ByteReader reader(std::vector<uint8_t>{});
  if (!this->consume_request_response_ack_(reader, context) || reader.limit() - reader.position() < 2) {
    return false;
  }
  const uint16_t table_length = reader.get_u16_be();
  if (reader.limit() - reader.position() < table_length) {
    ESP_LOGD(TAG, "%s returned %u data bytes, expected %u", context,
             static_cast<unsigned>(reader.limit() - reader.position()), static_cast<unsigned>(table_length));
    return false;
  }
  data.clear();
  data.reserve(table_length);
  for (uint16_t i = 0; i < table_length; i++) {
    data.push_back(reader.get_u8());
  }
  return true;
}

bool OSGPMeter::parse_mbus_et11_(const std::vector<uint8_t> &data) {
  if (data.size() < 7) {
    return false;
  }
  this->mbus_device_count_ = data[0];
  this->mbus_status_entry_size_ = data[2];
  this->mbus_data_entry_size_ = read_u16(data.data() + 5, this->byte_order_little_);
  if (this->mbus_device_count_ < 4 || this->mbus_status_entry_size_ < 6 || this->mbus_data_entry_size_ < 24) {
    ESP_LOGD(TAG, "Unsupported ET11 dimensions: devices=%u status=%u data=%u",
             static_cast<unsigned>(this->mbus_device_count_), static_cast<unsigned>(this->mbus_status_entry_size_),
             static_cast<unsigned>(this->mbus_data_entry_size_));
    return false;
  }
  this->mbus_dimensions_loaded_ = true;
  ESP_LOGI(TAG, "M-Bus dimensions: devices=%u status_entry=%u data_entry=%u",
           static_cast<unsigned>(this->mbus_device_count_), static_cast<unsigned>(this->mbus_status_entry_size_),
           static_cast<unsigned>(this->mbus_data_entry_size_));
  return true;
}

bool OSGPMeter::parse_mbus_et14_header_(const std::vector<uint8_t> &data) {
  if (data.size() < 6) {
    return false;
  }
  const uint8_t occupancy = data[5];
  for (size_t slot = 0; slot < this->mbus_slots_.size(); slot++) {
    MBusSlot &info = this->mbus_slots_[slot];
    info = MBusSlot{};
    info.occupied = (occupancy & (1U << slot)) != 0;
  }
  for (MBusDevice &device : this->mbus_devices_) {
    if (device.slot >= 1 && device.slot <= this->mbus_slots_.size() &&
        !this->mbus_slots_[device.slot - 1].occupied) {
      device.slot = 0;
      device.handle = 0;
      device.found = false;
    }
  }
  ESP_LOGI(TAG, "M-Bus occupied slots: 0x%02X", static_cast<unsigned>(occupancy & 0x0F));
  return true;
}

bool OSGPMeter::parse_mbus_et14_entry_(uint8_t slot, const std::vector<uint8_t> &data) {
  if (slot < 1 || slot > this->mbus_slots_.size() || data.size() < 6) {
    return false;
  }
  MBusSlot &info = this->mbus_slots_[slot - 1];
  info.handle = read_u16(data.data(), this->byte_order_little_);
  info.status = data[2];
  info.billing_read_length = read_u16(data.data() + 4, this->byte_order_little_);

  for (MBusDevice &device : this->mbus_devices_) {
    if (device.slot == slot || (!device.found && device.slot_hint == slot)) {
      if (device.found && device.handle != 0 && device.handle != info.handle) {
        device.found = false;
      }
      device.handle = info.handle;
      device.device_status = info.status;
#ifdef USE_TEXT_SENSOR
      utils::publish_state_if_present(device.status_text_sensor, device_status_label(info.status));
#endif
    }
  }
  return true;
}

bool OSGPMeter::parse_mbus_et36_(const std::vector<uint8_t> &data) {
  if (data.size() < 17) {
    return false;
  }
  const uint16_t table_id = read_u16(data.data(), this->byte_order_little_);
  if (table_id != 0x082D) {
    ESP_LOGD(TAG, "ET36 entry 5 describes table 0x%04X instead of ET45", static_cast<unsigned>(table_id));
    return false;
  }
  this->mbus_et45_entry_size_ = read_u16(data.data() + 2, this->byte_order_little_);
  this->mbus_et45_current_entries_ = read_u16(data.data() + 6, this->byte_order_little_);
  if (this->mbus_et45_entry_size_ < 16) {
    return false;
  }
  this->mbus_et45_dimensions_loaded_ = true;
  ESP_LOGI(TAG, "M-Bus ET45 dimensions: entry_size=%u entries=%u",
           static_cast<unsigned>(this->mbus_et45_entry_size_),
           static_cast<unsigned>(this->mbus_et45_current_entries_));
  return true;
}

bool OSGPMeter::parse_mbus_et45_header_(const std::vector<uint8_t> &data) {
  if (data.size() < 6) {
    return false;
  }
  const uint16_t valid_entries = data[1];
  const uint16_t last_entry = data[2];
  this->mbus_et45_pending_sequence_ = read_u16(data.data() + 3, this->byte_order_little_);
  if (this->mbus_et45_sequence_valid_ && this->mbus_et45_pending_sequence_ == this->mbus_et45_sequence_) {
    this->mbus_et45_scan_remaining_ = 0;
    return true;
  }
  if (this->mbus_et45_current_entries_ == 0 || valid_entries == 0 || last_entry >= this->mbus_et45_current_entries_) {
    this->mbus_et45_scan_remaining_ = 0;
    return true;
  }
  this->mbus_et45_scan_remaining_ = std::min(valid_entries, this->mbus_et45_current_entries_);
  this->mbus_et45_scan_index_ = last_entry;
  return true;
}

void OSGPMeter::parse_mbus_et16_entry_(uint8_t slot, const std::vector<uint8_t> &data) {
  mbus::Reading reading;
  std::string error;
  if (!mbus::parse_reading(data.data(), data.size(), reading, &error)) {
    ESP_LOGD(TAG, "M-Bus slot %u ET16 data ignored: %s", static_cast<unsigned>(slot), error.c_str());
    return;
  }
  for (size_t i = 0; i < this->mbus_devices_.size(); i++) {
    MBusDevice &device = this->mbus_devices_[i];
    if (device.serial_number != reading.serial_number)
      continue;
    device.slot = slot;
    device.handle = this->mbus_slots_[slot - 1].handle;
    device.device_status = this->mbus_slots_[slot - 1].status;
    device.found = true;
    if (device.slot_hint != 0 && device.slot_hint != slot) {
      ESP_LOGI(TAG, "M-Bus serial %s discovered in slot %u (configured hint was %u)", device.serial_number.c_str(),
               static_cast<unsigned>(slot), static_cast<unsigned>(device.slot_hint));
    }
    this->publish_mbus_reading_(device, reading);
    return;
  }
  ESP_LOGD(TAG, "Ignoring unconfigured M-Bus serial %s in slot %u", reading.serial_number.c_str(),
           static_cast<unsigned>(slot));
}

void OSGPMeter::parse_mbus_et45_entry_(const std::vector<uint8_t> &data) {
  if (data.size() < 8)
    return;
  const uint16_t entry_length = read_u16(data.data(), this->byte_order_little_);
  if (entry_length < 6 || static_cast<size_t>(entry_length) + 2U > data.size())
    return;
  const uint16_t handle = read_u16(data.data() + 4, this->byte_order_little_);
  if (data[7] != 4)
    return;

  const size_t reading_length = entry_length - 6U;
  mbus::Reading reading;
  std::string error;
  if (!mbus::parse_reading(data.data() + 8, reading_length, reading, &error)) {
    ESP_LOGD(TAG, "M-Bus ET45 handle %u ignored: %s", static_cast<unsigned>(handle), error.c_str());
    return;
  }
  for (size_t i = 0; i < this->mbus_devices_.size(); i++) {
    MBusDevice &device = this->mbus_devices_[i];
    if ((device.handle != 0 && device.handle != handle) || device.serial_number != reading.serial_number)
      continue;
    device.handle = handle;
    device.found = true;
    this->publish_mbus_reading_(device, reading);
    if (i < this->mbus_cycle_devices_found_.size())
      this->mbus_cycle_devices_found_[i] = true;
    return;
  }
}

void OSGPMeter::publish_mbus_reading_(MBusDevice &device, const mbus::Reading &reading) {
  if (!mbus::medium_matches(static_cast<mbus::Medium>(device.configured_medium), reading.medium)) {
    ESP_LOGD(TAG, "M-Bus serial %s medium mismatch: configured=%u received=%u", device.serial_number.c_str(),
             static_cast<unsigned>(device.configured_medium), static_cast<unsigned>(reading.medium));
#ifdef USE_TEXT_SENSOR
    utils::publish_state_if_present(device.status_text_sensor, "medium_mismatch");
#endif
    return;
  }

#ifdef USE_TEXT_SENSOR
  if (!reading.timestamp.empty())
    utils::publish_state_if_present(device.last_read_text_sensor, reading.timestamp);
  utils::publish_state_if_present(device.status_text_sensor, device.device_status == 2 ? "down" : "active");
#endif

  auto publish_quantity = [&](sensor::Sensor *sensor, mbus::Quantity quantity) {
    if (sensor == nullptr)
      return;
    for (const mbus::Record &record : reading.records) {
      if (!is_current_record(record))
        continue;
      double value;
      if (mbus::normalized_value(record, quantity, value)) {
        sensor->publish_state(static_cast<float>(value));
        return;
      }
    }
  };

  publish_quantity(device.total_volume_sensor, mbus::Quantity::VOLUME);
  publish_quantity(device.total_energy_sensor, mbus::Quantity::ENERGY_WH);
  publish_quantity(device.volume_flow_rate_sensor, mbus::Quantity::VOLUME_FLOW_HOUR);
  publish_quantity(device.thermal_power_sensor, mbus::Quantity::POWER_W);
  publish_quantity(device.flow_temperature_sensor, mbus::Quantity::FLOW_TEMPERATURE);
  publish_quantity(device.return_temperature_sensor, mbus::Quantity::RETURN_TEMPERATURE);
  publish_quantity(device.temperature_difference_sensor, mbus::Quantity::TEMPERATURE_DIFFERENCE);

  for (const MBusRecordSensor &configured : device.records) {
    const mbus::Record *match = nullptr;
    size_t matches = 0;
    for (const mbus::Record &record : reading.records) {
      if (!record.numeric || record.dif != configured.dif || record.vif != configured.vif ||
          record.function != configured.function)
        continue;
      if (configured.storage >= 0 && record.storage != static_cast<uint64_t>(configured.storage))
        continue;
      if (configured.tariff >= 0 && record.tariff != static_cast<uint64_t>(configured.tariff))
        continue;
      if (configured.subunit >= 0 && record.subunit != static_cast<uint64_t>(configured.subunit))
        continue;
      if (!configured.vife.empty() && configured.vife != record.vife)
        continue;
      match = &record;
      matches++;
    }
    if (matches == 1 && configured.sensor != nullptr) {
      double value;
      if (mbus::canonical_value(*match, value))
        configured.sensor->publish_state(static_cast<float>(value));
    } else if (matches > 1) {
      ESP_LOGD(TAG, "M-Bus serial %s selector DIF=0x%02X VIF=0x%02X is ambiguous", device.serial_number.c_str(),
               static_cast<unsigned>(configured.dif), static_cast<unsigned>(configured.vif));
    }
  }

  ESP_LOGI(TAG, "M-Bus serial=%s slot=%u medium=%u timestamp=%s records=%u", device.serial_number.c_str(),
           static_cast<unsigned>(device.slot), static_cast<unsigned>(reading.medium),
           reading.timestamp.empty() ? "unknown" : reading.timestamp.c_str(),
           static_cast<unsigned>(reading.records.size()));
  if (this->mbus_dump_records_) {
    for (const mbus::Record &record : reading.records) {
      ESP_LOGD(TAG,
               "M-Bus record serial=%s DIF=0x%02X VIF=0x%02X function=%u storage=%lu tariff=%lu subunit=%lu "
               "quantity=%s value=%.6f",
               device.serial_number.c_str(), static_cast<unsigned>(record.dif), static_cast<unsigned>(record.vif),
               static_cast<unsigned>(record.function), static_cast<unsigned long>(record.storage),
               static_cast<unsigned long>(record.tariff), static_cast<unsigned long>(record.subunit),
               mbus::quantity_name(record.quantity), record.value);
    }
  }
}

bool OSGPMeter::all_mbus_cycle_devices_found_() const {
  return !this->mbus_cycle_devices_found_.empty() &&
         std::all_of(this->mbus_cycle_devices_found_.begin(), this->mbus_cycle_devices_found_.end(),
                     [](bool found) { return found; });
}

void OSGPMeter::finish_mbus_cycle_(uint32_t now) {
  this->next_mbus_update_ms_ = now + this->mbus_update_interval_ms_;
  this->session_state_ = SessionState::CONNECTED_IDLE;
}

void OSGPMeter::process_mbus_state_(uint32_t now) {
  auto read_partial = [&](uint16_t table, uint32_t offset, uint16_t count, const char *name) {
    uint8_t payload[8] = {
        protocol::REQUEST_ID_READ_PARTIAL,
        static_cast<uint8_t>((table >> 8) & 0xFF),
        static_cast<uint8_t>(table & 0xFF),
        static_cast<uint8_t>((offset >> 16) & 0xFF),
        static_cast<uint8_t>((offset >> 8) & 0xFF),
        static_cast<uint8_t>(offset & 0xFF),
        static_cast<uint8_t>((count >> 8) & 0xFF),
        static_cast<uint8_t>(count & 0xFF),
    };
    return this->run_request_step_(payload, sizeof(payload), false, name, false);
  };

  switch (this->session_state_) {
    case SessionState::MBUS_PREPARE:
      this->mbus_cycle_devices_found_.assign(this->mbus_devices_.size(), false);
      this->mbus_et14_scan_slot_ = 1;
      this->mbus_et16_scan_slot_ = 1;
      this->session_state_ = this->mbus_dimensions_loaded_ ? SessionState::MBUS_READ_ET14
                                                          : SessionState::MBUS_READ_ET11;
      return;

    case SessionState::MBUS_READ_ET11: {
      const StepResult step = read_partial(0x080B, 0, 7, "ReadET11");
      if (step == StepResult::IN_PROGRESS)
        return;
      std::vector<uint8_t> data;
      if (step == StepResult::FAILURE || !this->consume_partial_table_reply_(data, "ET11") ||
          !this->parse_mbus_et11_(data)) {
        this->finish_mbus_cycle_(now);
        return;
      }
      this->session_state_ = SessionState::MBUS_READ_ET14;
      return;
    }

    case SessionState::MBUS_READ_ET14: {
      const StepResult step = read_partial(0x080E, 0, 6, "ReadET14Header");
      if (step == StepResult::IN_PROGRESS)
        return;
      std::vector<uint8_t> data;
      if (step == StepResult::FAILURE || !this->consume_partial_table_reply_(data, "ET14 header") ||
          !this->parse_mbus_et14_header_(data)) {
        this->finish_mbus_cycle_(now);
        return;
      }
      this->session_state_ = SessionState::MBUS_READ_ET14_ENTRY;
      return;
    }

    case SessionState::MBUS_READ_ET14_ENTRY: {
      while (this->mbus_et14_scan_slot_ <= this->mbus_slots_.size() &&
             !this->mbus_slots_[this->mbus_et14_scan_slot_ - 1].occupied) {
        this->mbus_et14_scan_slot_++;
      }
      if (this->mbus_et14_scan_slot_ <= this->mbus_slots_.size()) {
        const uint8_t slot = this->mbus_et14_scan_slot_;
        const uint32_t offset = 6U + static_cast<uint32_t>(slot - 1) * this->mbus_status_entry_size_;
        const StepResult step = read_partial(0x080E, offset, 6, "ReadET14Entry");
        if (step == StepResult::IN_PROGRESS)
          return;
        if (step == StepResult::SUCCESS) {
          std::vector<uint8_t> data;
          if (this->consume_partial_table_reply_(data, "ET14 entry"))
            this->parse_mbus_et14_entry_(slot, data);
        }
        this->mbus_et14_scan_slot_++;
        return;
      }
      bool need_discovery = false;
      for (const MBusDevice &device : this->mbus_devices_)
        need_discovery |= !device.found;
      this->session_state_ = need_discovery ? SessionState::MBUS_READ_ET16
                                            : (this->mbus_et45_dimensions_loaded_ ? SessionState::MBUS_READ_ET45_HEADER
                                                                                 : SessionState::MBUS_READ_ET36_COUNT);
      return;
    }

    case SessionState::MBUS_READ_ET16: {
      while (this->mbus_et16_scan_slot_ <= 4) {
        const MBusSlot &slot = this->mbus_slots_[this->mbus_et16_scan_slot_ - 1];
        if (slot.occupied && slot.billing_read_length >= 8)
          break;
        this->mbus_et16_scan_slot_++;
      }
      if (this->mbus_et16_scan_slot_ > 4) {
        this->session_state_ = this->mbus_et45_dimensions_loaded_ ? SessionState::MBUS_READ_ET45_HEADER
                                                                 : SessionState::MBUS_READ_ET36_COUNT;
        return;
      }
      const uint8_t slot_number = this->mbus_et16_scan_slot_;
      const MBusSlot &slot = this->mbus_slots_[slot_number - 1];
      const uint16_t count = std::min(slot.billing_read_length, this->mbus_data_entry_size_);
      const uint32_t offset = static_cast<uint32_t>(slot_number - 1) * this->mbus_data_entry_size_;
      const StepResult step = read_partial(0x0810, offset, count, "ReadET16Entry");
      if (step == StepResult::IN_PROGRESS)
        return;
      if (step == StepResult::SUCCESS) {
        std::vector<uint8_t> data;
        if (this->consume_partial_table_reply_(data, "ET16"))
          this->parse_mbus_et16_entry_(slot_number, data);
      }
      this->mbus_et16_scan_slot_++;
      return;
    }

    case SessionState::MBUS_READ_ET36_COUNT: {
      const StepResult step = read_partial(0x0824, 0, 1, "ReadET36Count");
      if (step == StepResult::IN_PROGRESS)
        return;
      std::vector<uint8_t> data;
      if (step == StepResult::FAILURE || !this->consume_partial_table_reply_(data, "ET36Count") || data.empty()) {
        this->finish_mbus_cycle_(now);
        return;
      }
      this->mbus_et36_count_ = data[0];
      this->session_state_ = SessionState::MBUS_READ_ET36;
      return;
    }

    case SessionState::MBUS_READ_ET36: {
      constexpr uint8_t et45_dimension_index = 4;
      if (this->mbus_et36_count_ <= et45_dimension_index) {
        this->finish_mbus_cycle_(now);
        return;
      }
      const uint32_t offset = 1U + static_cast<uint32_t>(et45_dimension_index) * 17U;
      const StepResult step = read_partial(0x0824, offset, 17, "ReadET36Entry");
      if (step == StepResult::IN_PROGRESS)
        return;
      std::vector<uint8_t> data;
      if (step == StepResult::FAILURE || !this->consume_partial_table_reply_(data, "ET36") ||
          !this->parse_mbus_et36_(data)) {
        this->finish_mbus_cycle_(now);
        return;
      }
      this->session_state_ = SessionState::MBUS_READ_ET45_HEADER;
      return;
    }

    case SessionState::MBUS_READ_ET45_HEADER: {
      if (!this->mbus_et45_dimensions_loaded_ || this->mbus_et45_current_entries_ == 0) {
        this->finish_mbus_cycle_(now);
        return;
      }
      const StepResult step = read_partial(0x082D, 0, 6, "ReadET45Header");
      if (step == StepResult::IN_PROGRESS)
        return;
      std::vector<uint8_t> data;
      if (step == StepResult::FAILURE || !this->consume_partial_table_reply_(data, "ET45Header") ||
          !this->parse_mbus_et45_header_(data)) {
        this->finish_mbus_cycle_(now);
        return;
      }
      if (this->mbus_et45_scan_remaining_ == 0) {
        this->mbus_et45_sequence_ = this->mbus_et45_pending_sequence_;
        this->mbus_et45_sequence_valid_ = true;
        this->finish_mbus_cycle_(now);
      } else {
        this->session_state_ = SessionState::MBUS_READ_ET45_ENTRY;
      }
      return;
    }

    case SessionState::MBUS_READ_ET45_ENTRY: {
      const uint32_t offset = 6U + static_cast<uint32_t>(this->mbus_et45_scan_index_) * this->mbus_et45_entry_size_;
      const StepResult step = read_partial(0x082D, offset, 8, "ReadET45EntryHeader");
      if (step == StepResult::IN_PROGRESS)
        return;
      bool read_entry = false;
      if (step == StepResult::SUCCESS) {
        std::vector<uint8_t> data;
        if (this->consume_partial_table_reply_(data, "ET45 entry header") && data.size() >= 8) {
          const uint16_t entry_length = read_u16(data.data(), this->byte_order_little_);
          const uint16_t handle = read_u16(data.data() + 4, this->byte_order_little_);
          const bool configured_handle = std::any_of(
              this->mbus_devices_.begin(), this->mbus_devices_.end(),
              [handle](const MBusDevice &device) { return handle != 0 && device.handle == handle; });
          if (entry_length >= 6 && static_cast<uint32_t>(entry_length) + 2U <= this->mbus_et45_entry_size_ &&
              data[7] == 4 && configured_handle) {
            this->mbus_et45_pending_offset_ = offset;
            this->mbus_et45_pending_length_ = entry_length + 2U;
            this->session_state_ = SessionState::MBUS_READ_ET45_ENTRY_DATA;
            read_entry = true;
          }
        }
      }
      if (read_entry)
        return;
      if (this->mbus_et45_scan_remaining_ > 0)
        this->mbus_et45_scan_remaining_--;
      if (this->mbus_et45_scan_remaining_ == 0 || this->all_mbus_cycle_devices_found_()) {
        this->mbus_et45_sequence_ = this->mbus_et45_pending_sequence_;
        this->mbus_et45_sequence_valid_ = true;
        this->finish_mbus_cycle_(now);
        return;
      }
      this->mbus_et45_scan_index_ =
          mbus::previous_ring_index(this->mbus_et45_scan_index_, this->mbus_et45_current_entries_);
      return;
    }

    case SessionState::MBUS_READ_ET45_ENTRY_DATA: {
      const StepResult step = read_partial(0x082D, this->mbus_et45_pending_offset_, this->mbus_et45_pending_length_,
                                           "ReadET45EntryData");
      if (step == StepResult::IN_PROGRESS)
        return;
      if (step == StepResult::SUCCESS) {
        std::vector<uint8_t> data;
        if (this->consume_partial_table_reply_(data, "ET45 entry data"))
          this->parse_mbus_et45_entry_(data);
      }
      if (this->mbus_et45_scan_remaining_ > 0)
        this->mbus_et45_scan_remaining_--;
      if (this->mbus_et45_scan_remaining_ == 0 || this->all_mbus_cycle_devices_found_()) {
        this->mbus_et45_sequence_ = this->mbus_et45_pending_sequence_;
        this->mbus_et45_sequence_valid_ = true;
        this->finish_mbus_cycle_(now);
        return;
      }
      this->mbus_et45_scan_index_ =
          mbus::previous_ring_index(this->mbus_et45_scan_index_, this->mbus_et45_current_entries_);
      this->session_state_ = SessionState::MBUS_READ_ET45_ENTRY;
      return;
    }

    default:
      this->finish_mbus_cycle_(now);
      return;
  }
}

}  // namespace osgp_meter
}  // namespace esphome
