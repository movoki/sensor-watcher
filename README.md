![SensorWatcher Logo](logo.png?raw=True)

# About

SensorWatcher is a firmware for the ESP32 family of microcontrollers that automatically detects I2C, 1-Wire, or BLE sensors and uploads the sensor measurements to backends using HTTP, MQTT, or UDP.

# Backends

## Protocols

- HTTP
- HTTPS
- MQTT
- MQTTS
- UDP

## Authentication

- Basic
- Digest
- Bearer
- Token
- Header
- Postman
- X.509

## Data formats

- SenML
- Postman
- User defined template

## Predefined templates

- InfluxDB
- InfluxDB Cloud
- TagoIO
- Thinger
- Ubidots


# Hardware

## ESP32 boards

- Adafruit ESP32 Feather V2
- Adafruit QT Py ESP32 Pico
- M5Stack Atom Lite
- M5Stack Atom Matrix
- M5Stack Atom Echo
- M5Stack Atom U
- M5Stack Core2
- M5Stack Core2 AWS
- M5stack MStickC
- M5stack MStickC Plus
- M5Stack Station-BAT
- M5Stack Station-RS485
- M5Stack Tough
- Generic ESP32 boards

## ESP32-S3 boards

- Adafruit ESP32-S3 Feather
- Adafruit QT Py ESP32-S3
- M5Stack AtomS3 Lite
- SeeedStudio Xiao ESP32S3
- Generic ESP32-S3 boards

## ESP32-C3 boards

- SeeedStudio Xiao ESP32C3
- Generic ESP32-C3 boards

## I2C devices

- BH1750
- BMP280
- BMP388
- DPS310
- HTU21
- HTU31
- LPS22
- LPS25
- LPS33
- LPS35
- MCP9600
- MCP9601
- MCP9808
- MLX90614
- PCA9548A
- SCD40
- SCD41
- SEN50
- SEN54
- SEN55
- SHT30
- SHT31
- SHT35
- SHT40
- SHT41
- TCA9548A
- TMP117
- TSL2591
- VEML7700

## 1-Wire devices

- DS18B20
- TMP1826

## BLE devices

- RuuviTag
- Minew S1
- Xiaomi LYWSDCGQ


# Install and configure

## Easy way

- Go to https://movoki.com/setup/ and use the web setup tool there to upload the firmware and configure it. You will need to use Google Chrome because it requires support for WebSerial.

## Hard way

- Download and install ESP-IDF 5.2 https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html
- Go to the esp32 or esp32s3 folder in this project, depending on what device you have, and run 'idf.py flash'.
- Then go to the tools folder in this project and use sensorwatcher-cli.py to configure the firmware.


# Community resources

- [Telegram group](https://t.me/SensorWatcher).


# License

Most of the code is released under the GPL-3 license. Some libraries are under the MIT license.


# Roadmap

- Support for ESP32-C6:
    - SeeedStudio Xiao ESP32C6
    - M5Stack NanoC6

- Encripted BLE advertisings for extended and long-range modes.

- Add new wireless transports like LoRa and ZigBee.

- Support more sensors!


# ChangeLog

## 0.11

- Added support for defining I2C and 1-Wire buses with shared pins. This allows autodetecting I2C and 1-Wire devices on the same Grove port. Boards that have this enabled by default on Grove PORT-A:
    - Atom Lite
    - Atom Echo
    - Atom Matrix
    - Atom U,
    - AtomS3
    - AtomS3 Lite
    - MStickC
    - MStickC Plus

- Postman requests received over HTTP can now send a response in another HTTP request.

- BLE scanning window reduced from 100% to 35% to avoid overheating.

- New board parameter to set the CPU frequency.

- Measurements "name" renamed as "path". Some template variables changed also: i->n, n->p, P->D, p->d.

## 0.10

- Continuous BLE scan mode that can be enabled by setting the scan_duration to 255.

- More tuning to the BLE advertisement timing to improve data reception rate at the gateways.

- Better timestamping for measurements from nodes to support parallel redundant BLE gateways.

## 0.9

- Added support for extended and long-range BLE advertisements for chips that support BT5.

- Changed the format for BT4 node advertisements. If you are using SensorWatcher as BLE nodes with previous releases, please upgrade both sensor and gateway nodes to this release at the same time.

- Does not start the WiFi driver if the SSID is not set. For BLE-only use cases.

## 0.8

- Added support for ESP32-C3 and Xiao ESP32C3.

- Use the hardware USB Serial JTAG when available instead of TinyUSB.

- Use a single buffer instead of two for HTTP requests and responses.

## 0.7

- New "nodes" resource to track other SensorWatcher nodes sharing measurements.

- Added support for receiving measurements via BLE from other SensorWatcher nodes, so some nodes can act as BLE sensors and others as BLE gateways that collect measurements via BLE and forward them to backends.

- New options for setting the power level of BLE advertisements and to receive BLE advertisements only from persistent devices or nodes.

- Included a draft of an API schema for all resources. The idea is to enable automatic web configurator generation in the not-so-distant future.

- Added support for generic ESP32-S3 boards.

- Diagnostic measurements are now split by resource and can be enabled independently.

- Disabled pins now use None instead of 255.

- Fixed problems with TinyUSB on ESP32-S3 when transferring packets larger than ~4KB.

- Fixed a bug in BigPacks messing with the cursor stack when a response does not fit the buffer.

- Renamed "fixed" in devices to "persistent".

- Devices status is now a string instead of a boolean.

## 0.6

- Replace zero timestamps with NOW before sending for all supported encodings.

- Reset system time at boot to avoid using the unreliable internal RTC time.

- Added support for default 1-Wire bus to more boards and remapped the pins to make them compatible
  with the [Grove DS18B20 sensor](https://www.seeedstudio.com/One-Wire-Temperature-Sensor-p-1235.html)
  from SeeedStudio in these configurations:

    - Port A0 on the [Grove Base for XIAO](https://www.seeedstudio.com/Grove-Shield-for-Seeeduino-XIAO-p-4621.html):
        - Adafruit QT Py ESP32 Pico
        - Adafruit QT Py ESP32 S3
        - SeedStudio Xiao ESP32S3

    - Port 0-1 on the [Grove Shield for Particle Mesh](https://www.seeedstudio.com/Grove-Shield-for-Particle-Mesh-p-4080.html):
        - Adafruit ESP32 S3 Feather
        - Adafruit ESP32 Feather V2

    That sensor MAY work with these configurations too, but they are not recommended because the non-standard
    voltages in the M5Stack Grove ports will send 5V to the ESP32 3.3V input through the pull-up resistor:

    - Port B on the [AtomPortABC](https://docs.m5stack.com/en/unit/AtomPortABC):
        - M5Stack Atom Lite
        - M5Stack Atom Matrix
        - M5Stack Atom Echo
        - M5Stack AtomS3
        - M5Stack AtomS3 Lite

    - Port B:
        - M5Stack Core2
        - M5Stack Core2 AWS
        - M5Stack Tough

    - Port B2:
        - M5Stack Station BAT
        - M5Stack Station 485

    A better solution for M5Stack products is to build your own DS18B20 probe using pin 2 (SDA) for
    powering the sensor from a GPIO instead of pin 3 (VCC) as in the SeeedStudio DS18B20 probe. This
    will also work for Adafruit and SeeedStudio boards, and it allows you to turn off the sensor for
    lowering power consumption when using batteries:

            1 -------------o-----------------
                           |                | DQ(2)
        G       --/\/\/\/---  4.7K          |
        r       |                       ---------
        o   2 --o----------------------| DS18B20 |---
        v                       VDD(3)  ---------   | GND(1)
        e   3 - NC                                  |
                                                    |
            4 ---------------------------------------

