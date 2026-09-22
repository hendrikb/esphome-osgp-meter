# ESPHome OSGP Meter Component

`osgp_meter` is an [ESPHome](https://esphome.io) external component for smart meters that expose an
optical infrared port and speak [Open Smart Grid Protocol (OSGP)](https://www.osgp.org) ([Wikipedia](https://en.wikipedia.org/wiki/Open_Smart_Grid_Protocol)) instead of the
often seen SML protocol (which is already [well supported in ESPHome](https://esphome.io/components/sml/)).
It has been tested on [Networked Energy Services (NES) / Echelon style meters](https://www.networkedenergy.com/en/products/smart-meters) with a [Hichi WLAN v2 infrared device](https://sites.google.com/view/hichi-lesekopf/wifi-v2) and it exposes lots of useful information on your power usage, consumption, voltages and many other grid-specific data points. You can also monitor energy returned to the grid. Via the `mbus:` configuration setting, you can additionally track [Meter-Bus](https://en.wikipedia.org/wiki/Meter-Bus)-connected meters, such as water consumption or heat.

> You'll most likely require a read-only key (RK) that -- at least in Germany -- is handed out by some power grid companies that install these smart meters.

It is intended to be dropped into ESPHome via an `external_components`
configuration entry. See the [electricity example](example_nes_meter.yaml) or
the [electricity, water, and heat example](example_nes_meter_mbus.yaml).

## Why?

Reading your smart meter data via ESPHome allows you to very easily make energy data available in home automation
systems like [Home Assistant](https://home-assistant.io), especially for [Energy Management](https://www.home-assistant.io/home-energy-management/#monitor_usage) there.
See documentation how to integrate ESPHome devices like yours. You maybe want to configure an [`api:`](https://esphome.io/components/api/) section.

## What You Get

- OSGP handshake, authentication, and table reads over UART-connected IR heads
- Forward / reverse energy counters
- Instantaneous power, current, voltage, power factor, and frequency
- Static meter information such as manufacturer, model, firmware, and serials
- Scheduled M-Bus water and heat-meter readings exposed through the electricity meter
- Optional diagnostics for unknown frames and reset reasons

## Installation

[ESPHome](https://esphome.io) will compile a firmware binary for you that you can upload to any device capable of running it  (e.g. ESP32-C3).
Of course, you need to have a proper Infrared LED setup wired onto your device.

If you don't want to get your hands dirty and solder yourself,
you can purchase one of many pre-made devices of this kind. This software is confirmed to work with "Hichi WIFI v2" -- don't worry: *that* specific device ships with Tasmota
but you will replace Tasmota with this compiled ESPHome binary.

Put the following directive into your ESPhome device configuration to load the component:

```yaml
external_components:
  - source: github://hendrikb/esphome-osgp-meter@main
    components: [osgp_meter]
```

Right after loading the component you can continue configuring it to your needs.

Make sure to have all other required settings configured in your ESPHome YAML (e.g. Wifi, Home Assistant API, device family etc.)

## Minimal OSGP Component Configuration

See [example_nes_meter.yaml](example_nes_meter.yaml) for a full example. You will need to configure your ESPHome-compatible infrared device properly (e.g. to connect to your WiFi, connect to Home Assistant etc.), but the important pieces for this OSGP Meter ESPHome component are as follows:

```yaml
# The serial port interface for the infrared LEDs: Change according to your setup.
# Here, transmitting LED is wired to GPIO1 and reading LED is wired to GPIO3.
# This is compatible with the famous Hichi IR WiFi v2 device.
uart:
  - id: meter_uart
    tx_pin: GPIO1
    rx_pin: GPIO3
    baud_rate: 9600
    data_bits: 8
    parity: NONE
    stop_bits: 1

# Keep as is
logger:
  level: INFO
  baud_rate: 0

# The following section defines the fields you want to see.
# It is a trivial example config only. Typically your Smart Meter is capable of providing
# way more data points. See the example_nes_meter.yaml for all configuration options.
# NOTE: The "password" field is your read-only key (RK), most likely to be acquired
# from your power grid company.
sensor:
  - platform: osgp_meter
    uart_id: meter_uart
    user_id: 1
    username: "esphome"
    password: !secret osgp_password
    refresh_interval: 5s
    logoff_interval: 540s
    static_info_interval: 1h
    poll_jitter: 200ms
    health_log_interval: 60s
    log_raw: false
    fwd_active_energy:
      name: "Meter Forward Active Energy"
    fwd_active_power:
      name: "Meter Forward Active Power"
    manufacturer:
      name: "Meter Manufacturer"
    model:
      name: "Meter Model"
```

## Known Working Baseline

The current implementation is known to work with a live NES meter using:

- `9600 8N1`
- `logger.baud_rate: 0` so UART logging does not collide with the IR head
- `refresh_interval: 5s`
- `poll_jitter: 200ms`
- `logoff_interval: 540s`
- username `esphome`
- password provided by power grid company (20 character ASCII key)

## Example: M-Bus Water And Heat Meters

Some OSGP electricity meters act as an M-Bus relay for other household meters. See your electricity meter display for a "M"-line with adjacent numbers. These numbers represent the "slots" where other meters in your household are connected. Try pressing the "Display Cycle" button on your NES smart meter for about eleven seconds to run a M-Bus scan that searches for available M-Bus meters in your household. Note: It is very likely that they are connected and available already, so there is usually no need to run this scan.

If the meter knows about the connected M-Bus meters, this ESPHome component can discover configured devices by their eight-digit M-Bus serial
number and read RK-accessible scheduled data from ET16 and the ET45 circular
log. `mbus.update_interval` controls how often the component scans ET45; it
does not make the electricity meter physically poll a subordinate meter. The
component does not issue on-demand reads because EP19 requires MAK/MK-level
access rather than the read-only key (RK).

Use [example_nes_meter_mbus.yaml](example_nes_meter_mbus.yaml) for a complete
water and heat setup. Keep household identifiers in the ignored
`secrets.yaml`:

```yaml
mbus_water_meter_serial: "12345678"
mbus_heat_meter_serial: "87654321"
```

The optional `slot` is a discovery hint (`1` through `4`); serial matching is
authoritative. `medium: water` accepts generic, hot-water, and cold-water M-Bus
medium codes. If a configured convenience value is absent from a telegram, its
ESPHome entity remains unavailable and no recurring warning is emitted.

The component supplies native ESPHome/Home Assistant metadata unless YAML
overrides it:

| Value | Unit | Device class | State class |
| --- | --- | --- | --- |
| Water total | `m³` | `water` | `total_increasing` |
| Water/heating flow | `m³/h` | `volume_flow_rate` | `measurement` |
| Heat total | `kWh` | `energy` | `total_increasing` |
| Thermal power | `W` | `power` | `measurement` |
| Temperatures | `°C` | `temperature` | `measurement` |
| Temperature difference | `K` | `temperature_delta` | `measurement` |

Pressure and duration raw records also receive corresponding units and device
classes. Explicit `unit_of_measurement`, `device_class`, `state_class`,
`accuracy_decimals`, and `icon` values in YAML take precedence over inferred
defaults. These properties are forwarded through ESPHome's native API for Home
Assistant dashboards and long-term statistics. See the
[ESPHome sensor metadata](https://esphome.io/components/sensor/index.html) and
[Home Assistant sensor classes](https://developers.home-assistant.io/docs/core/entity/sensor/).

M-Bus timestamps describe when the electricity meter collected each scheduled
read. Collection frequency is configured in the meter and can be much slower
than `mbus.update_interval`; use `last_read` to judge freshness. An ET14 status
of `active` reports the subordinate meter's communication/commissioning state,
not the freshness of its measurement data. Similarly, an unavailable water
flow sensor means the stored telegram contains no matching flow record; the
component does not derive flow from changes in a daily total.

At startup the component reads the RK-accessible ET13, ET34, and primary ET42
configuration and writes one INFO summary per configured device:

```text
M-Bus configuration: serial=12345678 slot=1 handle=1 schedule="daily at 00:00" status_reads="every 1 min" primary_load_profile="no M-Bus channels; interval=15 min; poll=60 min"
```

The summary reports the scheduled billing read, separate status-read cadence,
primary load-profile interval, physical M-Bus polling interval, and any matching
M-Bus channel/MDT identifiers. Configuration is checked again according to
`static_info_interval`, but INFO is emitted again only if the summary changes.
Malformed or inaccessible optional diagnostic tables are visible at DEBUG and
do not interrupt electricity or scheduled M-Bus reads.

When the summary says `no M-Bus channels`, no faster primary load-profile data
is currently available to read. Ask the meter supplier to increase the ET13
scheduled billing-read frequency, or to assign the subordinate meter to M-Bus
channels in the primary load profile and configure a suitable physical polling
interval. These changes cannot be made with the component's read-only RK; it
does not invoke EP19 or write meter configuration.

For initial discovery, temporarily set `dump_records: true` and use a DEBUG
logger. Each record is logged with DIF, VIF, function, storage, tariff, and
subunit. Uncommon records can then be selected explicitly:

```yaml
records:
  - name: "Custom M-Bus Value"
    dif: 0x04
    vif: 0x13
    function: instantaneous
    storage: 1
```

Return `dump_records` to `false` for normal operation.

## Important Notes

- Do not enable UART debug logging on the same UART as the optical IR head.
- The component only compiles the text sensors you actually configure.
- `log_raw: true` is useful for troubleshooting, but it is noisy and usually
  not needed for normal operation.
- Your meter must allow read-only access with the password / RK you provide.
- M-Bus availability and scheduled-read contents depend on the electricity
  meter's M-Bus configuration and the records emitted by each attached meter.

## References & Links

OSGP protocol & reference documentation is available on Github: [High Level](https://github.com/OSGP-Alliance-MEP-and-Optical/Documentation) and
very [detailed protocol description](https://github.com/OSGP-Alliance-MEP-and-Optical/Documentation) input is there. These documents were consulted to implement this component.

There is an inofficial, early-stage [openHAB binding for OSGP smart meters](https://community.openhab.org/t/smartmeterosgp-binding/142859), too.

## License

This software is provided without warranty. The software author can not be held liable for any
damages inflicted by the software. Use it at your own risk and according to laws and
terms & conditions applying to you.

See [LICENSE](LICENSE) for more details.
