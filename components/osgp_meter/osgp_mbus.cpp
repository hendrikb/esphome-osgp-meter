#include "osgp_mbus.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace esphome {
namespace osgp_meter {
namespace mbus {

namespace {

void set_error(std::string *error, const char *message) {
  if (error != nullptr) {
    *error = message;
  }
}

bool valid_timestamp(const uint8_t *data) {
  return data[0] <= 99 && data[1] >= 1 && data[1] <= 12 && data[2] >= 1 && data[2] <= 31 && data[3] <= 23 &&
         data[4] <= 59 && data[5] <= 59;
}

std::string format_timestamp(const uint8_t *data) {
  if (!valid_timestamp(data)) {
    return {};
  }
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "20%02u-%02u-%02uT%02u:%02u:%02u", static_cast<unsigned>(data[0]),
                static_cast<unsigned>(data[1]), static_cast<unsigned>(data[2]), static_cast<unsigned>(data[3]),
                static_cast<unsigned>(data[4]), static_cast<unsigned>(data[5]));
  return buffer;
}

bool decode_bcd_serial(const uint8_t *data, std::string &serial) {
  serial.clear();
  serial.reserve(8);
  for (int i = 3; i >= 0; i--) {
    const uint8_t high = (data[i] >> 4) & 0x0F;
    const uint8_t low = data[i] & 0x0F;
    if (high > 9 || low > 9) {
      serial.clear();
      return false;
    }
    serial.push_back(static_cast<char>('0' + high));
    serial.push_back(static_cast<char>('0' + low));
  }
  return true;
}

uint64_t read_unsigned(const uint8_t *data, size_t length, bool little) {
  uint64_t value = 0;
  if (little) {
    for (size_t i = 0; i < length; i++) {
      value |= static_cast<uint64_t>(data[i]) << (8U * i);
    }
  } else {
    for (size_t i = 0; i < length; i++) {
      value = (value << 8U) | data[i];
    }
  }
  return value;
}

int64_t sign_extend(uint64_t value, size_t bytes) {
  if (bytes == 0 || bytes >= sizeof(uint64_t)) {
    return static_cast<int64_t>(value);
  }
  const uint64_t sign = uint64_t{1} << (bytes * 8U - 1U);
  if ((value & sign) != 0) {
    value |= (~uint64_t{0}) << (bytes * 8U);
  }
  return static_cast<int64_t>(value);
}

bool decode_bcd(const uint8_t *data, size_t length, bool little, double &value) {
  uint64_t result = 0;
  if (little) {
    for (size_t i = length; i > 0; i--) {
      const uint8_t byte = data[i - 1];
      const uint8_t high = (byte >> 4) & 0x0F;
      const uint8_t low = byte & 0x0F;
      if (high > 9 || low > 9) {
        return false;
      }
      result = result * 100U + high * 10U + low;
    }
  } else {
    for (size_t i = 0; i < length; i++) {
      const uint8_t high = (data[i] >> 4) & 0x0F;
      const uint8_t low = data[i] & 0x0F;
      if (high > 9 || low > 9) {
        return false;
      }
      result = result * 100U + high * 10U + low;
    }
  }
  value = static_cast<double>(result);
  return true;
}

bool decode_numeric(uint8_t data_field, const uint8_t *data, size_t length, bool little, double &value) {
  if (data_field >= 1 && data_field <= 4) {
    value = static_cast<double>(sign_extend(read_unsigned(data, length, little), length));
    return true;
  }
  if (data_field == 5 && length == 4) {
    uint8_t ordered[4];
    if (little) {
      std::memcpy(ordered, data, sizeof(ordered));
    } else {
      std::reverse_copy(data, data + 4, ordered);
    }
    float float_value;
    std::memcpy(&float_value, ordered, sizeof(float_value));
    if (!std::isfinite(float_value)) {
      return false;
    }
    value = float_value;
    return true;
  }
  if (data_field == 6 || data_field == 7) {
    value = static_cast<double>(sign_extend(read_unsigned(data, length, little), length));
    return true;
  }
  if (data_field >= 9 && data_field <= 12) {
    return decode_bcd(data, length, little, value);
  }
  if (data_field == 14) {
    return decode_bcd(data, length, little, value);
  }
  return false;
}

size_t fixed_data_length(uint8_t data_field) {
  static constexpr uint8_t LENGTHS[16] = {0, 1, 2, 3, 4, 4, 6, 8, 0, 1, 2, 3, 4, 0, 6, 0};
  return LENGTHS[data_field & 0x0F];
}

void decode_vif(uint8_t vif, Quantity &quantity, double &scale) {
  const uint8_t base = vif & 0x7F;
  quantity = Quantity::UNKNOWN;
  scale = 1.0;
  if (base <= 0x07) {
    quantity = Quantity::ENERGY_WH;
    scale = std::pow(10.0, static_cast<int>(base) - 3);
  } else if (base <= 0x0F) {
    quantity = Quantity::ENERGY_J;
    scale = std::pow(10.0, static_cast<int>(base) - 8);
  } else if (base <= 0x17) {
    quantity = Quantity::VOLUME;
    scale = std::pow(10.0, static_cast<int>(base) - 0x16);
  } else if (base <= 0x1F) {
    quantity = Quantity::MASS;
    scale = std::pow(10.0, static_cast<int>(base) - 0x1B);
  } else if (base <= 0x27) {
    quantity = Quantity::DURATION;
    static constexpr double TIME_SCALE[4] = {1.0, 60.0, 3600.0, 86400.0};
    scale = TIME_SCALE[base & 0x03];
  } else if (base <= 0x2F) {
    quantity = Quantity::POWER_W;
    scale = std::pow(10.0, static_cast<int>(base) - 0x2B);
  } else if (base <= 0x37) {
    quantity = Quantity::POWER_J_PER_HOUR;
    scale = std::pow(10.0, static_cast<int>(base) - 0x30);
  } else if (base <= 0x3F) {
    quantity = Quantity::VOLUME_FLOW_HOUR;
    scale = std::pow(10.0, static_cast<int>(base) - 0x3E);
  } else if (base <= 0x47) {
    quantity = Quantity::VOLUME_FLOW_MINUTE;
    scale = std::pow(10.0, static_cast<int>(base) - 0x47);
  } else if (base <= 0x4F) {
    quantity = Quantity::VOLUME_FLOW_SECOND;
    scale = std::pow(10.0, static_cast<int>(base) - 0x4F);
  } else if (base <= 0x57) {
    quantity = Quantity::MASS_FLOW;
    scale = std::pow(10.0, static_cast<int>(base) - 0x53);
  } else if (base <= 0x5B) {
    quantity = Quantity::FLOW_TEMPERATURE;
    scale = std::pow(10.0, static_cast<int>(base) - 0x5B);
  } else if (base <= 0x5F) {
    quantity = Quantity::RETURN_TEMPERATURE;
    scale = std::pow(10.0, static_cast<int>(base) - 0x5F);
  } else if (base <= 0x63) {
    quantity = Quantity::TEMPERATURE_DIFFERENCE;
    scale = std::pow(10.0, static_cast<int>(base) - 0x63);
  } else if (base <= 0x67) {
    quantity = Quantity::EXTERNAL_TEMPERATURE;
    scale = std::pow(10.0, static_cast<int>(base) - 0x67);
  } else if (base <= 0x6B) {
    quantity = Quantity::PRESSURE;
    scale = std::pow(10.0, static_cast<int>(base) - 0x6B);
  }
}

bool decode_fixed_unit(uint8_t unit, Quantity &quantity, double &scale) {
  quantity = Quantity::UNKNOWN;
  scale = 1.0;
  if (unit >= 0x02 && unit <= 0x0A) {
    quantity = Quantity::ENERGY_WH;
    scale = std::pow(10.0, static_cast<int>(unit) - 0x02);
  } else if (unit >= 0x0B && unit <= 0x13) {
    quantity = Quantity::ENERGY_J;
    scale = std::pow(10.0, static_cast<int>(unit) - 0x08);
  } else if (unit >= 0x14 && unit <= 0x1C) {
    quantity = Quantity::POWER_W;
    scale = std::pow(10.0, static_cast<int>(unit) - 0x14);
  } else if (unit >= 0x1D && unit <= 0x25) {
    quantity = Quantity::POWER_J_PER_HOUR;
    scale = std::pow(10.0, static_cast<int>(unit) - 0x1A);
  } else if (unit >= 0x26 && unit <= 0x2E) {
    quantity = Quantity::VOLUME;
    scale = std::pow(10.0, static_cast<int>(unit) - 0x2C);
  } else if (unit >= 0x2F && unit <= 0x37) {
    quantity = Quantity::VOLUME_FLOW_HOUR;
    scale = std::pow(10.0, static_cast<int>(unit) - 0x35);
  } else if (unit == 0x38) {
    quantity = Quantity::EXTERNAL_TEMPERATURE;
    scale = 0.001;
  }
  return quantity != Quantity::UNKNOWN;
}

uint8_t decode_fixed_medium(uint8_t first, uint8_t second) {
  uint8_t medium = static_cast<uint8_t>(((first >> 6) & 0x03) | (((second >> 6) & 0x03) << 2));
  switch (medium) {
    case 0x0A:
      return static_cast<uint8_t>(Medium::GAS);
    case 0x0B:
      return static_cast<uint8_t>(Medium::HEAT);
    case 0x0C:
      return static_cast<uint8_t>(Medium::HOT_WATER);
    case 0x0D:
      return static_cast<uint8_t>(Medium::WATER);
    default:
      return medium;
  }
}

bool parse_variable_records(const uint8_t *data, size_t length, bool little, std::vector<Record> &records,
                            std::string *error) {
  size_t pos = 0;
  while (pos < length) {
    const uint8_t dif = data[pos++];
    if (dif == 0x00 || dif == 0x0F || dif == 0x1F) {
      return true;
    }
    if (dif == 0x2F) {
      continue;
    }

    Record record;
    record.dif = dif;
    record.function = (dif >> 4) & 0x03;
    record.storage = (dif >> 6) & 0x01;

    uint8_t extension = dif;
    uint8_t extension_index = 0;
    while ((extension & 0x80) != 0) {
      if (pos >= length || extension_index >= 10) {
        set_error(error, "truncated or excessive DIFE chain");
        return false;
      }
      extension = data[pos++];
      record.storage |= static_cast<uint32_t>(extension & 0x0F) << (1U + 4U * extension_index);
      record.tariff |= static_cast<uint32_t>((extension >> 4) & 0x03) << (2U * extension_index);
      record.subunit |= static_cast<uint32_t>((extension >> 6) & 0x01) << extension_index;
      extension_index++;
    }

    if (pos >= length) {
      set_error(error, "record missing VIF");
      return false;
    }
    record.vif = data[pos++];
    extension = record.vif;
    while ((extension & 0x80) != 0) {
      if (pos >= length || record.vife.size() >= 10) {
        set_error(error, "truncated or excessive VIFE chain");
        return false;
      }
      extension = data[pos++];
      record.vife.push_back(extension);
    }

    const uint8_t data_field = dif & 0x0F;
    size_t value_length = fixed_data_length(data_field);
    bool variable_numeric = false;
    bool variable_bcd = false;
    if (data_field == 13) {
      if (pos >= length) {
        set_error(error, "variable record missing LVAR");
        return false;
      }
      const uint8_t lvar = data[pos++];
      if (lvar <= 0xBF) {
        value_length = lvar;
      } else {
        value_length = lvar & 0x0F;
        variable_bcd = (lvar & 0xE0) == 0xC0 || (lvar & 0xF0) == 0xD0;
        variable_numeric = variable_bcd || (lvar & 0xF0) == 0xE0;
      }
    }
    if (pos + value_length > length) {
      set_error(error, "record value exceeds telegram");
      return false;
    }

    double raw_value = 0.0;
    bool numeric = decode_numeric(data_field, data + pos, value_length, little, raw_value);
    if (data_field == 13 && variable_numeric) {
      numeric = variable_bcd ? decode_bcd(data + pos, value_length, little, raw_value) : true;
      if (!variable_bcd) {
        raw_value = static_cast<double>(sign_extend(read_unsigned(data + pos, value_length, little), value_length));
      }
    }
    pos += value_length;

    double scale = 1.0;
    decode_vif(record.vif, record.quantity, scale);
    record.numeric = numeric && record.quantity != Quantity::UNKNOWN;
    if (record.numeric) {
      record.value = raw_value * scale;
    }
    records.push_back(std::move(record));
  }
  return true;
}

bool parse_variable(const uint8_t *data, size_t length, bool little, Reading &reading, std::string *error) {
  size_t pos = 0;
  bool found_telegram = false;
  while (pos < length) {
    const uint8_t telegram_length = data[pos++];
    if (telegram_length == 0) {
      break;
    }
    if (telegram_length > length - pos) {
      set_error(error, "M-Bus telegram length exceeds device read");
      return false;
    }
    if (telegram_length < 13) {
      set_error(error, "M-Bus variable telegram is too short");
      return false;
    }
    const uint8_t *telegram = data + pos;
    std::string serial;
    if (!decode_bcd_serial(telegram, serial)) {
      set_error(error, "invalid packed-BCD M-Bus serial");
      return false;
    }
    if (!found_telegram) {
      reading.serial_number = serial;
      reading.medium = telegram[7];
      reading.access_number = telegram[8];
      reading.status = telegram[9];
    } else if (serial != reading.serial_number) {
      set_error(error, "M-Bus telegrams contain different serial numbers");
      return false;
    }

    size_t records_length = telegram_length - 12;
    if (records_length > 0 && telegram[telegram_length - 1] == 0x00) {
      records_length--;
    }
    if (!parse_variable_records(telegram + 12, records_length, little, reading.records, error)) {
      return false;
    }
    found_telegram = true;
    pos += telegram_length;
  }
  if (!found_telegram) {
    set_error(error, "device read contains no M-Bus telegram");
  }
  return found_telegram;
}

bool parse_fixed(const uint8_t *data, size_t length, bool little, Reading &reading, std::string *error) {
  if (length < 17 || data[0] != 16) {
    set_error(error, "M-Bus fixed telegram is malformed");
    return false;
  }
  const uint8_t *telegram = data + 1;
  if (!decode_bcd_serial(telegram, reading.serial_number)) {
    set_error(error, "invalid packed-BCD M-Bus serial");
    return false;
  }
  reading.access_number = telegram[4];
  reading.status = telegram[5];
  reading.medium = decode_fixed_medium(telegram[6], telegram[7]);

  uint8_t units[2] = {static_cast<uint8_t>(telegram[6] & 0x3F), static_cast<uint8_t>(telegram[7] & 0x3F)};
  if (units[0] == 0x3E)
    units[0] = units[1];
  if (units[1] == 0x3E)
    units[1] = units[0];

  for (size_t index = 0; index < 2; index++) {
    Record record;
    record.dif = 0x04;
    record.vif = units[index];
    record.function = 0;
    record.storage = (reading.status & 0x02) != 0 ? 1 : 0;
    double raw_value = 0.0;
    if ((reading.status & 0x01) != 0) {
      raw_value = static_cast<double>(sign_extend(read_unsigned(telegram + 8 + index * 4, 4, little), 4));
    } else if (!decode_bcd(telegram + 8 + index * 4, 4, little, raw_value)) {
      set_error(error, "invalid fixed-data BCD counter");
      return false;
    }
    double scale = 1.0;
    record.numeric = decode_fixed_unit(units[index], record.quantity, scale);
    record.value = raw_value * scale;
    reading.records.push_back(record);
  }
  return true;
}

}  // namespace

bool parse_reading(const uint8_t *data, size_t length, Reading &reading, std::string *error) {
  reading = Reading{};
  if (data == nullptr || length < 8) {
    set_error(error, "M-Bus device read is too short");
    return false;
  }
  reading.timestamp = format_timestamp(data);
  const uint8_t response_info = data[6];
  const bool little = (response_info & 0x01) == 0;
  reading.fixed_data = (response_info & 0x02) != 0;
  const bool key_available = (response_info & 0x04) != 0;
  const uint8_t authentication = (response_info >> 3) & 0x03;
  const bool security_failed = (response_info & 0x20) != 0;
  reading.security_ok = !security_failed && authentication != 1 && authentication != 2;
  if (!reading.security_ok) {
    set_error(error, "M-Bus device read failed authentication or decryption");
    return false;
  }
  (void) key_available;
  if (reading.fixed_data) {
    return parse_fixed(data + 7, length - 7, little, reading, error);
  }
  return parse_variable(data + 7, length - 7, little, reading, error);
}

bool parse_device_configuration(const uint8_t *data, size_t length, bool little, DeviceConfiguration &configuration,
                                std::string *error) {
  configuration = DeviceConfiguration{};
  if (data == nullptr || length < 6) {
    set_error(error, "ET13 device configuration is too short");
    return false;
  }
  configuration.scheduled_day = data[0];
  configuration.scheduled_hour = data[1];
  configuration.scheduled_minute = data[2];
  configuration.scheduled_frequency = data[3];
  configuration.status_interval_minutes = static_cast<uint16_t>(read_unsigned(data + 4, 2, little));
  if (configuration.scheduled_hour > 23 || configuration.scheduled_minute > 59 ||
      configuration.scheduled_frequency > 4) {
    set_error(error, "ET13 device schedule contains an invalid time or frequency");
    return false;
  }
  if (configuration.scheduled_frequency == 2 && configuration.scheduled_day != 39 &&
      (configuration.scheduled_day < 32 || configuration.scheduled_day > 38)) {
    set_error(error, "ET13 weekly schedule contains an invalid day");
    return false;
  }
  if (configuration.scheduled_frequency == 3 && configuration.scheduled_day > 28) {
    set_error(error, "ET13 monthly schedule contains an invalid day");
    return false;
  }
  return true;
}

bool parse_load_profile_poll_rate(const uint8_t *data, size_t length, bool little, uint16_t &poll_rate_minutes,
                                  std::string *error) {
  poll_rate_minutes = 0;
  if (data == nullptr || length < 2) {
    set_error(error, "ET34 load-profile poll rate is too short");
    return false;
  }
  poll_rate_minutes = static_cast<uint16_t>(read_unsigned(data, 2, little));
  return true;
}

bool parse_primary_load_profile_layout(const uint8_t *data, size_t length, bool little,
                                       PrimaryLoadProfileLayout &layout, std::string *error) {
  layout = PrimaryLoadProfileLayout{};
  if (data == nullptr || length < 31) {
    set_error(error, "ET42 fixed section is too short");
    return false;
  }
  layout.table_length = static_cast<uint16_t>(read_unsigned(data, 2, little));
  const uint8_t fixed_section_length = data[2];
  const uint8_t log_list_size = data[3];
  const uint8_t demand_sources = data[13];
  const uint8_t coincident_sources = data[14];
  layout.channel_count = data[29];
  layout.interval_minutes = data[30];

  const uint32_t source_offset = static_cast<uint32_t>(fixed_section_length) + 3U * log_list_size + demand_sources +
                                 coincident_sources;
  const uint32_t source_end = source_offset + 2U * layout.channel_count;
  if (layout.table_length < length || source_offset > std::numeric_limits<uint16_t>::max() ||
      source_end > layout.table_length || layout.channel_count > 64) {
    set_error(error, "ET42 primary load-profile source dimensions are invalid");
    return false;
  }
  layout.source_offset = static_cast<uint16_t>(source_offset);
  return true;
}

bool find_primary_load_profile_channels(const uint8_t *data, size_t length, bool little, uint8_t slot,
                                        std::vector<PrimaryLoadProfileChannel> &channels, std::string *error) {
  channels.clear();
  if (slot < 1 || slot > 4) {
    set_error(error, "M-Bus slot is outside the supported range");
    return false;
  }
  if (data == nullptr || (length % 2) != 0) {
    set_error(error, "ET42 primary load-profile source list is malformed");
    return false;
  }
  const uint8_t device_index = slot - 1;
  for (size_t pos = 0; pos < length; pos += 2) {
    const uint16_t source = static_cast<uint16_t>(read_unsigned(data + pos, 2, little));
    if ((source >> 12) != 4 || ((source >> 8) & 0x0F) != device_index)
      continue;
    channels.push_back(
        {static_cast<uint8_t>(pos / 2U), static_cast<uint8_t>(source & 0x1F)});
  }
  return true;
}

std::string format_scheduled_read(const DeviceConfiguration &configuration) {
  char buffer[64];
  switch (configuration.scheduled_frequency) {
    case 0:
      std::snprintf(buffer, sizeof(buffer), "hourly at minute %02u",
                    static_cast<unsigned>(configuration.scheduled_minute));
      return buffer;
    case 1:
      std::snprintf(buffer, sizeof(buffer), "daily at %02u:%02u", static_cast<unsigned>(configuration.scheduled_hour),
                    static_cast<unsigned>(configuration.scheduled_minute));
      return buffer;
    case 2: {
      if (configuration.scheduled_day == 39)
        return "never";
      static constexpr const char *DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
                                              "Saturday"};
      const size_t day_index = configuration.scheduled_day - 32U;
      std::snprintf(buffer, sizeof(buffer), "weekly on %s at %02u:%02u", DAYS[day_index],
                    static_cast<unsigned>(configuration.scheduled_hour),
                    static_cast<unsigned>(configuration.scheduled_minute));
      return buffer;
    }
    case 3:
      std::snprintf(buffer, sizeof(buffer), "monthly on day %u at %02u:%02u",
                    static_cast<unsigned>(configuration.scheduled_day == 0 ? 1 : configuration.scheduled_day),
                    static_cast<unsigned>(configuration.scheduled_hour),
                    static_cast<unsigned>(configuration.scheduled_minute));
      return buffer;
    case 4:
    default:
      return "never";
  }
}

std::string format_status_reads(const DeviceConfiguration &configuration) {
  if (configuration.status_interval_minutes == 0)
    return "with billing reads";
  char buffer[48];
  std::snprintf(buffer, sizeof(buffer), "every %u min",
                static_cast<unsigned>(configuration.status_interval_minutes));
  return buffer;
}

std::string format_primary_load_profile(uint8_t interval_minutes, uint16_t poll_rate_minutes,
                                        const std::vector<PrimaryLoadProfileChannel> &channels) {
  std::string result;
  if (channels.empty()) {
    result = "no M-Bus channels";
  } else {
    result = "channels=";
    for (size_t i = 0; i < channels.size(); i++) {
      if (i != 0)
        result += ',';
      result += std::to_string(channels[i].channel) + ":MDT" + std::to_string(channels[i].mdt);
    }
  }
  if (interval_minutes == 84) {
    result += "; interval=24 h";
  } else if (interval_minutes == 0) {
    result += "; interval=disabled";
  } else {
    result += "; interval=" + std::to_string(interval_minutes) + " min";
  }
  if (poll_rate_minutes == 0) {
    result += "; poll=every interval";
  } else {
    result += "; poll=" + std::to_string(poll_rate_minutes) + " min";
  }
  return result;
}

bool replace_diagnostic_summary_if_changed(std::string &previous, const std::string &current) {
  if (previous == current)
    return false;
  previous = current;
  return true;
}

const char *quantity_name(Quantity quantity) {
  switch (quantity) {
    case Quantity::ENERGY_WH:
      return "energy_wh";
    case Quantity::ENERGY_J:
      return "energy_j";
    case Quantity::VOLUME:
      return "volume";
    case Quantity::MASS:
      return "mass";
    case Quantity::DURATION:
      return "duration";
    case Quantity::POWER_W:
      return "power_w";
    case Quantity::POWER_J_PER_HOUR:
      return "power_j_per_hour";
    case Quantity::VOLUME_FLOW_HOUR:
      return "volume_flow_hour";
    case Quantity::VOLUME_FLOW_MINUTE:
      return "volume_flow_minute";
    case Quantity::VOLUME_FLOW_SECOND:
      return "volume_flow_second";
    case Quantity::MASS_FLOW:
      return "mass_flow";
    case Quantity::FLOW_TEMPERATURE:
      return "flow_temperature";
    case Quantity::RETURN_TEMPERATURE:
      return "return_temperature";
    case Quantity::TEMPERATURE_DIFFERENCE:
      return "temperature_difference";
    case Quantity::EXTERNAL_TEMPERATURE:
      return "external_temperature";
    case Quantity::PRESSURE:
      return "pressure";
    case Quantity::UNKNOWN:
    default:
      return "unknown";
  }
}

bool normalized_value(const Record &record, Quantity wanted, double &value) {
  if (!record.numeric) {
    return false;
  }
  if (wanted == Quantity::ENERGY_WH) {
    if (record.quantity == Quantity::ENERGY_WH) {
      value = record.value / 1000.0;
      return true;
    }
    if (record.quantity == Quantity::ENERGY_J) {
      value = record.value / 3600000.0;
      return true;
    }
  }
  if (wanted == Quantity::POWER_W) {
    if (record.quantity == Quantity::POWER_W) {
      value = record.value;
      return true;
    }
    if (record.quantity == Quantity::POWER_J_PER_HOUR) {
      value = record.value / 3600.0;
      return true;
    }
  }
  if (wanted == Quantity::VOLUME_FLOW_HOUR) {
    if (record.quantity == Quantity::VOLUME_FLOW_HOUR) {
      value = record.value;
      return true;
    }
    if (record.quantity == Quantity::VOLUME_FLOW_MINUTE) {
      value = record.value * 60.0;
      return true;
    }
    if (record.quantity == Quantity::VOLUME_FLOW_SECOND) {
      value = record.value * 3600.0;
      return true;
    }
  }
  if (record.quantity == wanted) {
    value = record.value;
    return true;
  }
  return false;
}

bool canonical_value(const Record &record, double &value) {
  if (!record.numeric) {
    return false;
  }
  if (record.quantity == Quantity::POWER_J_PER_HOUR) {
    return normalized_value(record, Quantity::POWER_W, value);
  }
  if (record.quantity == Quantity::VOLUME_FLOW_MINUTE || record.quantity == Quantity::VOLUME_FLOW_SECOND) {
    return normalized_value(record, Quantity::VOLUME_FLOW_HOUR, value);
  }
  value = record.value;
  return true;
}

bool medium_matches(Medium configured, uint8_t received) {
  if (configured == Medium::AUTO || received == 0 || static_cast<uint8_t>(configured) == received) {
    return true;
  }
  return configured == Medium::WATER &&
         (received == static_cast<uint8_t>(Medium::HOT_WATER) || received == static_cast<uint8_t>(Medium::COLD_WATER));
}

bool valid_slot_hint(uint8_t slot) { return slot <= 4; }

uint16_t previous_ring_index(uint16_t index, uint16_t count) {
  if (count == 0) {
    return 0;
  }
  return static_cast<uint16_t>((index + count - 1U) % count);
}

}  // namespace mbus
}  // namespace osgp_meter
}  // namespace esphome
