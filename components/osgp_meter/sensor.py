import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_PASSWORD,
    CONF_USERNAME,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

from . import OSGPMeter

DEPENDENCIES = ["uart"]

CONF_USER_ID = "user_id"
CONF_REFRESH_INTERVAL = "refresh_interval"
CONF_LOGOFF_INTERVAL = "logoff_interval"
CONF_LOG_RAW = "log_raw"
CONF_STATIC_INFO_INTERVAL = "static_info_interval"
CONF_POLL_JITTER = "poll_jitter"
CONF_HEALTH_LOG_INTERVAL = "health_log_interval"

CONF_FWD_ACTIVE_ENERGY = "fwd_active_energy"
CONF_REV_ACTIVE_ENERGY = "rev_active_energy"
CONF_FWD_ACTIVE_POWER = "fwd_active_power"
CONF_REV_ACTIVE_POWER = "rev_active_power"
CONF_IMPORT_REACTIVE_VAR = "import_reactive_var"
CONF_EXPORT_REACTIVE_VAR = "export_reactive_var"
CONF_L1_CURRENT = "l1_current"
CONF_L2_CURRENT = "l2_current"
CONF_L3_CURRENT = "l3_current"
CONF_L1_VOLTAGE = "l1_voltage"
CONF_L2_VOLTAGE = "l2_voltage"
CONF_L3_VOLTAGE = "l3_voltage"
CONF_POWER_FACTOR_L1 = "power_factor_l1"
CONF_POWER_FACTOR_L2 = "power_factor_l2"
CONF_POWER_FACTOR_L3 = "power_factor_l3"
CONF_FREQUENCY = "frequency"
CONF_TOU_TIER1_FWD_ACTIVE_ENERGY = "tou_tier1_fwd_active_energy"
CONF_TOU_TIER2_FWD_ACTIVE_ENERGY = "tou_tier2_fwd_active_energy"
CONF_TOU_TIER3_FWD_ACTIVE_ENERGY = "tou_tier3_fwd_active_energy"
CONF_TOU_TIER4_FWD_ACTIVE_ENERGY = "tou_tier4_fwd_active_energy"
CONF_TOU_TIER1_REV_ACTIVE_ENERGY = "tou_tier1_rev_active_energy"
CONF_TOU_TIER2_REV_ACTIVE_ENERGY = "tou_tier2_rev_active_energy"
CONF_TOU_TIER3_REV_ACTIVE_ENERGY = "tou_tier3_rev_active_energy"
CONF_TOU_TIER4_REV_ACTIVE_ENERGY = "tou_tier4_rev_active_energy"
CONF_MANUFACTURER = "manufacturer"
CONF_MODEL = "model"
CONF_HARDWARE_VERSION = "hardware_version"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_MANUFACTURER_SERIAL = "manufacturer_serial"
CONF_UTILITY_SERIAL = "utility_serial"
CONF_UNKNOWN_SIGNAL_COUNT = "unknown_signal_count"
CONF_UNKNOWN_PRE_SEND_COUNT = "unknown_pre_send_count"
CONF_UNKNOWN_START_SCAN_COUNT = "unknown_start_scan_count"
CONF_UNKNOWN_RESPONSE_COUNT = "unknown_response_count"
CONF_UNKNOWN_SEQUENCE_COUNT = "unknown_sequence_count"
CONF_UNKNOWN_SEQUENCE_LAST = "unknown_sequence_last"
CONF_RESET_REASON = "reset_reason"

TEXT_SENSOR_KEYS = (
    CONF_MANUFACTURER,
    CONF_MODEL,
    CONF_HARDWARE_VERSION,
    CONF_FIRMWARE_VERSION,
    CONF_MANUFACTURER_SERIAL,
    CONF_UTILITY_SERIAL,
    CONF_UNKNOWN_SEQUENCE_LAST,
    CONF_RESET_REASON,
)


def AUTO_LOAD(config):
    if any(key in config for key in TEXT_SENSOR_KEYS):
        return ["text_sensor"]
    return []


SENSOR_SCHEMA_ENERGY = sensor.sensor_schema(
    unit_of_measurement=UNIT_KILOWATT_HOURS,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
)

SENSOR_SCHEMA_POWER = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
)

SENSOR_SCHEMA_REACTIVE_POWER = sensor.sensor_schema(
    unit_of_measurement="var",
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

SENSOR_SCHEMA_CURRENT = sensor.sensor_schema(
    unit_of_measurement=UNIT_AMPERE,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
)

SENSOR_SCHEMA_VOLTAGE = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)

SENSOR_SCHEMA_POWER_FACTOR = sensor.sensor_schema(
    accuracy_decimals=3,
    state_class=STATE_CLASS_MEASUREMENT,
)

SENSOR_SCHEMA_FREQUENCY = sensor.sensor_schema(
    unit_of_measurement=UNIT_HERTZ,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_FREQUENCY,
    state_class=STATE_CLASS_MEASUREMENT,
)

SENSOR_SCHEMA_COUNTER = sensor.sensor_schema(
    unit_of_measurement="count",
    accuracy_decimals=0,
    state_class=STATE_CLASS_TOTAL_INCREASING,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OSGPMeter),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_USER_ID, default=1): cv.int_range(min=0, max=65535),
        cv.Optional(CONF_USERNAME, default="OpenHAB"): cv.string_strict,
        cv.Required(CONF_PASSWORD): cv.string_strict,
        cv.Optional(CONF_REFRESH_INTERVAL, default="2s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_LOGOFF_INTERVAL, default="540s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_LOG_RAW, default=True): cv.boolean,
        cv.Optional(CONF_STATIC_INFO_INTERVAL, default="1h"): cv.positive_time_period_seconds,
        cv.Optional(CONF_POLL_JITTER, default="0ms"): cv.positive_time_period,
        cv.Optional(CONF_HEALTH_LOG_INTERVAL, default="60s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_FWD_ACTIVE_ENERGY): SENSOR_SCHEMA_ENERGY,
        cv.Optional(CONF_REV_ACTIVE_ENERGY): SENSOR_SCHEMA_ENERGY,
        cv.Optional(CONF_FWD_ACTIVE_POWER): SENSOR_SCHEMA_POWER,
        cv.Optional(CONF_REV_ACTIVE_POWER): SENSOR_SCHEMA_POWER,
        cv.Optional(CONF_IMPORT_REACTIVE_VAR): SENSOR_SCHEMA_REACTIVE_POWER,
        cv.Optional(CONF_EXPORT_REACTIVE_VAR): SENSOR_SCHEMA_REACTIVE_POWER,
        cv.Optional(CONF_L1_CURRENT): SENSOR_SCHEMA_CURRENT,
        cv.Optional(CONF_L2_CURRENT): SENSOR_SCHEMA_CURRENT,
        cv.Optional(CONF_L3_CURRENT): SENSOR_SCHEMA_CURRENT,
        cv.Optional(CONF_L1_VOLTAGE): SENSOR_SCHEMA_VOLTAGE,
        cv.Optional(CONF_L2_VOLTAGE): SENSOR_SCHEMA_VOLTAGE,
        cv.Optional(CONF_L3_VOLTAGE): SENSOR_SCHEMA_VOLTAGE,
        cv.Optional(CONF_POWER_FACTOR_L1): SENSOR_SCHEMA_POWER_FACTOR,
        cv.Optional(CONF_POWER_FACTOR_L2): SENSOR_SCHEMA_POWER_FACTOR,
        cv.Optional(CONF_POWER_FACTOR_L3): SENSOR_SCHEMA_POWER_FACTOR,
        cv.Optional(CONF_FREQUENCY): SENSOR_SCHEMA_FREQUENCY,
        cv.Optional(CONF_TOU_TIER1_FWD_ACTIVE_ENERGY): SENSOR_SCHEMA_ENERGY,
        cv.Optional(CONF_TOU_TIER2_FWD_ACTIVE_ENERGY): SENSOR_SCHEMA_ENERGY,
        cv.Optional(CONF_TOU_TIER3_FWD_ACTIVE_ENERGY): SENSOR_SCHEMA_ENERGY,
        cv.Optional(CONF_TOU_TIER4_FWD_ACTIVE_ENERGY): SENSOR_SCHEMA_ENERGY,
        cv.Optional(CONF_TOU_TIER1_REV_ACTIVE_ENERGY): SENSOR_SCHEMA_ENERGY,
        cv.Optional(CONF_TOU_TIER2_REV_ACTIVE_ENERGY): SENSOR_SCHEMA_ENERGY,
        cv.Optional(CONF_TOU_TIER3_REV_ACTIVE_ENERGY): SENSOR_SCHEMA_ENERGY,
        cv.Optional(CONF_TOU_TIER4_REV_ACTIVE_ENERGY): SENSOR_SCHEMA_ENERGY,
        cv.Optional(CONF_MANUFACTURER): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_MODEL): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_HARDWARE_VERSION): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_FIRMWARE_VERSION): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_MANUFACTURER_SERIAL): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_UTILITY_SERIAL): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_UNKNOWN_SIGNAL_COUNT): SENSOR_SCHEMA_COUNTER,
        cv.Optional(CONF_UNKNOWN_PRE_SEND_COUNT): SENSOR_SCHEMA_COUNTER,
        cv.Optional(CONF_UNKNOWN_START_SCAN_COUNT): SENSOR_SCHEMA_COUNTER,
        cv.Optional(CONF_UNKNOWN_RESPONSE_COUNT): SENSOR_SCHEMA_COUNTER,
        cv.Optional(CONF_UNKNOWN_SEQUENCE_COUNT): SENSOR_SCHEMA_COUNTER,
        cv.Optional(CONF_UNKNOWN_SEQUENCE_LAST): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_RESET_REASON): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_user_id(config[CONF_USER_ID]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_refresh_interval(config[CONF_REFRESH_INTERVAL].total_milliseconds))
    cg.add(var.set_logoff_interval(config[CONF_LOGOFF_INTERVAL].total_milliseconds))
    cg.add(var.set_log_raw(config[CONF_LOG_RAW]))
    cg.add(var.set_static_info_interval(config[CONF_STATIC_INFO_INTERVAL].total_milliseconds))
    cg.add(var.set_poll_jitter(config[CONF_POLL_JITTER].total_milliseconds))
    cg.add(var.set_health_log_interval(config[CONF_HEALTH_LOG_INTERVAL].total_milliseconds))

    if (sensor_config := config.get(CONF_FWD_ACTIVE_ENERGY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_fwd_active_energy_sensor(sens))
    if (sensor_config := config.get(CONF_REV_ACTIVE_ENERGY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_rev_active_energy_sensor(sens))
    if (sensor_config := config.get(CONF_FWD_ACTIVE_POWER)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_fwd_active_power_sensor(sens))
    if (sensor_config := config.get(CONF_REV_ACTIVE_POWER)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_rev_active_power_sensor(sens))
    if (sensor_config := config.get(CONF_IMPORT_REACTIVE_VAR)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_import_reactive_var_sensor(sens))
    if (sensor_config := config.get(CONF_EXPORT_REACTIVE_VAR)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_export_reactive_var_sensor(sens))
    if (sensor_config := config.get(CONF_L1_CURRENT)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_l1_current_sensor(sens))
    if (sensor_config := config.get(CONF_L2_CURRENT)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_l2_current_sensor(sens))
    if (sensor_config := config.get(CONF_L3_CURRENT)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_l3_current_sensor(sens))
    if (sensor_config := config.get(CONF_L1_VOLTAGE)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_l1_voltage_sensor(sens))
    if (sensor_config := config.get(CONF_L2_VOLTAGE)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_l2_voltage_sensor(sens))
    if (sensor_config := config.get(CONF_L3_VOLTAGE)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_l3_voltage_sensor(sens))
    if (sensor_config := config.get(CONF_POWER_FACTOR_L1)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_power_factor_l1_sensor(sens))
    if (sensor_config := config.get(CONF_POWER_FACTOR_L2)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_power_factor_l2_sensor(sens))
    if (sensor_config := config.get(CONF_POWER_FACTOR_L3)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_power_factor_l3_sensor(sens))
    if (sensor_config := config.get(CONF_FREQUENCY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_frequency_sensor(sens))
    if (sensor_config := config.get(CONF_TOU_TIER1_FWD_ACTIVE_ENERGY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_tou_fwd_active_energy_sensor(0, sens))
    if (sensor_config := config.get(CONF_TOU_TIER2_FWD_ACTIVE_ENERGY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_tou_fwd_active_energy_sensor(1, sens))
    if (sensor_config := config.get(CONF_TOU_TIER3_FWD_ACTIVE_ENERGY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_tou_fwd_active_energy_sensor(2, sens))
    if (sensor_config := config.get(CONF_TOU_TIER4_FWD_ACTIVE_ENERGY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_tou_fwd_active_energy_sensor(3, sens))
    if (sensor_config := config.get(CONF_TOU_TIER1_REV_ACTIVE_ENERGY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_tou_rev_active_energy_sensor(0, sens))
    if (sensor_config := config.get(CONF_TOU_TIER2_REV_ACTIVE_ENERGY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_tou_rev_active_energy_sensor(1, sens))
    if (sensor_config := config.get(CONF_TOU_TIER3_REV_ACTIVE_ENERGY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_tou_rev_active_energy_sensor(2, sens))
    if (sensor_config := config.get(CONF_TOU_TIER4_REV_ACTIVE_ENERGY)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_tou_rev_active_energy_sensor(3, sens))
    if (text_config := config.get(CONF_MANUFACTURER)) is not None:
        sens = await text_sensor.new_text_sensor(text_config)
        cg.add(var.set_manufacturer_text_sensor(sens))
    if (text_config := config.get(CONF_MODEL)) is not None:
        sens = await text_sensor.new_text_sensor(text_config)
        cg.add(var.set_model_text_sensor(sens))
    if (text_config := config.get(CONF_HARDWARE_VERSION)) is not None:
        sens = await text_sensor.new_text_sensor(text_config)
        cg.add(var.set_hardware_version_text_sensor(sens))
    if (text_config := config.get(CONF_FIRMWARE_VERSION)) is not None:
        sens = await text_sensor.new_text_sensor(text_config)
        cg.add(var.set_firmware_version_text_sensor(sens))
    if (text_config := config.get(CONF_MANUFACTURER_SERIAL)) is not None:
        sens = await text_sensor.new_text_sensor(text_config)
        cg.add(var.set_manufacturer_serial_text_sensor(sens))
    if (text_config := config.get(CONF_UTILITY_SERIAL)) is not None:
        sens = await text_sensor.new_text_sensor(text_config)
        cg.add(var.set_utility_serial_text_sensor(sens))
    if (sensor_config := config.get(CONF_UNKNOWN_SIGNAL_COUNT)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_unknown_signal_count_sensor(sens))
    if (sensor_config := config.get(CONF_UNKNOWN_PRE_SEND_COUNT)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_unknown_pre_send_count_sensor(sens))
    if (sensor_config := config.get(CONF_UNKNOWN_START_SCAN_COUNT)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_unknown_start_scan_count_sensor(sens))
    if (sensor_config := config.get(CONF_UNKNOWN_RESPONSE_COUNT)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_unknown_response_count_sensor(sens))
    if (sensor_config := config.get(CONF_UNKNOWN_SEQUENCE_COUNT)) is not None:
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_unknown_sequence_count_sensor(sens))
    if (text_config := config.get(CONF_UNKNOWN_SEQUENCE_LAST)) is not None:
        sens = await text_sensor.new_text_sensor(text_config)
        cg.add(var.set_unknown_sequence_last_text_sensor(sens))
    if (text_config := config.get(CONF_RESET_REASON)) is not None:
        sens = await text_sensor.new_text_sensor(text_config)
        cg.add(var.set_reset_reason_text_sensor(sens))
