# ESPHome OSGP Meter Component

`osgp_meter` is an [ESPHome](https://esphome.io) external component for smart meters that expose an
optical infrared port and speak [Open Smart Grid Protocol (OSGP)](https://www.osgp.org) ([Wikipedia](https://en.wikipedia.org/wiki/Open_Smart_Grid_Protocol)) instead of the
often seen SML protocol (which is already [well supported in ESPHome](https://esphome.io/components/sml/)).
It has been tested on [Networked Energy Services (NES) / Echelon style meters](https://www.networkedenergy.com/en/products/smart-meters) with a [Hichi WLAN v2 infrared device](https://sites.google.com/view/hichi-lesekopf/wifi-v2) and it exposes lots of useful information on your power usage, consumption, voltages and many other grid-specific data points. You can also monitor energy returned to the grid.

> You'll most likely require a read-only key (RK) that -- at least in Germany -- is handed out by some power grid companies that install these smart meters.

It is intended to be dropped into ESPHome via a `external_components`
configuration entry, see [the example configuration file](example_nes_meter.yaml).

## Why?

Reading your smart meter data via ESPHome allows you to very easily make energy data available in home automation
systems like [Home Assistant](https://home-assistant.io), especially for [Energy Management](https://www.home-assistant.io/home-energy-management/#monitor_usage) there.
See documentation how to integrate ESPHome devices like yours. You maybe want to configure an [`api:`](https://esphome.io/components/api/) section.

## What You Get

- OSGP handshake, authentication, and table reads over UART-connected IR heads
- Forward / reverse energy counters
- Instantaneous power, current, voltage, power factor, and frequency
- Static meter information such as manufacturer, model, firmware, and serials
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

See [example_nes_meter.yaml](example_nes_meter.yaml) for a full example. The
important pieces are as follows:

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

## Important Notes

- Do not enable UART debug logging on the same UART as the optical IR head.
- The component only compiles the text sensors you actually configure.
- `log_raw: true` is useful for troubleshooting, but it is noisy and usually
  not needed for normal operation.
- Your meter must allow read-only access with the password / RK you provide.

## References & Links

OSGP protocol & reference documentation is available on Github: [High Level](https://github.com/OSGP-Alliance-MEP-and-Optical/Documentation) and
very [detailed protocol description](https://github.com/OSGP-Alliance-MEP-and-Optical/Documentation) input is there. These documents were consulted to implement this component.

There is an inofficial, early-stage [openHAB binding for OSGP smart meters](https://community.openhab.org/t/smartmeterosgp-binding/142859), too.

## License

This software is provided without warranty. The software author can not be held liable for any
damages inflicted by the software. Use it at your own risk and according to laws and
terms & conditions applying to you.

See [LICENSE](LICENSE) for more details.
