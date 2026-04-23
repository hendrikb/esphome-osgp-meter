#include "osgp_meter.h"

#include "osgp_protocol_constants.h"
#include "osgp_utils.h"
#include "esphome/core/log.h"

namespace esphome {
namespace osgp_meter {

static const char *const TAG = "osgp_meter";

void OSGPMeter::record_unknown_signal_(uint8_t value, const char *context) {
  this->unknown_signal_count_++;
  utils::publish_state_if_present(this->unknown_signal_count_sensor_, static_cast<float>(this->unknown_signal_count_));
  ESP_LOGV(TAG, "Unknown signal (%s): 0x%02X (count=%u)", context, value,
           static_cast<unsigned>(this->unknown_signal_count_));
}

void OSGPMeter::record_unknown_signal_(uint8_t value, uint8_t context) {
  this->unknown_signal_count_++;
  utils::publish_state_if_present(this->unknown_signal_count_sensor_, static_cast<float>(this->unknown_signal_count_));

  switch (context) {
    case protocol::UNKNOWN_PRE_SEND:
      this->unknown_pre_send_count_++;
      utils::publish_state_if_present(this->unknown_pre_send_count_sensor_,
                                      static_cast<float>(this->unknown_pre_send_count_));
      break;
    case protocol::UNKNOWN_START_SCAN:
      this->unknown_start_scan_count_++;
      utils::publish_state_if_present(this->unknown_start_scan_count_sensor_,
                                      static_cast<float>(this->unknown_start_scan_count_));
      break;
    case protocol::UNKNOWN_RESPONSE:
      this->unknown_response_count_++;
      utils::publish_state_if_present(this->unknown_response_count_sensor_,
                                      static_cast<float>(this->unknown_response_count_));
      break;
    default:
      break;
  }

  const uint32_t now = millis();
  const char *context_label = utils::unknown_context_label(context);
  if (this->last_unknown_context_ != context_label ||
      (now - this->last_unknown_ms_) > protocol::UNKNOWN_SEQUENCE_WINDOW_MS) {
    this->finalize_unknown_sequence_();
    this->last_unknown_context_ = context_label;
    this->last_unknown_observed_count_ = 0;
  }
  this->last_unknown_observed_count_++;
  if (this->last_unknown_bytes_.size() < protocol::UNKNOWN_SEQUENCE_CAPTURE_MAX) {
    this->last_unknown_bytes_.push_back(value);
  }
  this->last_unknown_ms_ = now;
  ESP_LOGV(TAG, "Unknown signal (%s): 0x%02X (count=%u)", utils::unknown_context_label(context), value,
           static_cast<unsigned>(this->unknown_signal_count_));
}

void OSGPMeter::maybe_finalize_unknown_sequence_() {
  if (this->last_unknown_bytes_.empty()) {
    return;
  }
  const uint32_t now = millis();
  if (now - this->last_unknown_ms_ > protocol::UNKNOWN_SEQUENCE_WINDOW_MS) {
    this->finalize_unknown_sequence_();
  }
}

void OSGPMeter::finalize_unknown_sequence_() {
  if (this->last_unknown_bytes_.empty()) {
    return;
  }
  const uint32_t observed = this->last_unknown_observed_count_;
  std::string payload = utils::bytes_to_hex(this->last_unknown_bytes_.data(), this->last_unknown_bytes_.size());
  const size_t captured = this->last_unknown_bytes_.size();
  bool orphaned = false;
  if (this->last_unknown_context_ == utils::unknown_context_label(protocol::UNKNOWN_PRE_SEND) && captured >= 5 &&
      this->last_unknown_bytes_[0] == protocol::IDENTITY) {
    uint8_t ctrl = this->last_unknown_bytes_[1];
    uint8_t seq = this->last_unknown_bytes_[2];
    uint16_t length = (static_cast<uint16_t>(this->last_unknown_bytes_[3]) << 8) |
                      static_cast<uint16_t>(this->last_unknown_bytes_[4]);
    std::string flags = utils::ctrl_flags_label(ctrl);
    orphaned = true;
    if (this->unknown_signal_count_ >= observed) {
      this->unknown_signal_count_ -= observed;
    } else {
      this->unknown_signal_count_ = 0;
    }
    if (this->unknown_pre_send_count_ >= observed) {
      this->unknown_pre_send_count_ -= observed;
    } else {
      this->unknown_pre_send_count_ = 0;
    }
    utils::publish_state_if_present(this->unknown_signal_count_sensor_, static_cast<float>(this->unknown_signal_count_));
    utils::publish_state_if_present(this->unknown_pre_send_count_sensor_,
                                    static_cast<float>(this->unknown_pre_send_count_));
    ESP_LOGI(TAG,
             "Orphaned frame header (missing START, filtered) ctrl=0x%02X flags=%s seq=%u len=%u observed=%u "
             "captured=%u bytes=%s",
             ctrl, flags.c_str(), seq, length, static_cast<unsigned>(observed), static_cast<unsigned>(captured),
             payload.c_str());
  } else {
    this->unknown_sequence_count_++;
    utils::publish_state_if_present(this->unknown_sequence_count_sensor_,
                                    static_cast<float>(this->unknown_sequence_count_));
    ESP_LOGW(TAG, "Unknown sequence (%s) len=%u bytes=%s seq=%u", this->last_unknown_context_.c_str(),
             static_cast<unsigned>(captured), payload.c_str(), static_cast<unsigned>(this->unknown_sequence_count_));
  }
#ifdef USE_TEXT_SENSOR
  std::string out = orphaned ? "orphaned_pre_send" : this->last_unknown_context_;
  out.append(" observed=");
  out.append(std::to_string(observed));
  out.append(" len=");
  out.append(std::to_string(static_cast<unsigned>(captured)));
  out.append(" bytes=");
  out.append(payload);
  if (!orphaned) {
    out.append(" seq=");
    out.append(std::to_string(this->unknown_sequence_count_));
  }
  utils::publish_state_if_present(this->unknown_sequence_last_text_sensor_, out);
#endif
  this->last_unknown_bytes_.clear();
  this->last_unknown_observed_count_ = 0;
}

}  // namespace osgp_meter
}  // namespace esphome
