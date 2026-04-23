#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace esphome {
namespace osgp_meter {
namespace utils {

std::string bytes_to_hex(const uint8_t *data, size_t len);
std::string trim_right(const std::string &value);
std::string bcd_to_string(const std::vector<uint8_t> &data);
std::string firmware_version_string(uint8_t major_minor, uint8_t build);
const char *unknown_context_label(uint8_t context);
std::string ctrl_flags_label(uint8_t ctrl);

template<typename StatePublisher, typename ValueT>
void publish_state_if_present(StatePublisher *publisher, ValueT &&value) {
  if (publisher != nullptr) {
    publisher->publish_state(std::forward<ValueT>(value));
  }
}

}  // namespace utils
}  // namespace osgp_meter
}  // namespace esphome
