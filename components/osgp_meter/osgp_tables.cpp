#include "osgp_meter.h"

#include <algorithm>

#include "osgp_protocol_constants.h"
#include "osgp_utils.h"
#include "esphome/core/log.h"

namespace esphome {
namespace osgp_meter {

static const char *const TAG = "osgp_meter";

bool OSGPMeter::handle_table0_reply_(ByteReader &table_data) {
  if (table_data.limit() < 10) {
    ESP_LOGW(TAG, "Table 0 reply too short: %u", static_cast<unsigned>(table_data.limit()));
    return false;
  }

  uint16_t table_length = table_data.get_u16_be();
  uint8_t current = table_data.get_u8();
  this->byte_order_little_ = (current & 0x01) == 0;
  uint8_t char_format = (current >> 1) & 0x07;
  current = table_data.get_u8();
  uint8_t time_format = current & 0x07;
  uint8_t data_access_method = (current >> 3) & 0x03;
  bool identification_bcd = ((current >> 4) & 0x01) != 0;
  current = table_data.get_u8();

  std::string manufacturer;
  for (uint8_t i = 0; i < 4; i++) {
    manufacturer.push_back(static_cast<char>(table_data.get_u8()));
  }
  uint8_t nameplate_type = table_data.get_u8();
  uint8_t default_set_used = table_data.get_u8();
  uint8_t procedure_parameter_length = table_data.get_u8();
  uint8_t response_data_length = table_data.get_u8();
  uint8_t standard_version = table_data.get_u8();
  uint8_t standard_revision = table_data.get_u8();

  ESP_LOGI(TAG,
           "Table0 length=%u byte_order=%s char_format=%u time_format=%u data_access=%u ident_bcd=%s manufacturer=%s "
           "nameplate_type=%u default_set=%u procedure_len=%u response_len=%u std_ver=%u std_rev=%u",
           table_length, this->byte_order_little_ ? "little" : "big", char_format, time_format, data_access_method,
           identification_bcd ? "true" : "false", manufacturer.c_str(), nameplate_type, default_set_used,
           procedure_parameter_length, response_data_length, standard_version, standard_revision);

  return true;
}

bool OSGPMeter::handle_table23_reply_(ByteReader &table_data, bool use_tou) {
  if (table_data.limit() < 2 + 8) {
    ESP_LOGW(TAG, "Table 23 reply too short: %u", static_cast<unsigned>(table_data.limit()));
    return false;
  }

  uint16_t table_length = table_data.get_u16_be();
  table_data.set_order_little(this->byte_order_little_);

  if (!use_tou || this->num_summations_ == 0 || this->summation_source_ids_.empty()) {
    int32_t fwd_wh = table_data.get_i32();
    int32_t rev_wh = table_data.get_i32();
    utils::publish_state_if_present(this->fwd_active_energy_sensor_, static_cast<float>(fwd_wh) / 1000.0f);
    utils::publish_state_if_present(this->rev_active_energy_sensor_, static_cast<float>(rev_wh) / 1000.0f);
    if (table_length != 0x08) {
      ESP_LOGD(TAG, "Table23 unexpected length %u", table_length);
    }
    ESP_LOGI(TAG, "Table23 Fwd %.3f kWh Rev %.3f kWh", static_cast<float>(fwd_wh) / 1000.0f,
             static_cast<float>(rev_wh) / 1000.0f);
    return true;
  }

  if (this->demand_reset_counter_) {
    if (table_data.limit() < 2 + 1 + 4 * this->num_summations_) {
      ESP_LOGW(TAG, "Table23 summations reply too short: %u", static_cast<unsigned>(table_data.limit()));
      return false;
    }
    table_data.get_u8();
  } else if (table_data.limit() < 2 + 4 * this->num_summations_) {
    ESP_LOGW(TAG, "Table23 summations reply too short: %u", static_cast<unsigned>(table_data.limit()));
    return false;
  }

  std::vector<int32_t> summations;
  summations.reserve(this->num_summations_);
  for (uint8_t i = 0; i < this->num_summations_; i++) {
    summations.push_back(table_data.get_i32());
  }

  int idx_fwd = this->summation_index_fwd_ >= 0 ? this->summation_index_fwd_ : 0;
  int idx_rev = this->summation_index_rev_ >= 0 ? this->summation_index_rev_ : 1;
  float fwd_kwh = 0.0f;
  float rev_kwh = 0.0f;
  if (idx_fwd >= 0 && idx_fwd < static_cast<int>(summations.size())) {
    int32_t fwd_wh = summations[static_cast<size_t>(idx_fwd)];
    utils::publish_state_if_present(this->fwd_active_energy_sensor_, static_cast<float>(fwd_wh) / 1000.0f);
    fwd_kwh = static_cast<float>(fwd_wh) / 1000.0f;
  }
  if (idx_rev >= 0 && idx_rev < static_cast<int>(summations.size())) {
    int32_t rev_wh = summations[static_cast<size_t>(idx_rev)];
    utils::publish_state_if_present(this->rev_active_energy_sensor_, static_cast<float>(rev_wh) / 1000.0f);
    rev_kwh = static_cast<float>(rev_wh) / 1000.0f;
  }

  bool published_from_sources = this->publish_tou_from_sources_(summations);
  if (!published_from_sources) {
    if (this->has_tou_sensors_() && this->num_tiers_ > 0 && this->num_summations_ > 0) {
      if (this->num_demands_ != 0 || this->num_coincident_ != 0) {
        if (!this->warned_tier_block_skip_) {
          ESP_LOGW(TAG, "TOU tier blocks skipped because demand/coincident sizes are non-zero (demands=%u coincident=%u)",
                   this->num_demands_, this->num_coincident_);
          this->warned_tier_block_skip_ = true;
        }
      } else {
        this->tou_tier_block_fallback_needed_ = true;
        this->tou_tier_block_index_ = 0;
        this->tou_tier_block_count_ = std::min(this->num_tiers_, MAX_TOU_TIERS);
        ESP_LOGD(TAG, "BT22 does not include tier-specific source IDs; scheduling BT23 tier-block fallback");
      }
    }
  }

  if (table_length != 0x08 && this->num_summations_ == 2) {
    ESP_LOGD(TAG, "Table23 unexpected length %u", table_length);
  }
  ESP_LOGI(TAG, "Table23 Fwd %.3f kWh Rev %.3f kWh", fwd_kwh, rev_kwh);
  return true;
}

bool OSGPMeter::handle_table28_reply_(ByteReader &table_data) {
  if (table_data.limit() < 1 + 2 + 40) {
    ESP_LOGW(TAG, "Table 28 reply too short: %u", static_cast<unsigned>(table_data.limit()));
    return false;
  }

  uint16_t table_length = table_data.get_u16_be();
  table_data.set_order_little(this->byte_order_little_);

  int32_t fwd_w = table_data.get_i32();
  int32_t rev_w = table_data.get_i32();
  int32_t import_var = table_data.get_i32();
  int32_t export_var = table_data.get_i32();
  int32_t l1_ma = table_data.get_i32();
  int32_t l2_ma = table_data.get_i32();
  int32_t l3_ma = table_data.get_i32();
  int32_t l1_mv = table_data.get_i32();
  int32_t l2_mv = table_data.get_i32();
  int32_t l3_mv = table_data.get_i32();

  int32_t pf_l1 = 0;
  int32_t pf_l2 = 0;
  int32_t pf_l3 = 0;
  int32_t freq_mhz = 0;
  bool have_pf_l1 = false;
  bool have_pf_l2 = false;
  bool have_pf_l3 = false;
  bool have_freq = false;

  const bool want_pf_l1 = this->power_factor_l1_sensor_ != nullptr;
  const bool want_pf_l2 = this->power_factor_l2_sensor_ != nullptr;
  const bool want_pf_l3 = this->power_factor_l3_sensor_ != nullptr;
  const bool want_freq = this->frequency_sensor_ != nullptr;
  uint8_t max_index_needed = 9;
  if (want_pf_l2 || want_pf_l3) {
    max_index_needed = 14;
  } else if (want_pf_l1 || want_freq) {
    max_index_needed = 11;
  }
  if (max_index_needed > 9) {
    const size_t remaining_bytes = table_data.limit() - table_data.position();
    const uint8_t extra_available = static_cast<uint8_t>(remaining_bytes / 4U);
    const uint8_t extra_needed = static_cast<uint8_t>(max_index_needed - 9);
    const uint8_t extra_to_read = std::min(extra_available, extra_needed);
    for (uint8_t i = 0; i < extra_to_read; i++) {
      int32_t value = table_data.get_i32();
      const uint8_t index = static_cast<uint8_t>(10 + i);
      switch (index) {
        case 10:
          pf_l1 = value;
          have_pf_l1 = true;
          break;
        case 11:
          freq_mhz = value;
          have_freq = true;
          break;
        case 12:
          break;
        case 13:
          pf_l2 = value;
          have_pf_l2 = true;
          break;
        case 14:
          pf_l3 = value;
          have_pf_l3 = true;
          break;
        default:
          break;
      }
    }
    if (extra_to_read < extra_needed) {
      ESP_LOGD(TAG, "Table28 missing extended values: need %u extra, have %u", extra_needed, extra_available);
    }
  }

  utils::publish_state_if_present(this->fwd_active_power_sensor_, static_cast<float>(fwd_w));
  utils::publish_state_if_present(this->rev_active_power_sensor_, static_cast<float>(rev_w));
  utils::publish_state_if_present(this->import_reactive_var_sensor_, static_cast<float>(import_var));
  utils::publish_state_if_present(this->export_reactive_var_sensor_, static_cast<float>(export_var));
  utils::publish_state_if_present(this->l1_current_sensor_, static_cast<float>(l1_ma) / 1000.0f);
  utils::publish_state_if_present(this->l2_current_sensor_, static_cast<float>(l2_ma) / 1000.0f);
  utils::publish_state_if_present(this->l3_current_sensor_, static_cast<float>(l3_ma) / 1000.0f);
  utils::publish_state_if_present(this->l1_voltage_sensor_, static_cast<float>(l1_mv) / 1000.0f);
  utils::publish_state_if_present(this->l2_voltage_sensor_, static_cast<float>(l2_mv) / 1000.0f);
  utils::publish_state_if_present(this->l3_voltage_sensor_, static_cast<float>(l3_mv) / 1000.0f);
  if (have_pf_l1) {
    utils::publish_state_if_present(this->power_factor_l1_sensor_, static_cast<float>(pf_l1) / 1000.0f);
  }
  if (have_pf_l2) {
    utils::publish_state_if_present(this->power_factor_l2_sensor_, static_cast<float>(pf_l2) / 1000.0f);
  }
  if (have_pf_l3) {
    utils::publish_state_if_present(this->power_factor_l3_sensor_, static_cast<float>(pf_l3) / 1000.0f);
  }
  if (have_freq) {
    utils::publish_state_if_present(this->frequency_sensor_, static_cast<float>(freq_mhz) / 1000.0f);
  }

  if (table_length < 0x28) {
    ESP_LOGD(TAG, "Table28 unexpected length %u", table_length);
  }
  ESP_LOGI(TAG,
           "Table28 P_fwd=%dW P_rev=%dW Q_imp=%dvar Q_exp=%dvar I=%.3f/%.3f/%.3fA V=%.3f/%.3f/%.3fV", fwd_w, rev_w,
           import_var, export_var, static_cast<float>(l1_ma) / 1000.0f, static_cast<float>(l2_ma) / 1000.0f,
           static_cast<float>(l3_ma) / 1000.0f, static_cast<float>(l1_mv) / 1000.0f,
           static_cast<float>(l2_mv) / 1000.0f, static_cast<float>(l3_mv) / 1000.0f);
  return true;
}

bool OSGPMeter::publish_tou_from_sources_(const std::vector<int32_t> &summations) {
  bool any_published = false;
  for (uint8_t tier = 0; tier < MAX_TOU_TIERS; tier++) {
    int8_t idx_fwd = this->tier_fwd_source_index_[tier];
    int8_t idx_rev = this->tier_rev_source_index_[tier];
    if (this->tou_fwd_active_energy_sensors_[tier] != nullptr && idx_fwd >= 0 &&
        idx_fwd < static_cast<int8_t>(summations.size())) {
      int32_t value = summations[static_cast<size_t>(idx_fwd)];
      utils::publish_state_if_present(this->tou_fwd_active_energy_sensors_[tier], static_cast<float>(value) / 1000.0f);
      any_published = true;
    }
    if (this->tou_rev_active_energy_sensors_[tier] != nullptr && idx_rev >= 0 &&
        idx_rev < static_cast<int8_t>(summations.size())) {
      int32_t value = summations[static_cast<size_t>(idx_rev)];
      utils::publish_state_if_present(this->tou_rev_active_energy_sensors_[tier], static_cast<float>(value) / 1000.0f);
      any_published = true;
    }
  }

  if (!any_published && this->has_tou_sensors_() && !this->warned_missing_tou_sources_) {
    ESP_LOGD(TAG, "BT22 does not include tier-specific source IDs; trying tier blocks if possible");
    this->warned_missing_tou_sources_ = true;
  }

  return any_published;
}

}  // namespace osgp_meter
}  // namespace esphome
