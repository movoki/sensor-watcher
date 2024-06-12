// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef i2c_h
#define i2c_h

#define I2C_BUSES_NUM_MAX 		4
#define I2C_BUS_SPEED_DEFAULT 	100000
#define I2C_BUS_SPEED_MAX 		4000000
#define I2C_MASTER_TIMEOUT_MS   1000
#define I2C_PCA9548_ADDRESS 	0x70
#define I2C_PCA9548_NUM_MAX 	6

#include "devices.h"
#include "enums.h"
#include "bigpacks.h"

typedef struct {
	uint32_t	speed;
	uint8_t		port;
	uint8_t 	sda_pin;
	uint8_t 	scl_pin;
	bool		enabled;
	bool		active;
} i2c_bus_t;

extern i2c_bus_t i2c_buses[];
extern uint8_t i2c_buses_count;

bool i2c_read(uint8_t port, uint8_t address, uint8_t *buffer, size_t buffer_size);
bool i2c_write(uint8_t port, uint8_t address, uint8_t *buffer, size_t buffer_size);

void i2c_init();
bool i2c_read_from_nvs();
bool i2c_write_to_nvs();
bool i2c_using_gpio(uint8_t gpio);
esp_err_t i2c_start();
esp_err_t i2c_start_bus(uint8_t bus);
esp_err_t i2c_stop();
esp_err_t i2c_stop_bus(uint8_t bus);
void i2c_set_power(bool state);
void i2c_set_default();
bool i2c_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t i2c_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

void i2c_detect_devices();
bool i2c_detect_channel(device_bus_t bus, device_multiplexer_t multiplexer, device_channel_t channel);
bool i2c_detect_device(device_bus_t bus, device_part_t part, device_address_t address);
bool i2c_measure_device(devices_index_t device);

int32_t twos_complement(int32_t value, uint8_t bits);
uint8_t mlx_crc(uint8_t *buffer, int length);
bool sensirion_check_crc(uint8_t *buffer);
bool htu_check_crc(uint8_t *buffer);

bool i2c_detect_sht3x(device_bus_t bus, device_address_t address);
bool i2c_detect_sht4x(device_bus_t bus, device_address_t address);
bool i2c_detect_htu21d(device_bus_t bus, device_address_t address);
bool i2c_detect_htu31d(device_bus_t bus, device_address_t address);
bool i2c_detect_mcp9808(device_bus_t bus, device_address_t address);
bool i2c_detect_tmp117(device_bus_t bus, device_address_t address);
bool i2c_detect_lps2x3x(device_bus_t bus, device_address_t address);
bool i2c_detect_bmp280(device_bus_t bus, device_address_t address);
bool i2c_detect_bmp388(device_bus_t bus, device_address_t address);
bool i2c_detect_dps310(device_bus_t bus, device_address_t address);
bool i2c_detect_mlx90614(device_bus_t bus, device_address_t address);
bool i2c_detect_mcp960x(device_bus_t bus, device_address_t address);
bool i2c_detect_bh1750(device_bus_t bus, device_address_t address);
bool i2c_detect_veml7700(device_bus_t bus, device_address_t address);
bool i2c_detect_veml6075(device_bus_t bus, device_address_t address);
bool i2c_detect_tsl2591(device_bus_t bus, device_address_t address);
bool i2c_detect_scd4x(device_bus_t bus, device_address_t address);
bool i2c_detect_sen5x(device_bus_t bus, device_address_t address);

bool i2c_measure_sht3x(devices_index_t device);
bool i2c_measure_sht4x(devices_index_t device);
bool i2c_measure_htu21d(devices_index_t device);
bool i2c_measure_htu31d(devices_index_t device);
bool i2c_measure_mcp9808(devices_index_t device);
bool i2c_measure_tmp117(devices_index_t device);
bool i2c_measure_lps2x3x(devices_index_t device);
bool i2c_measure_bmp280(devices_index_t device);
bool i2c_measure_bmp388(devices_index_t device);
bool i2c_measure_dps310(devices_index_t device);
bool i2c_measure_mlx90614(devices_index_t device);
bool i2c_measure_mcp960x(devices_index_t device);
bool i2c_measure_bh1750(devices_index_t device);
bool i2c_measure_veml7700(devices_index_t device);
bool i2c_measure_veml6075(devices_index_t device);
bool i2c_measure_tsl2591(devices_index_t device);
bool i2c_measure_scd4x(devices_index_t device);
bool i2c_measure_sen5x(devices_index_t device);

#endif
