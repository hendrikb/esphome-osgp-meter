import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor, uart
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ID,
    CONF_ICON,
    CONF_STATE_CLASS,
    CONF_UART_ID,
    CONF_UNIT_OF_MEASUREMENT,
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
CONF_MBUS = "mbus"
CONF_DEVICES = "devices"
CONF_SERIAL_NUMBER = "serial_number"
CONF_SLOT = "slot"
CONF_MEDIUM = "medium"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_DUMP_RECORDS = "dump_records"
CONF_TOTAL_VOLUME = "total_volume"
CONF_TOTAL_ENERGY = "total_energy"
CONF_VOLUME_FLOW_RATE = "volume_flow_rate"
CONF_THERMAL_POWER = "thermal_power"
CONF_FLOW_TEMPERATURE = "flow_temperature"
CONF_RETURN_TEMPERATURE = "return_temperature"
CONF_TEMPERATURE_DIFFERENCE = "temperature_difference"
CONF_LAST_READ = "last_read"
CONF_STATUS = "status"
CONF_RECORDS = "records"
CONF_DIF = "dif"
CONF_VIF = "vif"
CONF_VIFE = "vife"
CONF_FUNCTION = "function"
CONF_STORAGE = "storage"
CONF_TARIFF = "tariff"
CONF_SUBUNIT = "subunit"

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

MBUS_MEDIUMS = {
    "auto": 0,
    "gas": 3,
    "heat": 4,
    "hot_water": 6,
    "water": 7,
}

MBUS_FUNCTIONS = {
    "instantaneous": 0,
    "maximum": 1,
    "minimum": 2,
    "value_during_error": 3,
}


def AUTO_LOAD(config):
    mbus_has_text_sensor = any(
        CONF_LAST_READ in device or CONF_STATUS in device
        for device in config.get(CONF_MBUS, {}).get(CONF_DEVICES, [])
    )
    if any(key in config for key in TEXT_SENSOR_KEYS) or mbus_has_text_sensor:
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

SENSOR_SCHEMA_MBUS_VOLUME = sensor.sensor_schema(
    unit_of_measurement="m³",
    accuracy_decimals=3,
    state_class=STATE_CLASS_TOTAL_INCREASING,
)

SENSOR_SCHEMA_MBUS_ENERGY = sensor.sensor_schema(
    unit_of_measurement=UNIT_KILOWATT_HOURS,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    icon="mdi:radiator",
)

SENSOR_SCHEMA_MBUS_FLOW = sensor.sensor_schema(
    unit_of_measurement="m³/h",
    accuracy_decimals=3,
    device_class="volume_flow_rate",
    state_class=STATE_CLASS_MEASUREMENT,
)

SENSOR_SCHEMA_MBUS_POWER = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
)

SENSOR_SCHEMA_MBUS_TEMPERATURE = sensor.sensor_schema(
    unit_of_measurement="°C",
    accuracy_decimals=2,
    device_class="temperature",
    state_class=STATE_CLASS_MEASUREMENT,
)

SENSOR_SCHEMA_MBUS_TEMPERATURE_DIFFERENCE = sensor.sensor_schema(
    unit_of_measurement="K",
    accuracy_decimals=2,
    device_class="temperature_delta",
    state_class=STATE_CLASS_MEASUREMENT,
)


def _validate_mbus_serial(value):
    value = cv.string_strict(value)
    if len(value) != 8 or not value.isdigit():
        raise cv.Invalid("M-Bus serial_number must contain exactly 8 decimal digits")
    return value


def _mbus_record_metadata(config):
    vif = config[CONF_VIF] & 0x7F
    metadata = {}
    state_class = STATE_CLASS_MEASUREMENT
    accuracy_decimals = 3
    if 0x00 <= vif <= 0x07:
        metadata = {
            CONF_UNIT_OF_MEASUREMENT: "Wh",
            CONF_DEVICE_CLASS: "energy",
            CONF_ICON: "mdi:flash",
        }
        state_class = STATE_CLASS_TOTAL_INCREASING
        accuracy_decimals = max(0, 3 - vif)
    elif 0x08 <= vif <= 0x0F:
        metadata = {
            CONF_UNIT_OF_MEASUREMENT: "J",
            CONF_DEVICE_CLASS: "energy",
            CONF_ICON: "mdi:flash",
        }
        state_class = STATE_CLASS_TOTAL_INCREASING
        accuracy_decimals = max(0, 8 - vif)
    elif 0x10 <= vif <= 0x17:
        metadata = {
            CONF_UNIT_OF_MEASUREMENT: "m³",
            CONF_DEVICE_CLASS: "volume",
            CONF_ICON: "mdi:water",
        }
        state_class = STATE_CLASS_TOTAL_INCREASING
        accuracy_decimals = max(0, 0x16 - vif)
    elif 0x18 <= vif <= 0x1F:
        metadata = {CONF_UNIT_OF_MEASUREMENT: "kg", CONF_DEVICE_CLASS: "weight"}
        accuracy_decimals = max(0, 0x1B - vif)
    elif 0x20 <= vif <= 0x27:
        metadata = {CONF_UNIT_OF_MEASUREMENT: "s", CONF_DEVICE_CLASS: "duration"}
        accuracy_decimals = 0
    elif 0x28 <= vif <= 0x37:
        metadata = {CONF_UNIT_OF_MEASUREMENT: "W", CONF_DEVICE_CLASS: "power"}
        accuracy_decimals = max(0, 0x2B - vif) if vif <= 0x2F else 3
    elif 0x38 <= vif <= 0x4F:
        metadata = {CONF_UNIT_OF_MEASUREMENT: "m³/h", CONF_DEVICE_CLASS: "volume_flow_rate"}
        accuracy_decimals = max(0, 0x3E - vif) if vif <= 0x3F else 3
    elif 0x50 <= vif <= 0x57:
        metadata = {CONF_UNIT_OF_MEASUREMENT: "kg/h"}
        accuracy_decimals = max(0, 0x53 - vif)
    elif 0x58 <= vif <= 0x5F or 0x64 <= vif <= 0x67:
        metadata = {CONF_UNIT_OF_MEASUREMENT: "°C", CONF_DEVICE_CLASS: "temperature"}
        accuracy_decimals = max(0, (0x5B if vif <= 0x5B else 0x5F if vif <= 0x5F else 0x67) - vif)
    elif 0x60 <= vif <= 0x63:
        metadata = {CONF_UNIT_OF_MEASUREMENT: "K", CONF_DEVICE_CLASS: "temperature_delta"}
        accuracy_decimals = max(0, 0x63 - vif)
    elif 0x68 <= vif <= 0x6B:
        metadata = {CONF_UNIT_OF_MEASUREMENT: "bar", CONF_DEVICE_CLASS: "pressure"}
        accuracy_decimals = max(0, 0x6B - vif)

    for key, value in metadata.items():
        config.setdefault(key, value)
    config.setdefault(CONF_STATE_CLASS, state_class)
    config.setdefault(CONF_ACCURACY_DECIMALS, accuracy_decimals)
    return config


def _mbus_device_metadata(config):
    if (volume := config.get(CONF_TOTAL_VOLUME)) is not None:
        if config[CONF_MEDIUM] == "gas":
            volume.setdefault(CONF_DEVICE_CLASS, "gas")
            volume.setdefault(CONF_ICON, "mdi:meter-gas")
        else:
            volume.setdefault(CONF_DEVICE_CLASS, "water")
            volume.setdefault(CONF_ICON, "mdi:water")
    return config


def _validate_mbus_devices(devices):
    serials = [device[CONF_SERIAL_NUMBER] for device in devices]
    if len(serials) != len(set(serials)):
        raise cv.Invalid("M-Bus device serial_number values must be unique")
    slots = [device[CONF_SLOT] for device in devices if device[CONF_SLOT] != 0]
    if len(slots) != len(set(slots)):
        raise cv.Invalid("non-zero M-Bus slot hints must be unique")
    return devices


MBUS_RECORD_SCHEMA = cv.All(
    sensor.sensor_schema().extend(
        {
            cv.Required(CONF_DIF): cv.hex_uint8_t,
            cv.Required(CONF_VIF): cv.hex_uint8_t,
            cv.Optional(CONF_VIFE, default=[]): cv.ensure_list(cv.hex_uint8_t),
            cv.Optional(CONF_FUNCTION, default="instantaneous"): cv.one_of(*MBUS_FUNCTIONS, lower=True),
            cv.Optional(CONF_STORAGE): cv.int_range(min=0, max=0xFFFFFFFF),
            cv.Optional(CONF_TARIFF): cv.int_range(min=0, max=0xFFFFFFFF),
            cv.Optional(CONF_SUBUNIT): cv.int_range(min=0, max=0xFFFFFFFF),
        }
    ),
    _mbus_record_metadata,
)

MBUS_DEVICE_SCHEMA = cv.All(
    cv.Schema(
        {
        cv.Required(CONF_SERIAL_NUMBER): _validate_mbus_serial,
        cv.Optional(CONF_SLOT, default=0): cv.int_range(min=0, max=4),
        cv.Optional(CONF_MEDIUM, default="auto"): cv.one_of(*MBUS_MEDIUMS, lower=True),
        cv.Optional(CONF_TOTAL_VOLUME): SENSOR_SCHEMA_MBUS_VOLUME,
        cv.Optional(CONF_TOTAL_ENERGY): SENSOR_SCHEMA_MBUS_ENERGY,
        cv.Optional(CONF_VOLUME_FLOW_RATE): SENSOR_SCHEMA_MBUS_FLOW,
        cv.Optional(CONF_THERMAL_POWER): SENSOR_SCHEMA_MBUS_POWER,
        cv.Optional(CONF_FLOW_TEMPERATURE): SENSOR_SCHEMA_MBUS_TEMPERATURE,
        cv.Optional(CONF_RETURN_TEMPERATURE): SENSOR_SCHEMA_MBUS_TEMPERATURE,
        cv.Optional(CONF_TEMPERATURE_DIFFERENCE): SENSOR_SCHEMA_MBUS_TEMPERATURE_DIFFERENCE,
        cv.Optional(CONF_LAST_READ): text_sensor.text_sensor_schema(icon="mdi:clock-outline"),
        cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(icon="mdi:connection"),
        cv.Optional(CONF_RECORDS, default=[]): cv.ensure_list(MBUS_RECORD_SCHEMA),
        }
    ),
    _mbus_device_metadata,
)

MBUS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_DUMP_RECORDS, default=False): cv.boolean,
        cv.Required(CONF_DEVICES): cv.All(
            cv.ensure_list(MBUS_DEVICE_SCHEMA), cv.Length(min=1, max=4), _validate_mbus_devices
        ),
    }
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
        cv.Optional(CONF_MBUS): MBUS_SCHEMA,
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

    if (mbus_config := config.get(CONF_MBUS)) is not None:
        cg.add(var.set_mbus_update_interval(mbus_config[CONF_UPDATE_INTERVAL].total_milliseconds))
        cg.add(var.set_mbus_dump_records(mbus_config[CONF_DUMP_RECORDS]))
        for device_index, device_config in enumerate(mbus_config[CONF_DEVICES]):
            cg.add(
                var.add_mbus_device(
                    device_config[CONF_SERIAL_NUMBER],
                    device_config[CONF_SLOT],
                    MBUS_MEDIUMS[device_config[CONF_MEDIUM]],
                )
            )
            convenience_setters = (
                (CONF_TOTAL_VOLUME, var.set_mbus_total_volume_sensor),
                (CONF_TOTAL_ENERGY, var.set_mbus_total_energy_sensor),
                (CONF_VOLUME_FLOW_RATE, var.set_mbus_volume_flow_rate_sensor),
                (CONF_THERMAL_POWER, var.set_mbus_thermal_power_sensor),
                (CONF_FLOW_TEMPERATURE, var.set_mbus_flow_temperature_sensor),
                (CONF_RETURN_TEMPERATURE, var.set_mbus_return_temperature_sensor),
                (CONF_TEMPERATURE_DIFFERENCE, var.set_mbus_temperature_difference_sensor),
            )
            for key, setter in convenience_setters:
                if (sensor_config := device_config.get(key)) is not None:
                    sens = await sensor.new_sensor(sensor_config)
                    cg.add(setter(device_index, sens))
            if (text_config := device_config.get(CONF_LAST_READ)) is not None:
                sens = await text_sensor.new_text_sensor(text_config)
                cg.add(var.set_mbus_last_read_text_sensor(device_index, sens))
            if (text_config := device_config.get(CONF_STATUS)) is not None:
                sens = await text_sensor.new_text_sensor(text_config)
                cg.add(var.set_mbus_status_text_sensor(device_index, sens))
            for record_index, record_config in enumerate(device_config[CONF_RECORDS]):
                sens = await sensor.new_sensor(record_config)
                cg.add(
                    var.add_mbus_record_sensor(
                        device_index,
                        sens,
                        record_config[CONF_DIF],
                        record_config[CONF_VIF],
                        MBUS_FUNCTIONS[record_config[CONF_FUNCTION]],
                        record_config.get(CONF_STORAGE, -1),
                        record_config.get(CONF_TARIFF, -1),
                        record_config.get(CONF_SUBUNIT, -1),
                    )
                )
                for vife in record_config[CONF_VIFE]:
                    cg.add(var.add_mbus_record_vife(device_index, record_index, vife))

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
