#include "osgp_utils.h"

#include <cstdio>

#include "osgp_protocol_constants.h"

namespace esphome {
namespace osgp_meter {
namespace utils {

std::string bytes_to_hex(const uint8_t *data, size_t len) {
  std::string out;
  out.reserve(len * 3);
  char buf[4];
  for (size_t i = 0; i < len; i++) {
    snprintf(buf, sizeof(buf), "%02X ", data[i]);
    out.append(buf);
  }
  return out;
}

std::string trim_right(const std::string &value) {
  if (value.empty()) {
    return value;
  }
  size_t end = value.size();
  while (end > 0) {
    char c = value[end - 1];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\0') {
      break;
    }
    end--;
  }
  return value.substr(0, end);
}

std::string bcd_to_string(const std::vector<uint8_t> &data) {
  std::string out;
  out.reserve(data.size() * 2);
  for (uint8_t byte : data) {
    uint8_t high = (byte >> 4) & 0x0F;
    uint8_t low = byte & 0x0F;
    if (high <= 9) {
      out.push_back(static_cast<char>('0' + high));
    }
    if (low <= 9) {
      out.push_back(static_cast<char>('0' + low));
    }
  }
  return out;
}

std::string firmware_version_string(uint8_t major_minor, uint8_t build) {
  const uint16_t value = (static_cast<uint16_t>(major_minor) << 8) | build;
  const uint8_t major = (value >> 12) & 0x0F;
  const uint8_t minor = (value >> 5) & 0x7F;
  const uint8_t patch = value & 0x1F;
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%u.%02u.%02u", major, minor, patch);
  return buffer;
}

const char *unknown_context_label(uint8_t context) {
  switch (context) {
    case protocol::UNKNOWN_PRE_SEND:
      return "pre-send";
    case protocol::UNKNOWN_START_SCAN:
      return "start-scan";
    case protocol::UNKNOWN_RESPONSE:
      return "response";
    default:
      return "unknown";
  }
}

std::string ctrl_flags_label(uint8_t ctrl) {
  std::string flags;
  if ((ctrl & 0x80) != 0) {
    flags = "multipacket";
  }
  if ((ctrl & 0x40) != 0) {
    if (!flags.empty()) {
      flags.append(",");
    }
    flags.append("flag_0x40");
  }
  if ((ctrl & 0x20) != 0) {
    if (!flags.empty()) {
      flags.append(",");
    }
    flags.append("toggle");
  }
  uint8_t unknown = ctrl & static_cast<uint8_t>(~0xE0);
  if (unknown != 0) {
    if (!flags.empty()) {
      flags.append(",");
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "other=0x%02X", unknown);
    flags.append(buf);
  }
  if (flags.empty()) {
    flags = "none";
  }
  return flags;
}

}  // namespace utils
}  // namespace osgp_meter
}  // namespace esphome
