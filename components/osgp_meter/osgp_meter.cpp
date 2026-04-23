#include "osgp_meter.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esp_system.h"
#include "crc16.h"
#include "osgp_protocol_constants.h"
#include "osgp_utils.h"
#ifdef USE_ESP32
#include "esp_task_wdt.h"
#endif

namespace esphome {
namespace osgp_meter {

static const char *const TAG = "osgp_meter";

static const char *reset_reason_label(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:
      return "unknown";
    case ESP_RST_POWERON:
      return "power-on";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "int_wdt";
    case ESP_RST_TASK_WDT:
      return "task_wdt";
    case ESP_RST_WDT:
      return "other_wdt";
    case ESP_RST_DEEPSLEEP:
      return "deep_sleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    default:
      return "unknown";
  }
}

uint8_t ByteReader::get_u8() {
  if (this->pos_ >= this->data_.size()) {
    return 0;
  }
  return this->data_[this->pos_++];
}

uint16_t ByteReader::get_u16_be() {
  if (this->pos_ + 1 >= this->data_.size()) {
    this->pos_ = this->data_.size();
    return 0;
  }
  uint16_t value = (static_cast<uint16_t>(this->data_[this->pos_]) << 8) |
                   static_cast<uint16_t>(this->data_[this->pos_ + 1]);
  this->pos_ += 2;
  return value;
}

int32_t ByteReader::get_i32() {
  if (this->pos_ + 3 >= this->data_.size()) {
    this->pos_ = this->data_.size();
    return 0;
  }
  uint32_t value = 0;
  if (this->order_little_) {
    value = static_cast<uint32_t>(this->data_[this->pos_]) |
            (static_cast<uint32_t>(this->data_[this->pos_ + 1]) << 8) |
            (static_cast<uint32_t>(this->data_[this->pos_ + 2]) << 16) |
            (static_cast<uint32_t>(this->data_[this->pos_ + 3]) << 24);
  } else {
    value = (static_cast<uint32_t>(this->data_[this->pos_]) << 24) |
            (static_cast<uint32_t>(this->data_[this->pos_ + 1]) << 16) |
            (static_cast<uint32_t>(this->data_[this->pos_ + 2]) << 8) |
            static_cast<uint32_t>(this->data_[this->pos_ + 3]);
  }
  this->pos_ += 4;
  return static_cast<int32_t>(value);
}

void OSGPMeter::setup() {
  this->tx_frame_buffer_.reserve(128);
  this->rx_tail_buffer_.reserve(256);
  this->rx_frame_log_buffer_.reserve(256);
  this->rx_contents_buffer_.reserve(256);
  this->request_payload_buffer_.reserve(64);
  this->request_response_buffer_.reserve(256);
  this->last_unknown_bytes_.reserve(protocol::UNKNOWN_SEQUENCE_CAPTURE_MAX);

  esp_reset_reason_t reason = esp_reset_reason();
  const char *label = reset_reason_label(reason);
  ESP_LOGI(TAG, "Reset reason: %s (%d)", label, static_cast<int>(reason));
#ifdef USE_TEXT_SENSOR
  utils::publish_state_if_present(this->reset_reason_text_sensor_, label);
#endif

  this->session_state_ = SessionState::INIT_WAIT;
  this->poll_due_ = true;
  this->next_action_ms_ = 0;
  this->wakeup_remaining_ = 0;
  this->reset_rx_parser_();
}

void OSGPMeter::feed_watchdog_() {
#ifdef USE_ESP32
  esp_task_wdt_reset();
#endif
}

const char *OSGPMeter::connect_state_label_() const {
  switch (this->connect_state_) {
    case ConnectState::INIT:
      return "init";
    case ConnectState::CONNECTED:
      return "connected";
    default:
      return "unknown";
  }
}

void OSGPMeter::dump_config() {
  ESP_LOGCONFIG(TAG, "OSGP Meter");
  ESP_LOGCONFIG(TAG, "  User ID: %u", this->user_id_);
  ESP_LOGCONFIG(TAG, "  Username: %s", this->username_.c_str());
  ESP_LOGCONFIG(TAG, "  Refresh interval: %u ms", this->refresh_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Logoff interval: %u ms", this->logoff_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Static info interval: %u ms", this->static_info_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Poll jitter: %u ms", this->poll_jitter_ms_);
  ESP_LOGCONFIG(TAG, "  Health log interval: %u ms", this->health_log_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Raw frame logging: %s", this->log_raw_ ? "true" : "false");
}

void OSGPMeter::set_refresh_interval(uint32_t interval_ms) {
  this->refresh_interval_ms_ = interval_ms;
  this->set_update_interval(interval_ms);
}

void OSGPMeter::update() { this->poll_due_ = true; }

void OSGPMeter::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  this->process_uart_(now);
  this->maybe_finalize_unknown_sequence_();
  this->process_session_state_(now);
  this->process_request_engine_(now);
  this->maybe_log_health_(now);
}

void OSGPMeter::schedule_init_backoff_(const char *reason) {
  if (this->init_failures_ < 6) {
    this->init_failures_++;
  }
  uint32_t backoff_ms = 2000U << (this->init_failures_ - 1);
  if (backoff_ms > 60000U) {
    backoff_ms = 60000U;
  }
  const uint32_t now = App.get_loop_component_start_time();
  this->next_init_ms_ = now + backoff_ms;
  this->session_state_ = SessionState::INIT_WAIT;
  this->request_active_ = false;
  this->request_completed_ = false;
  this->request_success_ = false;
  this->request_state_ = RequestState::IDLE;
  this->request_response_buffer_.clear();
  this->rx_contents_buffer_.clear();
  this->reset_rx_parser_();
  ESP_LOGW(TAG, "Init failed (%s), backing off for %u ms", reason, backoff_ms);
}

bool OSGPMeter::has_tou_sensors_() const {
  for (uint8_t i = 0; i < MAX_TOU_TIERS; i++) {
    if (this->tou_fwd_active_energy_sensors_[i] != nullptr || this->tou_rev_active_energy_sensors_[i] != nullptr) {
      return true;
    }
  }
  return false;
}

bool OSGPMeter::has_static_info_sensors_() const {
#ifdef USE_TEXT_SENSOR
  return this->manufacturer_text_sensor_ != nullptr || this->model_text_sensor_ != nullptr ||
         this->hardware_version_text_sensor_ != nullptr || this->firmware_version_text_sensor_ != nullptr ||
         this->manufacturer_serial_text_sensor_ != nullptr || this->utility_serial_text_sensor_ != nullptr;
#else
  return false;
#endif
}

void OSGPMeter::reset_bt21_bt22_() {
  this->bt21_loaded_ = false;
  this->bt22_loaded_ = false;
  this->demand_reset_counter_ = false;
  this->num_summations_ = 0;
  this->num_tiers_ = 0;
  this->num_demands_ = 0;
  this->num_coincident_ = 0;
  this->warned_missing_tou_sources_ = false;
  this->warned_tier_block_skip_ = false;
  this->summation_source_ids_.clear();
  this->summation_index_fwd_ = -1;
  this->summation_index_rev_ = -1;
  this->tou_tier_block_fallback_needed_ = false;
  this->tou_tier_block_index_ = 0;
  this->tou_tier_block_count_ = 0;
  for (uint8_t i = 0; i < MAX_TOU_TIERS; i++) {
    this->tier_fwd_source_index_[i] = -1;
    this->tier_rev_source_index_[i] = -1;
  }
}

bool OSGPMeter::can_send_now_(uint32_t now) const {
  if (now - this->last_send_ms_ < protocol::MIN_REQUEST_INTERVAL_MS) {
    return false;
  }
  if (this->last_rx_ms_ != 0 && (now - this->last_rx_ms_) < protocol::RX_QUIET_TIME_MS) {
    return false;
  }
  return true;
}

void OSGPMeter::reset_rx_parser_() {
  this->rx_parser_state_ = RxParserState::WAIT_START;
  this->rx_header_pos_ = 0;
  this->rx_payload_pos_ = 0;
  this->rx_expected_length_ = 0;
}

bool OSGPMeter::begin_request_(const uint8_t *payload, size_t length, bool hide_contents, const char *name) {
  if (this->request_active_) {
    return false;
  }

  this->request_payload_buffer_.assign(payload, payload + length);
  this->request_name_ = name;
  this->request_hide_contents_ = hide_contents;
  this->request_attempts_ = 0;
  this->request_active_ = true;
  this->request_completed_ = false;
  this->request_success_ = false;
  this->request_state_ = RequestState::WAIT_SEND_SLOT;
  this->request_response_buffer_.clear();
  this->rx_contents_buffer_.clear();
  this->request_send_log_.clear();
  this->request_send_log_ready_ = false;
  this->reset_rx_parser_();
  this->request_next_send_ms_ = App.get_loop_component_start_time();

  this->tx_frame_buffer_.clear();
  this->tx_frame_buffer_.reserve(length + 8);
  this->tx_frame_buffer_.push_back(protocol::START);
  this->tx_frame_buffer_.push_back(protocol::IDENTITY);
  this->tx_frame_buffer_.push_back(this->toggle_control_ ? 0x20 : 0x00);
  this->tx_frame_buffer_.push_back(0x00);
  this->tx_frame_buffer_.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
  this->tx_frame_buffer_.push_back(static_cast<uint8_t>(length & 0xFF));
  this->toggle_control_ = !this->toggle_control_;
  this->tx_frame_buffer_.insert(this->tx_frame_buffer_.end(), payload, payload + length);

  CRC16 crc16;
  uint16_t crc = crc16.calculate(this->tx_frame_buffer_.data(), this->tx_frame_buffer_.size(), 0xFFFF) ^ 0xFFFF;
  this->tx_frame_buffer_.push_back(static_cast<uint8_t>(crc & 0xFF));
  this->tx_frame_buffer_.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));

  return true;
}

OSGPMeter::StepResult OSGPMeter::run_request_step_(const uint8_t *payload, size_t length, bool hide_contents,
                                                   const char *name) {
  if (!this->request_active_) {
    this->begin_request_(payload, length, hide_contents, name);
    return StepResult::IN_PROGRESS;
  }
  if (!this->request_completed_) {
    return StepResult::IN_PROGRESS;
  }

  StepResult result = this->request_success_ ? StepResult::SUCCESS : StepResult::FAILURE;
  this->request_active_ = false;
  this->request_completed_ = false;
  this->request_success_ = false;
  this->request_state_ = RequestState::IDLE;
  if (result == StepResult::FAILURE) {
    this->request_response_buffer_.clear();
  }
  return result;
}

bool OSGPMeter::consume_request_response_ack_(ByteReader &reader, const char *context) {
  if (this->request_response_buffer_.empty()) {
    ESP_LOGW(TAG, "Empty response for %s", context);
    return false;
  }
  ByteReader response_reader(std::move(this->request_response_buffer_));
  this->request_response_buffer_.clear();
  uint8_t response = response_reader.get_u8();
  if (response != 0x00) {
    ESP_LOGW(TAG, "Unexpected response code %u for %s", response, context);
    return false;
  }
  reader = std::move(response_reader);
  return true;
}

void OSGPMeter::send_request_frame_now_(uint32_t now) {
  if (this->log_raw_) {
    if (!this->request_send_log_ready_) {
      this->request_send_log_ = this->request_hide_contents_ ? std::string("<hidden>")
                                                             : utils::bytes_to_hex(this->tx_frame_buffer_.data(),
                                                                                   this->tx_frame_buffer_.size());
      this->request_send_log_ready_ = true;
    }
    ESP_LOGVV(TAG, "TX %s", this->request_send_log_.c_str());
  }
  this->write_array(this->tx_frame_buffer_.data(), this->tx_frame_buffer_.size());
  this->flush();
  this->last_send_ms_ = now;
  this->request_attempts_++;
  this->request_state_ = RequestState::WAIT_TX_ACK;
  this->request_deadline_ms_ = now + protocol::RESPONSE_TIMEOUT_MS;
}

void OSGPMeter::fail_request_(const char *reason) {
  this->request_state_ = RequestState::IDLE;
  this->request_completed_ = true;
  this->request_success_ = false;
  this->request_response_buffer_.clear();
  this->rx_contents_buffer_.clear();
  this->reset_rx_parser_();
  ESP_LOGW(TAG, "%s failed: %s", this->request_name_.c_str(), reason);
}

void OSGPMeter::schedule_request_retry_(uint32_t now, const char *reason) {
  if (this->request_attempts_ >= protocol::SEND_RETRY_COUNT) {
    this->fail_request_(reason);
    return;
  }
  ESP_LOGV(TAG, "Retrying %s after %s (%u/%u)", this->request_name_.c_str(), reason,
           static_cast<unsigned>(this->request_attempts_), static_cast<unsigned>(protocol::SEND_RETRY_COUNT));
  this->request_state_ = RequestState::WAIT_SEND_SLOT;
  this->request_next_send_ms_ = now + protocol::MIN_REQUEST_INTERVAL_MS;
  this->request_deadline_ms_ = 0;
  this->rx_contents_buffer_.clear();
  this->reset_rx_parser_();
}

void OSGPMeter::process_request_engine_(uint32_t now) {
  if (!this->request_active_ || this->request_completed_) {
    return;
  }

  switch (this->request_state_) {
    case RequestState::WAIT_SEND_SLOT: {
      if (now < this->request_next_send_ms_) {
        return;
      }
      if (!this->can_send_now_(now)) {
        uint32_t earliest = now;
        if (now - this->last_send_ms_ < protocol::MIN_REQUEST_INTERVAL_MS) {
          earliest = std::max(earliest, this->last_send_ms_ + protocol::MIN_REQUEST_INTERVAL_MS);
        }
        if (this->last_rx_ms_ != 0 && (now - this->last_rx_ms_) < protocol::RX_QUIET_TIME_MS) {
          earliest = std::max(earliest, this->last_rx_ms_ + protocol::RX_QUIET_TIME_MS);
        }
        this->request_next_send_ms_ = earliest;
        return;
      }
      this->send_request_frame_now_(now);
      return;
    }
    case RequestState::WAIT_TX_ACK:
      if (now > this->request_deadline_ms_) {
        this->schedule_request_retry_(now, "tx_ack_timeout");
      }
      return;
    case RequestState::WAIT_RESPONSE:
      if (now > this->request_deadline_ms_) {
        this->schedule_request_retry_(now, "response_timeout");
      }
      return;
    case RequestState::IDLE:
    default:
      return;
  }
}

void OSGPMeter::handle_rx_frame_complete_(uint32_t now) {
  CRC16 crc16;
  const uint16_t message_crc = static_cast<uint16_t>(this->rx_tail_buffer_[this->rx_expected_length_]) |
                               (static_cast<uint16_t>(this->rx_tail_buffer_[this->rx_expected_length_ + 1]) << 8);
  uint8_t start = protocol::START;
  uint16_t calculated_crc = crc16.calculate(&start, 1, 0xFFFF);
  calculated_crc = crc16.calculate(this->rx_header_.data(), this->rx_header_.size(), calculated_crc);
  calculated_crc = crc16.calculate(this->rx_tail_buffer_.data(), this->rx_expected_length_, calculated_crc) ^ 0xFFFF;
  if (message_crc != calculated_crc) {
    ESP_LOGW(TAG, "Incorrect CRC received. Calculated %04X got %04X", calculated_crc, message_crc);
    this->crc_error_count_++;
    this->write_byte(protocol::NACK);
    if (this->request_state_ == RequestState::WAIT_RESPONSE) {
      this->request_deadline_ms_ = now + protocol::RESPONSE_TIMEOUT_MS;
    }
    return;
  }

  this->write_byte(protocol::ACK);
  const uint8_t ctrl = this->rx_header_[1];
  const uint8_t sequence = this->rx_header_[2];
  const bool multipacket = (ctrl & 0x80) != 0;

  this->rx_contents_buffer_.insert(this->rx_contents_buffer_.end(), this->rx_tail_buffer_.begin(),
                                   this->rx_tail_buffer_.begin() + this->rx_expected_length_);

  if (multipacket && sequence != 0) {
    if (this->request_state_ == RequestState::WAIT_RESPONSE) {
      this->request_deadline_ms_ = now + protocol::RESPONSE_TIMEOUT_MS;
    }
    return;
  }

  if (this->request_state_ == RequestState::WAIT_RESPONSE) {
    this->request_response_buffer_ = this->rx_contents_buffer_;
    this->request_state_ = RequestState::IDLE;
    this->request_completed_ = true;
    this->request_success_ = true;
    this->rx_contents_buffer_.clear();
    return;
  }

  ESP_LOGD(TAG, "Received unsolicited frame (len=%u)", static_cast<unsigned>(this->rx_expected_length_));
  this->rx_contents_buffer_.clear();
}

void OSGPMeter::process_uart_byte_(uint8_t byte, uint32_t now) {
  this->last_rx_ms_ = now;

  if (this->request_state_ == RequestState::WAIT_TX_ACK) {
    if (byte == protocol::ACK || byte == 0x00) {
      if (byte == 0x00) {
        ESP_LOGV(TAG, "Received 0x00 and accepted as ACK");
      } else {
        ESP_LOGV(TAG, "Received ACK");
      }
      this->request_state_ = RequestState::WAIT_RESPONSE;
      this->request_deadline_ms_ = now + protocol::RESPONSE_TIMEOUT_MS;
      this->rx_contents_buffer_.clear();
      this->reset_rx_parser_();
      return;
    }
    if (byte == protocol::NACK) {
      this->schedule_request_retry_(now, "tx_nack");
      return;
    }
    this->record_unknown_signal_(byte, protocol::UNKNOWN_RESPONSE);
    this->schedule_request_retry_(now, "tx_unknown_response");
    return;
  }

  const uint8_t unknown_context =
      (this->request_state_ == RequestState::WAIT_RESPONSE) ? protocol::UNKNOWN_START_SCAN : protocol::UNKNOWN_PRE_SEND;

  switch (this->rx_parser_state_) {
    case RxParserState::WAIT_START:
      if (byte != protocol::START) {
        this->record_unknown_signal_(byte, unknown_context);
        return;
      }
      this->rx_parser_state_ = RxParserState::READ_HEADER;
      this->rx_header_pos_ = 0;
      if (this->request_state_ == RequestState::WAIT_RESPONSE) {
        this->request_deadline_ms_ = now + protocol::INTER_BYTE_TIMEOUT_MS + protocol::SEGMENT_DELAY_MS;
      }
      return;
    case RxParserState::READ_HEADER:
      this->rx_header_[this->rx_header_pos_++] = byte;
      if (this->request_state_ == RequestState::WAIT_RESPONSE) {
        this->request_deadline_ms_ = now + protocol::INTER_BYTE_TIMEOUT_MS + protocol::SEGMENT_DELAY_MS;
      }
      if (this->rx_header_pos_ < this->rx_header_.size()) {
        return;
      }
      this->rx_expected_length_ = (static_cast<uint16_t>(this->rx_header_[3]) << 8) | this->rx_header_[4];
      this->rx_tail_buffer_.resize(static_cast<size_t>(this->rx_expected_length_) + 2U);
      this->rx_payload_pos_ = 0;
      this->rx_parser_state_ = RxParserState::READ_PAYLOAD;
      return;
    case RxParserState::READ_PAYLOAD:
      this->rx_tail_buffer_[this->rx_payload_pos_++] = byte;
      if (this->request_state_ == RequestState::WAIT_RESPONSE) {
        this->request_deadline_ms_ = now + protocol::INTER_BYTE_TIMEOUT_MS + protocol::SEGMENT_DELAY_MS;
      }
      if (this->rx_payload_pos_ < this->rx_tail_buffer_.size()) {
        return;
      }
      this->handle_rx_frame_complete_(now);
      this->reset_rx_parser_();
      return;
    default:
      this->reset_rx_parser_();
      return;
  }
}

void OSGPMeter::process_uart_(uint32_t now) {
  while (this->available()) {
    uint8_t byte = 0;
    if (!this->read_byte(&byte)) {
      break;
    }
    this->process_uart_byte_(byte, now);
  }
}

bool OSGPMeter::parse_bt21_reply_(ByteReader &reader) {
  if (reader.limit() < 12) {
    ESP_LOGW(TAG, "BT21 reply too short: %u", static_cast<unsigned>(reader.limit()));
    return false;
  }
  uint16_t table_length = reader.get_u16_be();
  uint8_t flags0 = reader.get_u8();
  reader.get_u8();  // flags1
  reader.get_u8();  // num_selfreads
  this->num_summations_ = reader.get_u8();
  this->num_demands_ = reader.get_u8();
  this->num_coincident_ = reader.get_u8();
  reader.get_u8();  // num_occurrences
  this->num_tiers_ = reader.get_u8();
  reader.get_u8();  // num_present_demands
  reader.get_u8();  // num_present_values
  this->demand_reset_counter_ = (flags0 & 0x04) != 0;
  this->bt21_loaded_ = true;
  ESP_LOGD(TAG, "BT21 length %u summations %u tiers %u demands %u coincident %u", table_length, this->num_summations_,
           this->num_tiers_, this->num_demands_, this->num_coincident_);
  return true;
}

bool OSGPMeter::parse_bt22_reply_(ByteReader &reader) {
  if (this->num_summations_ == 0) {
    ESP_LOGW(TAG, "BT22 read skipped because BT21 reports 0 summations");
    return false;
  }
  if (reader.limit() < static_cast<size_t>(2 + this->num_summations_)) {
    ESP_LOGW(TAG, "BT22 reply too short: %u", static_cast<unsigned>(reader.limit()));
    return false;
  }
  uint16_t table_length = reader.get_u16_be();
  this->summation_source_ids_.assign(this->num_summations_, 0);
  this->summation_index_fwd_ = -1;
  this->summation_index_rev_ = -1;
  for (uint8_t i = 0; i < this->num_summations_; i++) {
    uint8_t source_id = reader.get_u8();
    this->summation_source_ids_[i] = source_id;
    if (source_id == 0) {
      this->summation_index_fwd_ = static_cast<int8_t>(i);
    } else if (source_id == 1) {
      this->summation_index_rev_ = static_cast<int8_t>(i);
    }
    for (uint8_t tier = 0; tier < MAX_TOU_TIERS; tier++) {
      if (source_id == 29 + static_cast<uint8_t>(11 * tier)) {
        this->tier_fwd_source_index_[tier] = static_cast<int8_t>(i);
      } else if (source_id == 30 + static_cast<uint8_t>(11 * tier)) {
        this->tier_rev_source_index_[tier] = static_cast<int8_t>(i);
      }
    }
  }
  this->bt22_loaded_ = true;
  ESP_LOGD(TAG, "BT22 length %u loaded %u summation source IDs", table_length, this->num_summations_);
  return true;
}

bool OSGPMeter::parse_bt01_reply_(ByteReader &reader) {
  if (reader.limit() < 2 + 24) {
    ESP_LOGW(TAG, "BT01 reply too short: %u", static_cast<unsigned>(reader.limit()));
    return false;
  }
  reader.get_u16_be();
  std::string manufacturer;
  manufacturer.reserve(4);
  for (uint8_t i = 0; i < 4; i++) {
    manufacturer.push_back(static_cast<char>(reader.get_u8()));
  }
  std::string model;
  model.reserve(8);
  for (uint8_t i = 0; i < 8; i++) {
    model.push_back(static_cast<char>(reader.get_u8()));
  }
  uint8_t hw_version = reader.get_u8();
  uint8_t hw_revision = reader.get_u8();
  uint8_t fw_version = reader.get_u8();
  uint8_t fw_revision = reader.get_u8();

  std::vector<uint8_t> serial_bcd;
  serial_bcd.reserve(8);
  for (uint8_t i = 0; i < 8; i++) {
    serial_bcd.push_back(reader.get_u8());
  }

#ifdef USE_TEXT_SENSOR
  utils::publish_state_if_present(this->manufacturer_text_sensor_, utils::trim_right(manufacturer));
  utils::publish_state_if_present(this->model_text_sensor_, utils::trim_right(model));
  utils::publish_state_if_present(this->hardware_version_text_sensor_,
                                  utils::firmware_version_string(hw_version, hw_revision));
  utils::publish_state_if_present(this->firmware_version_text_sensor_,
                                  utils::firmware_version_string(fw_version, fw_revision));
  utils::publish_state_if_present(this->manufacturer_serial_text_sensor_, utils::bcd_to_string(serial_bcd));
#endif
  return true;
}

bool OSGPMeter::parse_et03_reply_(ByteReader &reader) {
  if (reader.limit() < 2 + 30) {
    ESP_LOGW(TAG, "ET03 reply too short: %u", static_cast<unsigned>(reader.limit()));
    return false;
  }
  reader.get_u16_be();
  std::string serial;
  serial.reserve(30);
  for (uint8_t i = 0; i < 30; i++) {
    char c = static_cast<char>(reader.get_u8());
    if (c == '\0') {
      break;
    }
    serial.push_back(c);
  }
#ifdef USE_TEXT_SENSOR
  utils::publish_state_if_present(this->utility_serial_text_sensor_, utils::trim_right(serial));
#endif
  return true;
}

bool OSGPMeter::parse_tou_tier_block_reply_(ByteReader &reader, uint8_t tier) {
  const uint32_t summation_bytes = 4U * this->num_summations_;
  if (reader.limit() < 2 + summation_bytes) {
    ESP_LOGW(TAG, "BT23 tier block %u reply too short: %u", static_cast<unsigned>(tier + 1),
             static_cast<unsigned>(reader.limit()));
    return false;
  }

  reader.get_u16_be();
  reader.set_order_little(this->byte_order_little_);

  const int idx_fwd = this->summation_index_fwd_ >= 0 ? this->summation_index_fwd_ : 0;
  const int idx_rev = this->summation_index_rev_ >= 0 ? this->summation_index_rev_ : 1;
  int32_t fwd_wh = 0;
  int32_t rev_wh = 0;
  bool got_fwd = false;
  bool got_rev = false;

  for (uint8_t i = 0; i < this->num_summations_; i++) {
    const int32_t value = reader.get_i32();
    if (i == static_cast<uint8_t>(idx_fwd)) {
      fwd_wh = value;
      got_fwd = true;
    }
    if (i == static_cast<uint8_t>(idx_rev)) {
      rev_wh = value;
      got_rev = true;
    }
  }

  if (got_fwd) {
    utils::publish_state_if_present(this->tou_fwd_active_energy_sensors_[tier], static_cast<float>(fwd_wh) / 1000.0f);
  }
  if (got_rev) {
    utils::publish_state_if_present(this->tou_rev_active_energy_sensors_[tier], static_cast<float>(rev_wh) / 1000.0f);
  }
  return got_fwd || got_rev;
}

void OSGPMeter::process_session_state_(uint32_t now) {
  switch (this->session_state_) {
    case SessionState::INIT_WAIT:
      if (this->next_init_ms_ != 0 && now < this->next_init_ms_) {
        return;
      }
      this->toggle_control_ = false;
      this->reset_bt21_bt22_();
      this->request_active_ = false;
      this->request_state_ = RequestState::IDLE;
      this->request_completed_ = false;
      this->request_success_ = false;
      this->request_response_buffer_.clear();
      this->rx_contents_buffer_.clear();
      this->reset_rx_parser_();
      if (this->poll_jitter_ms_ > 0) {
        const uint32_t jitter = esphome::random_uint32() % (this->poll_jitter_ms_ + 1U);
        this->next_action_ms_ = now + jitter;
        this->session_state_ = SessionState::INIT_JITTER_WAIT;
      } else {
        this->session_state_ = SessionState::WAKEUP;
        this->wakeup_remaining_ = protocol::WAKEUP_COUNT;
        this->next_action_ms_ = now;
      }
      return;

    case SessionState::INIT_JITTER_WAIT:
      if (now < this->next_action_ms_) {
        return;
      }
      this->session_state_ = SessionState::WAKEUP;
      this->wakeup_remaining_ = protocol::WAKEUP_COUNT;
      this->next_action_ms_ = now;
      return;

    case SessionState::WAKEUP:
      if (now < this->next_action_ms_) {
        return;
      }
      if (this->wakeup_remaining_ > 0) {
        this->write_byte(protocol::WAKEUP_BYTE);
        this->flush();
        this->feed_watchdog_();
        this->wakeup_remaining_--;
        this->next_action_ms_ = now + protocol::WAKEUP_DELAY_MS;
        return;
      }
      this->session_state_ = SessionState::REQ_IDENT;
      return;

    case SessionState::REQ_IDENT: {
      uint8_t payload[1] = {protocol::REQUEST_ID_IDENT};
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "Ident");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::FAILURE) {
        this->schedule_init_backoff_("ident");
        return;
      }
      ByteReader reader(std::vector<uint8_t>{});
      if (!this->consume_request_response_ack_(reader, "Ident")) {
        this->schedule_init_backoff_("ident_ack");
        return;
      }
      this->session_state_ = SessionState::REQ_NEGOTIATE;
      return;
    }

    case SessionState::REQ_NEGOTIATE: {
      uint8_t payload[5] = {protocol::REQUEST_ID_NEGOTIATE2, 0x00, 0x40, 0x02, protocol::BAUD_9600_ENUM};
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "Negotiate");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::FAILURE) {
        this->schedule_init_backoff_("negotiate");
        return;
      }
      ByteReader reader(std::vector<uint8_t>{});
      if (!this->consume_request_response_ack_(reader, "Negotiate")) {
        this->schedule_init_backoff_("negotiate_ack");
        return;
      }
      this->session_state_ = SessionState::REQ_LOGON;
      return;
    }

    case SessionState::REQ_LOGON: {
      uint8_t payload[13];
      payload[0] = protocol::REQUEST_ID_LOGON;
      payload[1] = static_cast<uint8_t>((this->user_id_ >> 8) & 0xFF);
      payload[2] = static_cast<uint8_t>(this->user_id_ & 0xFF);
      std::string username = this->username_;
      if (username.size() > 10) {
        username.resize(10);
      }
      for (size_t i = 0; i < 10; i++) {
        payload[3 + i] = (i < username.size()) ? static_cast<uint8_t>(username[i]) : static_cast<uint8_t>(' ');
      }

      StepResult step = this->run_request_step_(payload, sizeof(payload), true, "Logon");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::FAILURE) {
        this->schedule_init_backoff_("logon");
        return;
      }
      ByteReader reader(std::vector<uint8_t>{});
      if (!this->consume_request_response_ack_(reader, "Logon")) {
        this->schedule_init_backoff_("logon_ack");
        return;
      }
      this->last_logon_ms_ = now;
      this->session_state_ = SessionState::REQ_SECURITY;
      return;
    }

    case SessionState::REQ_SECURITY: {
      uint8_t payload[21];
      payload[0] = protocol::REQUEST_ID_SECURITY;
      std::string password = this->password_;
      if (password.size() > 20) {
        password.resize(20);
      }
      for (size_t i = 0; i < 20; i++) {
        payload[1 + i] = (i < password.size()) ? static_cast<uint8_t>(password[i]) : 0x00;
      }

      StepResult step = this->run_request_step_(payload, sizeof(payload), true, "Security");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::FAILURE) {
        this->schedule_init_backoff_("security");
        return;
      }
      ByteReader reader(std::vector<uint8_t>{});
      if (!this->consume_request_response_ack_(reader, "Security")) {
        this->schedule_init_backoff_("security_ack");
        return;
      }
      this->session_state_ = SessionState::REQ_TABLE0;
      return;
    }

    case SessionState::REQ_TABLE0: {
      uint8_t payload[3] = {protocol::REQUEST_ID_READ, 0x00, 0x00};
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "ReadTable0");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::FAILURE) {
        this->schedule_init_backoff_("table0");
        return;
      }
      ByteReader reader(std::vector<uint8_t>{});
      if (!this->consume_request_response_ack_(reader, "Table0")) {
        this->schedule_init_backoff_("table0_ack");
        return;
      }
      if (!this->handle_table0_reply_(reader)) {
        this->schedule_init_backoff_("table0_parse");
        return;
      }
      this->connect_state_ = ConnectState::CONNECTED;
      this->init_failures_ = 0;
      this->next_init_ms_ = 0;
      this->poll_due_ = true;
      this->session_state_ = SessionState::CONNECTED_IDLE;
      return;
    }

    case SessionState::CONNECTED_IDLE:
      if (now - this->last_logon_ms_ + this->refresh_interval_ms_ > this->logoff_interval_ms_) {
        this->session_state_ = SessionState::REQ_LOGOFF;
        return;
      }
      if (!this->poll_due_) {
        return;
      }
      this->poll_due_ = false;
      if (this->poll_jitter_ms_ > 0) {
        const uint32_t jitter = esphome::random_uint32() % (this->poll_jitter_ms_ + 1U);
        this->next_action_ms_ = now + jitter;
        this->session_state_ = SessionState::POLL_JITTER_WAIT;
      } else {
        this->session_state_ = SessionState::POLL_PREPARE;
      }
      return;

    case SessionState::POLL_JITTER_WAIT:
      if (now >= this->next_action_ms_) {
        this->session_state_ = SessionState::POLL_PREPARE;
      }
      return;

    case SessionState::POLL_PREPARE:
      this->use_tou_for_poll_ = this->has_tou_sensors_();
      this->tou_tier_block_fallback_needed_ = false;
      this->tou_tier_block_index_ = 0;
      this->tou_tier_block_count_ = 0;
      if (this->use_tou_for_poll_ && !this->bt21_loaded_) {
        this->session_state_ = SessionState::POLL_LOAD_BT21;
        return;
      }
      if (this->use_tou_for_poll_ && !this->bt22_loaded_) {
        this->session_state_ = SessionState::POLL_LOAD_BT22;
        return;
      }
      this->static_info_updated_this_poll_ = false;
#ifdef USE_TEXT_SENSOR
      this->static_bt01_needed_ = (this->manufacturer_text_sensor_ != nullptr || this->model_text_sensor_ != nullptr ||
                                   this->hardware_version_text_sensor_ != nullptr ||
                                   this->firmware_version_text_sensor_ != nullptr ||
                                   this->manufacturer_serial_text_sensor_ != nullptr);
      this->static_et03_needed_ = (this->utility_serial_text_sensor_ != nullptr);
#else
      this->static_bt01_needed_ = false;
      this->static_et03_needed_ = false;
#endif
      if (!this->has_static_info_sensors_() || this->static_info_interval_ms_ == 0 ||
          (this->last_static_info_ms_ != 0 && now - this->last_static_info_ms_ < this->static_info_interval_ms_)) {
        this->static_bt01_needed_ = false;
        this->static_et03_needed_ = false;
      }
      if (this->static_bt01_needed_) {
        this->session_state_ = SessionState::POLL_STATIC_BT01;
        return;
      }
      if (this->static_et03_needed_) {
        this->session_state_ = SessionState::POLL_STATIC_ET03;
        return;
      }
      this->session_state_ = SessionState::POLL_READ_TABLE28;
      return;

    case SessionState::POLL_LOAD_BT21: {
      uint8_t payload[8] = {protocol::REQUEST_ID_READ_PARTIAL, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x0A};
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "ReadBT21");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::FAILURE) {
        ESP_LOGW(TAG, "TOU sensors configured but BT21 could not be loaded; continuing without TOU");
        this->use_tou_for_poll_ = false;
        this->session_state_ = SessionState::POLL_PREPARE;
        return;
      }
      ByteReader reader(std::vector<uint8_t>{});
      if (!this->consume_request_response_ack_(reader, "BT21") || !this->parse_bt21_reply_(reader)) {
        ESP_LOGW(TAG, "TOU sensors configured but BT21 parse failed; continuing without TOU");
        this->use_tou_for_poll_ = false;
      }
      this->session_state_ = SessionState::POLL_PREPARE;
      return;
    }

    case SessionState::POLL_LOAD_BT22: {
      uint8_t payload[8] = {protocol::REQUEST_ID_READ_PARTIAL, 0x00, 0x16, 0x00, 0x00, 0x00,
                            static_cast<uint8_t>((this->num_summations_ >> 8) & 0xFF),
                            static_cast<uint8_t>(this->num_summations_ & 0xFF)};
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "ReadBT22");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::FAILURE) {
        ESP_LOGW(TAG, "TOU sensors configured but BT22 could not be loaded; continuing without TOU");
        this->use_tou_for_poll_ = false;
        this->session_state_ = SessionState::POLL_PREPARE;
        return;
      }
      ByteReader reader(std::vector<uint8_t>{});
      if (!this->consume_request_response_ack_(reader, "BT22") || !this->parse_bt22_reply_(reader)) {
        ESP_LOGW(TAG, "TOU sensors configured but BT22 parse failed; continuing without TOU");
        this->use_tou_for_poll_ = false;
      }
      this->session_state_ = SessionState::POLL_PREPARE;
      return;
    }

    case SessionState::POLL_STATIC_BT01: {
      uint8_t payload[8] = {protocol::REQUEST_ID_READ_PARTIAL, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x18};
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "ReadBT01");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::SUCCESS) {
        ByteReader reader(std::vector<uint8_t>{});
        if (this->consume_request_response_ack_(reader, "BT01") && this->parse_bt01_reply_(reader)) {
          this->static_info_updated_this_poll_ = true;
        }
      }
      this->static_bt01_needed_ = false;
      if (this->static_et03_needed_) {
        this->session_state_ = SessionState::POLL_STATIC_ET03;
      } else {
        if (this->static_info_updated_this_poll_) {
          this->last_static_info_ms_ = now;
        }
        this->session_state_ = SessionState::POLL_READ_TABLE28;
      }
      return;
    }

    case SessionState::POLL_STATIC_ET03: {
      uint8_t payload[8] = {protocol::REQUEST_ID_READ_PARTIAL, 0x08, 0x03, 0x00, 0x00, 0x00, 0x00, 0x1E};
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "ReadET03");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::SUCCESS) {
        ByteReader reader(std::vector<uint8_t>{});
        if (this->consume_request_response_ack_(reader, "ET03") && this->parse_et03_reply_(reader)) {
          this->static_info_updated_this_poll_ = true;
        }
      }
      if (this->static_info_updated_this_poll_) {
        this->last_static_info_ms_ = now;
      }
      this->static_et03_needed_ = false;
      this->session_state_ = SessionState::POLL_READ_TABLE28;
      return;
    }

    case SessionState::POLL_READ_TABLE28: {
      uint8_t values_needed = 10;
      if (this->power_factor_l2_sensor_ != nullptr || this->power_factor_l3_sensor_ != nullptr) {
        values_needed = 15;
      } else if (this->power_factor_l1_sensor_ != nullptr || this->frequency_sensor_ != nullptr) {
        values_needed = 12;
      }
      this->current_table28_bytes_ = static_cast<uint16_t>(values_needed * 4U);
      uint8_t payload[8] = {
          protocol::REQUEST_ID_READ_PARTIAL,
          0x00,
          0x1C,
          0x00,
          0x00,
          0x00,
          static_cast<uint8_t>((this->current_table28_bytes_ >> 8) & 0xFF),
          static_cast<uint8_t>(this->current_table28_bytes_ & 0xFF),
      };
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "ReadTable28");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::FAILURE) {
        this->connect_state_ = ConnectState::INIT;
        this->schedule_init_backoff_("read_table28");
        return;
      }
      ByteReader reader(std::vector<uint8_t>{});
      if (!this->consume_request_response_ack_(reader, "Table28") || !this->handle_table28_reply_(reader)) {
        this->connect_state_ = ConnectState::INIT;
        this->schedule_init_backoff_("parse_table28");
        return;
      }
      this->session_state_ = SessionState::POLL_READ_TABLE23;
      return;
    }

    case SessionState::POLL_READ_TABLE23: {
      this->current_table23_bytes_ = 8;
      if (this->use_tou_for_poll_ && this->num_summations_ > 0) {
        this->current_table23_bytes_ =
            static_cast<uint16_t>(4U * this->num_summations_ + (this->demand_reset_counter_ ? 1U : 0U));
      }
      uint8_t payload[8] = {
          protocol::REQUEST_ID_READ_PARTIAL,
          0x00,
          0x17,
          0x00,
          0x00,
          0x00,
          static_cast<uint8_t>((this->current_table23_bytes_ >> 8) & 0xFF),
          static_cast<uint8_t>(this->current_table23_bytes_ & 0xFF),
      };
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "ReadTable23");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::FAILURE) {
        this->connect_state_ = ConnectState::INIT;
        this->schedule_init_backoff_("read_table23");
        return;
      }
      ByteReader reader(std::vector<uint8_t>{});
      if (!this->consume_request_response_ack_(reader, "Table23") ||
          !this->handle_table23_reply_(reader, this->use_tou_for_poll_)) {
        this->connect_state_ = ConnectState::INIT;
        this->schedule_init_backoff_("parse_table23");
        return;
      }

      this->last_success_ms_ = now;
      if (this->tou_tier_block_fallback_needed_ && this->tou_tier_block_count_ > 0) {
        this->session_state_ = SessionState::POLL_READ_TOU_TIER_BLOCK;
      } else if (now - this->last_logon_ms_ + this->refresh_interval_ms_ > this->logoff_interval_ms_) {
        this->session_state_ = SessionState::REQ_LOGOFF;
      } else {
        this->session_state_ = SessionState::CONNECTED_IDLE;
      }
      return;
    }

    case SessionState::POLL_READ_TOU_TIER_BLOCK: {
      if (this->tou_tier_block_index_ >= this->tou_tier_block_count_) {
        this->tou_tier_block_fallback_needed_ = false;
        if (now - this->last_logon_ms_ + this->refresh_interval_ms_ > this->logoff_interval_ms_) {
          this->session_state_ = SessionState::REQ_LOGOFF;
        } else {
          this->session_state_ = SessionState::CONNECTED_IDLE;
        }
        return;
      }

      const uint32_t summation_bytes = 4U * this->num_summations_;
      const uint32_t base_offset = (this->demand_reset_counter_ ? 1U : 0U) + summation_bytes;
      const uint32_t offset = base_offset + static_cast<uint32_t>(this->tou_tier_block_index_) * summation_bytes;
      uint8_t payload[8] = {
          protocol::REQUEST_ID_READ_PARTIAL,
          0x00,
          0x17,
          static_cast<uint8_t>((offset >> 16) & 0xFF),
          static_cast<uint8_t>((offset >> 8) & 0xFF),
          static_cast<uint8_t>(offset & 0xFF),
          static_cast<uint8_t>((summation_bytes >> 8) & 0xFF),
          static_cast<uint8_t>(summation_bytes & 0xFF),
      };

      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "ReadTable23TierBlock");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      if (step == StepResult::SUCCESS) {
        ByteReader reader(std::vector<uint8_t>{});
        if (this->consume_request_response_ack_(reader, "Table23TierBlock")) {
          this->parse_tou_tier_block_reply_(reader, this->tou_tier_block_index_);
        }
      } else {
        ESP_LOGW(TAG, "BT23 tier block %u read failed", static_cast<unsigned>(this->tou_tier_block_index_ + 1));
      }

      this->tou_tier_block_index_++;
      return;
    }

    case SessionState::REQ_LOGOFF: {
      uint8_t payload[1] = {protocol::REQUEST_ID_LOGOFF};
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "Logoff");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      this->session_state_ = SessionState::REQ_TERMINATE;
      return;
    }

    case SessionState::REQ_TERMINATE: {
      uint8_t payload[1] = {protocol::REQUEST_ID_TERMINATE};
      StepResult step = this->run_request_step_(payload, sizeof(payload), false, "Terminate");
      if (step == StepResult::IN_PROGRESS) {
        return;
      }
      this->connect_state_ = ConnectState::INIT;
      this->next_init_ms_ = now + this->refresh_interval_ms_;
      this->session_state_ = SessionState::INIT_WAIT;
      return;
    }

    default:
      this->session_state_ = SessionState::INIT_WAIT;
      return;
  }
}

void OSGPMeter::maybe_log_health_(uint32_t now) {
  if (this->health_log_interval_ms_ == 0) {
    return;
  }
  if (this->last_health_log_ms_ != 0 && now - this->last_health_log_ms_ < this->health_log_interval_ms_) {
    return;
  }
  this->last_health_log_ms_ = now;
  uint32_t last_success_age = (this->last_success_ms_ == 0) ? 0 : (now - this->last_success_ms_);
  uint32_t last_rx_age = (this->last_rx_ms_ == 0) ? 0 : (now - this->last_rx_ms_);
  ESP_LOGI(TAG,
           "Health state=%s last_success=%ums last_rx=%ums unknown=%u pre=%u start=%u resp=%u timeouts(h/p)=%u/%u "
           "crc=%u init_fail=%u",
           this->connect_state_label_(), last_success_age, last_rx_age, static_cast<unsigned>(this->unknown_signal_count_),
           static_cast<unsigned>(this->unknown_pre_send_count_), static_cast<unsigned>(this->unknown_start_scan_count_),
           static_cast<unsigned>(this->unknown_response_count_), static_cast<unsigned>(this->timeout_header_count_),
           static_cast<unsigned>(this->timeout_payload_count_), static_cast<unsigned>(this->crc_error_count_),
           static_cast<unsigned>(this->init_failures_));
}

}  // namespace osgp_meter
}  // namespace esphome
