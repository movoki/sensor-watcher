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
- Then go to the tools folder in this project and use postman-cli.py to configure the firmware.


# License

Most of the code is released under the GPL-3 license. Some libraries are under the MIT license.
