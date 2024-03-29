![SensorWatcher Logo](logo.png?raw=True)

# About

SensorWatcher is a firmware for the ESP32 family of microcontrollers that automatically detects I2C, 1-Wire, or BLE sensors and uploads the sensor measurements to backends using HTTP, MQTT, or UDP.


# Supported hardware

## ESP32

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
- Generic ESP32 board

## ESP32-S3

- Adafruit ESP32-S3 Feather
- Adafruit QT Py ESP32-S3
- M5Stack AtomS3 Lite
- SeeedStudio Xiao ESP32S3
- Generic ESP32-S3 board

## I2C

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

## 1-Wire

- DS18B20
- TMP1826

## BLE

- RuuviTag
- Minew S1
- Xiaomi LYWSDCGQ


# Install and configure

## Easy way

- Go to https://movoki.com/setup/ and use the web setup tool there to upload the firmware and configure it. You will need to use Google Chrome because it requires support for WebSerial.

## Hard way

- Download and install ESP-IDF 5.2 https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html
- Go to the esp32 or esp32s3 folder in this project, depending on what device you have, and run 'idf.py flash'.
- Then go to the tools folder in this project and use bigpostman-cli.py to configure the firmware.

# Community resources

- ![Telegram group](https://t.me/SensorWatcher).

# License

Most of the code is released under the GPL-3 license. Some libraries are under the MIT license.

# ChangeLog

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

