#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace osgp_meter {

class ByteReader {
 public:
  explicit ByteReader(std::vector<uint8_t> data) : data_(std::move(data)) {}

  size_t limit() const { return this->data_.size(); }
  size_t position() const { return this->pos_; }
  void set_position(size_t pos) { this->pos_ = pos; }
  void set_order_little(bool little) { this->order_little_ = little; }

  uint8_t get_u8();
  uint16_t get_u16_be();
  int32_t get_i32();

 private:
  std::vector<uint8_t> data_;
  size_t pos_{0};
  bool order_little_{true};
};

class OSGPMeter : public PollingComponent, public uart::UARTDevice {
 public:
  static constexpr uint8_t MAX_TOU_TIERS = 4;

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_user_id(uint16_t user_id) { this->user_id_ = user_id; }
  void set_username(const std::string &username) { this->username_ = username; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_refresh_interval(uint32_t interval_ms);
  void set_logoff_interval(uint32_t interval_ms) { this->logoff_interval_ms_ = interval_ms; }
  void set_log_raw(bool log_raw) { this->log_raw_ = log_raw; }
  void set_static_info_interval(uint32_t interval_ms) { this->static_info_interval_ms_ = interval_ms; }
  void set_poll_jitter(uint32_t jitter_ms) { this->poll_jitter_ms_ = jitter_ms; }
  void set_health_log_interval(uint32_t interval_ms) { this->health_log_interval_ms_ = interval_ms; }

  void set_fwd_active_energy_sensor(sensor::Sensor *sensor) { this->fwd_active_energy_sensor_ = sensor; }
  void set_rev_active_energy_sensor(sensor::Sensor *sensor) { this->rev_active_energy_sensor_ = sensor; }
  void set_fwd_active_power_sensor(sensor::Sensor *sensor) { this->fwd_active_power_sensor_ = sensor; }
  void set_rev_active_power_sensor(sensor::Sensor *sensor) { this->rev_active_power_sensor_ = sensor; }
  void set_import_reactive_var_sensor(sensor::Sensor *sensor) { this->import_reactive_var_sensor_ = sensor; }
  void set_export_reactive_var_sensor(sensor::Sensor *sensor) { this->export_reactive_var_sensor_ = sensor; }
  void set_l1_current_sensor(sensor::Sensor *sensor) { this->l1_current_sensor_ = sensor; }
  void set_l2_current_sensor(sensor::Sensor *sensor) { this->l2_current_sensor_ = sensor; }
  void set_l3_current_sensor(sensor::Sensor *sensor) { this->l3_current_sensor_ = sensor; }
  void set_l1_voltage_sensor(sensor::Sensor *sensor) { this->l1_voltage_sensor_ = sensor; }
  void set_l2_voltage_sensor(sensor::Sensor *sensor) { this->l2_voltage_sensor_ = sensor; }
  void set_l3_voltage_sensor(sensor::Sensor *sensor) { this->l3_voltage_sensor_ = sensor; }
  void set_power_factor_l1_sensor(sensor::Sensor *sensor) { this->power_factor_l1_sensor_ = sensor; }
  void set_power_factor_l2_sensor(sensor::Sensor *sensor) { this->power_factor_l2_sensor_ = sensor; }
  void set_power_factor_l3_sensor(sensor::Sensor *sensor) { this->power_factor_l3_sensor_ = sensor; }
  void set_frequency_sensor(sensor::Sensor *sensor) { this->frequency_sensor_ = sensor; }
  void set_unknown_signal_count_sensor(sensor::Sensor *sensor) { this->unknown_signal_count_sensor_ = sensor; }
  void set_unknown_pre_send_count_sensor(sensor::Sensor *sensor) { this->unknown_pre_send_count_sensor_ = sensor; }
  void set_unknown_start_scan_count_sensor(sensor::Sensor *sensor) { this->unknown_start_scan_count_sensor_ = sensor; }
  void set_unknown_response_count_sensor(sensor::Sensor *sensor) { this->unknown_response_count_sensor_ = sensor; }
  void set_unknown_sequence_count_sensor(sensor::Sensor *sensor) { this->unknown_sequence_count_sensor_ = sensor; }
#ifdef USE_TEXT_SENSOR
  void set_manufacturer_text_sensor(text_sensor::TextSensor *sensor) { this->manufacturer_text_sensor_ = sensor; }
  void set_model_text_sensor(text_sensor::TextSensor *sensor) { this->model_text_sensor_ = sensor; }
  void set_hardware_version_text_sensor(text_sensor::TextSensor *sensor) {
    this->hardware_version_text_sensor_ = sensor;
  }
  void set_firmware_version_text_sensor(text_sensor::TextSensor *sensor) {
    this->firmware_version_text_sensor_ = sensor;
  }
  void set_manufacturer_serial_text_sensor(text_sensor::TextSensor *sensor) {
    this->manufacturer_serial_text_sensor_ = sensor;
  }
  void set_utility_serial_text_sensor(text_sensor::TextSensor *sensor) { this->utility_serial_text_sensor_ = sensor; }
  void set_unknown_sequence_last_text_sensor(text_sensor::TextSensor *sensor) {
    this->unknown_sequence_last_text_sensor_ = sensor;
  }
  void set_reset_reason_text_sensor(text_sensor::TextSensor *sensor) { this->reset_reason_text_sensor_ = sensor; }
#endif
  void set_tou_fwd_active_energy_sensor(uint8_t tier, sensor::Sensor *sensor) {
    if (tier < MAX_TOU_TIERS) {
      this->tou_fwd_active_energy_sensors_[tier] = sensor;
    }
  }
  void set_tou_rev_active_energy_sensor(uint8_t tier, sensor::Sensor *sensor) {
    if (tier < MAX_TOU_TIERS) {
      this->tou_rev_active_energy_sensors_[tier] = sensor;
    }
  }

 protected:
  uint16_t user_id_{1};
  std::string username_{"OpenHAB"};
  std::string password_{};
  uint32_t refresh_interval_ms_{2000};
  uint32_t logoff_interval_ms_{540000};
  bool log_raw_{true};
  uint32_t static_info_interval_ms_{3600000};
  uint32_t last_static_info_ms_{0};
  uint32_t poll_jitter_ms_{0};
  uint32_t health_log_interval_ms_{60000};
  uint32_t last_health_log_ms_{0};
  uint32_t last_success_ms_{0};
  uint32_t timeout_header_count_{0};
  uint32_t timeout_payload_count_{0};
  uint32_t crc_error_count_{0};

  sensor::Sensor *fwd_active_energy_sensor_{nullptr};
  sensor::Sensor *rev_active_energy_sensor_{nullptr};
  sensor::Sensor *fwd_active_power_sensor_{nullptr};
  sensor::Sensor *rev_active_power_sensor_{nullptr};
  sensor::Sensor *import_reactive_var_sensor_{nullptr};
  sensor::Sensor *export_reactive_var_sensor_{nullptr};
  sensor::Sensor *l1_current_sensor_{nullptr};
  sensor::Sensor *l2_current_sensor_{nullptr};
  sensor::Sensor *l3_current_sensor_{nullptr};
  sensor::Sensor *l1_voltage_sensor_{nullptr};
  sensor::Sensor *l2_voltage_sensor_{nullptr};
  sensor::Sensor *l3_voltage_sensor_{nullptr};
  sensor::Sensor *power_factor_l1_sensor_{nullptr};
  sensor::Sensor *power_factor_l2_sensor_{nullptr};
  sensor::Sensor *power_factor_l3_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *tou_fwd_active_energy_sensors_[MAX_TOU_TIERS]{};
  sensor::Sensor *tou_rev_active_energy_sensors_[MAX_TOU_TIERS]{};
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *manufacturer_text_sensor_{nullptr};
  text_sensor::TextSensor *model_text_sensor_{nullptr};
  text_sensor::TextSensor *hardware_version_text_sensor_{nullptr};
  text_sensor::TextSensor *firmware_version_text_sensor_{nullptr};
  text_sensor::TextSensor *manufacturer_serial_text_sensor_{nullptr};
  text_sensor::TextSensor *utility_serial_text_sensor_{nullptr};
#endif
  sensor::Sensor *unknown_signal_count_sensor_{nullptr};
  sensor::Sensor *unknown_pre_send_count_sensor_{nullptr};
  sensor::Sensor *unknown_start_scan_count_sensor_{nullptr};
  sensor::Sensor *unknown_response_count_sensor_{nullptr};
  sensor::Sensor *unknown_sequence_count_sensor_{nullptr};
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *unknown_sequence_last_text_sensor_{nullptr};
  text_sensor::TextSensor *reset_reason_text_sensor_{nullptr};
#endif

  enum class ConnectState {
    INIT,
    CONNECTED,
  };

  enum class SessionState {
    INIT_WAIT,
    INIT_JITTER_WAIT,
    WAKEUP,
    REQ_IDENT,
    REQ_NEGOTIATE,
    REQ_LOGON,
    REQ_SECURITY,
    REQ_TABLE0,
    CONNECTED_IDLE,
    POLL_JITTER_WAIT,
    POLL_PREPARE,
    POLL_LOAD_BT21,
    POLL_LOAD_BT22,
    POLL_STATIC_BT01,
    POLL_STATIC_ET03,
    POLL_READ_TABLE28,
    POLL_READ_TABLE23,
    POLL_READ_TOU_TIER_BLOCK,
    REQ_LOGOFF,
    REQ_TERMINATE,
  };

  enum class RequestState {
    IDLE,
    WAIT_SEND_SLOT,
    WAIT_TX_ACK,
    WAIT_RESPONSE,
  };

  enum class RxParserState {
    WAIT_START,
    READ_HEADER,
    READ_PAYLOAD,
  };

  enum class StepResult {
    IN_PROGRESS,
    SUCCESS,
    FAILURE,
  };

  ConnectState connect_state_{ConnectState::INIT};
  SessionState session_state_{SessionState::INIT_WAIT};
  RequestState request_state_{RequestState::IDLE};
  RxParserState rx_parser_state_{RxParserState::WAIT_START};
  bool toggle_control_{false};
  bool byte_order_little_{true};
  uint32_t last_logon_ms_{0};
  uint32_t last_send_ms_{0};
  uint32_t next_init_ms_{0};
  uint8_t init_failures_{0};
  uint32_t unknown_signal_count_{0};
  uint32_t unknown_pre_send_count_{0};
  uint32_t unknown_start_scan_count_{0};
  uint32_t unknown_response_count_{0};
  uint32_t unknown_sequence_count_{0};
  uint32_t last_rx_ms_{0};
  uint32_t last_unknown_ms_{0};
  std::vector<uint8_t> last_unknown_bytes_{};
  uint32_t last_unknown_observed_count_{0};
  std::string last_unknown_context_{};
  bool bt21_loaded_{false};
  bool bt22_loaded_{false};
  bool demand_reset_counter_{false};
  uint8_t num_summations_{0};
  uint8_t num_tiers_{0};
  uint8_t num_demands_{0};
  uint8_t num_coincident_{0};
  bool warned_missing_tou_sources_{false};
  std::vector<uint8_t> summation_source_ids_{};
  int8_t summation_index_fwd_{-1};
  int8_t summation_index_rev_{-1};
  int8_t tier_fwd_source_index_[MAX_TOU_TIERS]{-1, -1, -1, -1};
  int8_t tier_rev_source_index_[MAX_TOU_TIERS]{-1, -1, -1, -1};
  std::vector<uint8_t> tx_frame_buffer_{};
  std::vector<uint8_t> rx_tail_buffer_{};
  std::vector<uint8_t> rx_frame_log_buffer_{};
  std::vector<uint8_t> rx_contents_buffer_{};
  std::vector<uint8_t> request_payload_buffer_{};
  std::vector<uint8_t> request_response_buffer_{};
  std::string request_name_{};
  std::string request_send_log_{};
  bool request_send_log_ready_{false};
  uint8_t request_attempts_{0};
  bool request_active_{false};
  bool request_completed_{false};
  bool request_success_{false};
  bool request_hide_contents_{false};
  uint32_t request_next_send_ms_{0};
  uint32_t request_deadline_ms_{0};
  bool poll_due_{true};
  bool use_tou_for_poll_{false};
  bool static_bt01_needed_{false};
  bool static_et03_needed_{false};
  bool static_info_updated_this_poll_{false};
  bool tou_tier_block_fallback_needed_{false};
  bool warned_tier_block_skip_{false};
  uint8_t tou_tier_block_index_{0};
  uint8_t tou_tier_block_count_{0};
  uint16_t current_table28_bytes_{40};
  uint16_t current_table23_bytes_{8};
  uint32_t next_action_ms_{0};
  uint8_t wakeup_remaining_{0};
  std::array<uint8_t, 5> rx_header_{};
  size_t rx_header_pos_{0};
  size_t rx_payload_pos_{0};
  uint16_t rx_expected_length_{0};

  void process_session_state_(uint32_t now);
  void process_request_engine_(uint32_t now);
  void process_uart_(uint32_t now);
  void process_uart_byte_(uint8_t byte, uint32_t now);
  void reset_rx_parser_();
  void handle_rx_frame_complete_(uint32_t now);
  bool can_send_now_(uint32_t now) const;
  void send_request_frame_now_(uint32_t now);
  void schedule_request_retry_(uint32_t now, const char *reason);
  void fail_request_(const char *reason);
  bool begin_request_(const uint8_t *payload, size_t length, bool hide_contents, const char *name);
  StepResult run_request_step_(const uint8_t *payload, size_t length, bool hide_contents, const char *name);
  bool consume_request_response_ack_(ByteReader &reader, const char *context);
  bool parse_bt21_reply_(ByteReader &reader);
  bool parse_bt22_reply_(ByteReader &reader);
  bool parse_bt01_reply_(ByteReader &reader);
  bool parse_et03_reply_(ByteReader &reader);
  bool parse_tou_tier_block_reply_(ByteReader &reader, uint8_t tier);
  bool handle_table0_reply_(ByteReader &table_data);
  bool handle_table23_reply_(ByteReader &table_data, bool use_tou);
  bool handle_table28_reply_(ByteReader &table_data);
  bool has_tou_sensors_() const;
  bool has_static_info_sensors_() const;
  void reset_bt21_bt22_();
  bool publish_tou_from_sources_(const std::vector<int32_t> &summations);
  void schedule_init_backoff_(const char *reason);
  void record_unknown_signal_(uint8_t value, const char *context);
  void record_unknown_signal_(uint8_t value, uint8_t context);
  void maybe_finalize_unknown_sequence_();
  void finalize_unknown_sequence_();
  void maybe_log_health_(uint32_t now);
  const char *connect_state_label_() const;
  void feed_watchdog_();
};

}  // namespace osgp_meter
}  // namespace esphome
