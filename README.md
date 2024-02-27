# nibe-mqtt-gateway

nibe-mqtt-gateway is an MQTT integration for Nibe heatpumps.

It is used to integrate a Nibe VVM310/S2125 into Home Assistant and additional monitoring systems via Mosquitto as MQTT broker.

## Features

- [ ] connection to Nibe heatpump VVM310/S2125 via RS485
- [x] wired Ethernet (no Wifi needed nor supported)
- [x] direct connection to MQTT broker
- [ ] configurable set of of published Nibe registers/coils
- [ ] supports writing to Nibe registers
- [x] exposes the 4 relays of the PRODIno ESP32 board via MQTT
- [x] supports Home Assistant MQTT auto-discovery
- [x] simple web UI for info and administration
- [x] OTA updates (well, over Ethernet)
- [x] upload of configuration files
- [x] metrics via Prometheus endpoint
- [x] logging via MQTT topic (as alternative to serial interface)

## Prerequisites

- [PRODINo ESP32 Ethernet v1 board](https://kmpelectronics.eu/products/prodino-esp32-ethernet-v1/) - other boards might be possible but have not been tested
- USB adapter for debugging (via serial output, no real JTAG debugging unfortunately) and initial uploading of firmware
- Nibe heatpump VVM310/S2125 - other models might work but have not been tested
- ESP-IDF v5.1.3, use branch release-5.1 until released
  - since the PRODINo ESP32 Ethernet v1 board doesn't wire the W5500 interrupt line, [PR #12692](https://github.com/espressif/esp-idf/pull/12692) needs to be cherry-picked on top, TODO: check
- MQTT broker like [Mosquitto](https://mosquitto.org/)

Additionally helpful:
- Visual Studio Code + [ESP-IDF Visual Studio Code Extension](https://github.com/espressif/vscode-esp-idf-extension)

## Installation

### Wiring

todo

### Building

- install ESP-IDF v5.1.3 (or release-5.1 branch)
- clone this project
- adapt settings, especially `idf.espIdfPath` to point to ESP-IDF installation
- generate sdkconfig and adapt if necessary
- build project

Result is a firmware file: `./build/nige-mqtt-gateway.bin`

### Installation

For initial installation, flash the firmware using a serial adapter.

For subsequent installations, the firmware can be uploaded via OTA: http://nibegw/update

### Configuration

When uploading a configuration, nibe-mqtt-gateway stores it in flash memory and reboots.

- http://nibegw/config shows the current configuration as json, save e.g. as `config.json` and use as template
- adapt `config.json` file (MQTT broker URL and credentials etc.)
- upload `config.json`: `curl -X POST -H "Content-Type: application/json" -d @config.json http://nibegw/config`

### Trouble Shooting

The RGB multi-functional LED shows the status of nibe-mqtt-gateway:
- blue - initializing, waiting for IP address (blue blinking)
- green blinking - running ok, connected with MQTT broker
- orange blinking - error, got IP address but e.g. no connected with MQTT broker, check http://nibegw or logs
- red blinking - OTA firmware upload in progress

Web UI:
- http://nibegw - main page with some status info
- http://nibegw/update - OTA update and upload of file system
- http://nibegw/metrics - Prometheus endpoint with some insights like heap, uptime and execution times

Depending on the configuration, logs are available via Serial interface or the MQTT topic `nibegw/log`.
Show logging over MQTT:
```
mosquitto_sub --url 'mqtt://<user>:<password>@<broker>/nibegw/log'
```

## Development

### Build

Visual Studio Code + [ESP-IDF Visual Studio Code Extension](https://github.com/espressif/vscode-esp-idf-extension) is used as IDE.

- switch target to `esp32`
- delete `sdkconfig` file
- 'Full Clean' (delete `build` directory)
- 'Build' project
- Upload to Prodino board either via serial interface or OTA
- see above for configuration and trouble shooting

### Unit Testing

Parts of the code base have unit tests using [Unity](https://www.throwtheswitch.org/unity) that can run on the Linux/Mac (see [ESP-IDF - Running Applications on Host](https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/api-guides/host-apps.html)).

How to run unit tests using vscode:
- switch target to `linux`
- delete `sdkconfig` file
- 'Full Clean' (delete `build` directory)
- 'Build' project, ignore the error on running `esp_idf_size`
- 'Monitor' runs the unit tests and prints results to console
  - attention: 'Monitor' doesn't build -> an explicit 'Build' is needed after any source change

Implementation:
- tests are located in a special `test` component, this component picks source files from `main` that can be tested 
- any code that depends on `arduino-esp32` can't run on the host emulation and therefore is not testable easily, this includes any usage of the Arduino String class

## To Do

- [ ] replace [PR #12692](https://github.com/espressif/esp-idf/pull/12692) by better solution mentioned in [#12682](https://github.com/espressif/esp-idf/issues/12682) once available on branch release-5.1
- [ ] switch to esp-dif 5.1.3 once released
- [ ] use arduino-esp32 as managed component once version >=3.0.0-alpha3 is available
- [ ] smoother unit testing development cycle: vscode task, less Arduino dependencies
- [ ] debugging, at least for unit tests on host
 
## Credits

Used source code and libraries
- [arduino-esp32](https://github.com/espressif/arduino-esp32) - Arduino core for the ESP32, [LGPL v2.1](https://github.com/espressif/arduino-esp32/blob/master/LICENSE.md)
- [ArduinoJson](https://arduinojson.org/) - JSON Library, [MIT License](https://github.com/bblanchon/ArduinoJson/blob/7.x/LICENSE.txt)
- [esp-idf](https://github.com/espressif/esp-idf) - Espressif IoT Development Framework, [Apache License 2.0](https://github.com/espressif/esp-idf/blob/master/LICENSE)
- [esp-idf-net-logging](https://github.com/nopnop2002/esp-idf-net-logging) - Redirect esp-idf logging to the network, [MIT License](https://github.com/nopnop2002/esp-idf-net-logging/blob/main/LICENSE)
- [org.openhab.binding.nibeheatpump](https://github.com/openhab/openhab-addons/tree/main/bundles/org.openhab.binding.nibeheatpump) - nibegw, [Eclipse Public License 2.0](https://github.com/openhab/openhab-addons/blob/main/bundles/org.openhab.binding.nibeheatpump/NOTICE)
- [ProDinoESP32](https://github.com/kmpelectronics/ProDinoESP32) - KMP ProDino ESP32 examples library
- [Unity](https://www.throwtheswitch.org/unity) - Unit testing for C

Inspiration and ideas
- [nibe-mqtt](https://github.com/yozik04/nibe-mqtt) - Nibe MQTT integration, [GPL 3.0](https://github.com/yozik04/nibe-mqtt/blob/master/LICENSE)

## License and Disclaimer

This project has been developed for personal use only. The code is provided AS IS under the [Apache License v2.0](./LICENSE) in the hope that it will be useful, but WITHOUT ANY WARRANTY whatsoever.

Nibe is registered mark of NIBE Energy Systems.