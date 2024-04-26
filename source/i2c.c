// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include <esp_check.h>
#include <esp_chip_info.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <driver/i2c.h>
#include <driver/gpio.h>

#include "application.h"
#include "postman.h"
#include "board.h"
#include "devices.h"
#include "enums.h"
#include "i2c.h"
#include "measurements.h"
#include "now.h"
#include "schema.h"

i2c_bus_t i2c_buses[I2C_BUSES_NUM_MAX] = {{0}};
uint8_t i2c_buses_count = 0;

bool i2c_read(uint8_t port, uint8_t address, uint8_t *buffer, size_t buffer_size)
{
    return !i2c_master_read_from_device(port, address, buffer, buffer_size, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

bool i2c_write(uint8_t port, uint8_t address, uint8_t *buffer, size_t buffer_size)
{
    return !i2c_master_write_to_device(port, address, buffer, buffer_size, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void i2c_init()
{
    i2c_buses_count = 0;
    memset(i2c_buses, 0, sizeof(i2c_buses));
    i2c_read_from_nvs();
    if(!i2c_buses_count)
        i2c_set_default();
    i2c_start();
}

bool i2c_read_from_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;
    char nvs_key[24];

    err = nvs_open("i2c", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        ok = ok && !nvs_get_u8(handle, "count", &i2c_buses_count);
        for(uint8_t i = 0; i < i2c_buses_count && ok; i++) {
            snprintf(nvs_key, sizeof(nvs_key), "%u_port", i);
            ok = ok && !nvs_get_u8(handle, nvs_key, &(i2c_buses[i].port));
            snprintf(nvs_key, sizeof(nvs_key), "%u_sda_pin", i);
            ok = ok && !nvs_get_u8(handle, nvs_key, &(i2c_buses[i].sda_pin));
            snprintf(nvs_key, sizeof(nvs_key), "%u_scl_pin", i);
            ok = ok && !nvs_get_u8(handle, nvs_key, &(i2c_buses[i].scl_pin));
            snprintf(nvs_key, sizeof(nvs_key), "%u_speed", i);
            ok = ok && !nvs_get_u32(handle, nvs_key, &(i2c_buses[i].speed));
        }

        if(!ok)
            i2c_buses_count = 0;

        nvs_close(handle);
        ESP_LOGI(__func__, "%s, count = %i", ok ? "done" : "failed", i2c_buses_count);
        return ok;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

bool i2c_write_to_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;
    char nvs_key[16];

    err = nvs_open("i2c", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        for(uint8_t i = 0; i < i2c_buses_count && ok; i++) {
            snprintf(nvs_key, sizeof(nvs_key), "%u_port", i % 255);
            ok = ok && !nvs_set_u8(handle, nvs_key, i2c_buses[i].port);
            snprintf(nvs_key, sizeof(nvs_key), "%u_sda_pin", i % 255);
            ok = ok && !nvs_set_u8(handle, nvs_key, i2c_buses[i].sda_pin);
            snprintf(nvs_key, sizeof(nvs_key), "%u_scl_pin", i % 255);
            ok = ok && !nvs_set_u8(handle, nvs_key, i2c_buses[i].scl_pin);
            snprintf(nvs_key, sizeof(nvs_key), "%u_speed", i % 255);
            ok = ok && !nvs_set_u32(handle, nvs_key, i2c_buses[i].speed);
        }
        ok = ok && !nvs_set_u8(handle, "count", i2c_buses_count);
        ok = ok && !nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(__func__, "%s, count = %i", ok ? "done" : "failed", i2c_buses_count);
        return ok;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

static bool write_resource_schema(bp_pack_t *writer)
{
    bool ok = true;
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_put_integer(writer, SCHEMA_LIST | SCHEMA_MAXIMUM_ELEMENTS);
        ok = ok && bp_create_container(writer, BP_LIST);
            ok = ok && bp_put_integer(writer, SCHEMA_MAP);
            ok = ok && bp_create_container(writer, BP_MAP);

                ok = ok && bp_put_string(writer, "port");
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_MINIMUM | SCHEMA_MAXIMUM);
                    ok = ok && bp_put_integer(writer, 0);
                    ok = ok && bp_put_integer(writer, I2C_NUM_MAX - 1);
                ok = ok && bp_finish_container(writer);

                ok = ok && bp_put_string(writer, "sda_pin");
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_MINIMUM | SCHEMA_MAXIMUM);
                    ok = ok && bp_put_integer(writer, 0);
                    ok = ok && bp_put_integer(writer, GPIO_NUM_MAX - 1);
                ok = ok && bp_finish_container(writer);

                ok = ok && bp_put_string(writer, "scl_pin");
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_MINIMUM | SCHEMA_MAXIMUM);
                    ok = ok && bp_put_integer(writer, 0);
                    ok = ok && bp_put_integer(writer, GPIO_NUM_MAX - 1);
                ok = ok && bp_finish_container(writer);

                ok = ok && bp_put_string(writer, "speed");
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_MINIMUM | SCHEMA_MAXIMUM);
                    ok = ok && bp_put_integer(writer, 0); ///
                    ok = ok && bp_put_integer(writer, I2C_BUS_SPEED_MAX);
                ok = ok && bp_finish_container(writer);

            ok = ok && bp_finish_container(writer);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, I2C_NUM_MAX);
    ok = ok && bp_finish_container(writer);
    return ok;
}

bool i2c_schema_handler(char *resource_name, bp_pack_t *writer)
{
    bool ok = true;

    // GET / PUT
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);                                // Path
            ok = ok && bp_put_string(writer, resource_name);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, SCHEMA_GET_RESPONSE | SCHEMA_PUT_REQUEST);    // Methods
        ok = ok && write_resource_schema(writer);                                       // Schema
    ok = ok && bp_finish_container(writer);

    return ok;
}


uint32_t i2c_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;

    if(method == PM_GET) {
        ok = ok && bp_create_container(writer, BP_LIST);
        for(uint32_t i=0; i != i2c_buses_count && ok; i++) {
            ok = ok && bp_create_container(writer, BP_MAP);
            ok = ok && bp_put_string(writer, "port") && bp_put_integer(writer, i2c_buses[i].port);
            ok = ok && bp_put_string(writer, "sda_pin") && bp_put_integer(writer, i2c_buses[i].sda_pin);
            ok = ok && bp_put_string(writer, "scl_pin") && bp_put_integer(writer, i2c_buses[i].scl_pin);
            ok = ok && bp_put_string(writer, "speed") && bp_put_integer(writer, i2c_buses[i].speed);
            ok = ok && bp_finish_container(writer);
        }
        ok = ok && bp_finish_container(writer);
        return ok ? PM_205_Content : PM_500_Internal_Server_Error;
    }
    else if(method == PM_PUT) {
        if(!bp_close(reader) || !bp_next(reader) || !bp_is_list(reader))
            return PM_400_Bad_Request;
        else {
            i2c_stop();
            i2c_buses_count = 0;
            memset(i2c_buses, 0, sizeof(i2c_buses));
            if(bp_open(reader)) {
                while(ok && i2c_buses_count < I2C_BUSES_NUM_MAX && bp_next(reader)) {
                    ok = ok && bp_open(reader);
                    while(ok && bp_next(reader)) {
                        if(bp_match(reader, "port"))
                            i2c_buses[i2c_buses_count].port = bp_get_integer(reader);
                        else if(bp_match(reader, "sda_pin"))
                            i2c_buses[i2c_buses_count].sda_pin = bp_get_integer(reader);
                        else if(bp_match(reader, "scl_pin"))
                            i2c_buses[i2c_buses_count].scl_pin = bp_get_integer(reader);
                        else if(bp_match(reader, "speed"))
                            i2c_buses[i2c_buses_count].speed = bp_get_integer(reader);
                        else bp_next(reader);
                    }
                    bp_close(reader);
                    if(ok) {
                        if(i2c_buses[i2c_buses_count].port < I2C_NUM_MAX && i2c_buses[i2c_buses_count].sda_pin < GPIO_NUM_MAX &&
                          i2c_buses[i2c_buses_count].scl_pin < GPIO_NUM_MAX && i2c_buses[i2c_buses_count].speed <= I2C_BUS_SPEED_MAX)
                                i2c_buses_count += 1;
                        else
                            ok = false;
                    }
                }
                bp_close(reader);
            }

            if(ok) {
                ok = ok && i2c_write_to_nvs();
                if(!i2c_buses_count)
                    i2c_set_default();
                i2c_start();
                return ok ? PM_204_Changed : PM_500_Internal_Server_Error;
            }
            else {
                i2c_buses_count = 0;
                memset(i2c_buses, 0, sizeof(i2c_buses));
                i2c_read_from_nvs();
                if(!i2c_buses_count)
                    i2c_set_default();
                i2c_start();
                return PM_400_Bad_Request;
            }
        }
    }
    else
        return PM_405_Method_Not_Allowed;
}

esp_err_t i2c_start()
{
    esp_err_t err = ESP_OK;

    i2c_set_power(true);
    for(int i = 0; i != i2c_buses_count; i++) {
        i2c_config_t config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = i2c_buses[i].sda_pin,
            .scl_io_num = i2c_buses[i].scl_pin,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = i2c_buses[i].speed,
        };
        err = err ? err : i2c_driver_install(i2c_buses[i].port, config.mode, 0, 0, 0);
        err = err ? err : i2c_param_config(i2c_buses[i].port, &config);
        ESP_LOGI(__func__, "starting I2C bus %i on SDA %i / SCL %i %s",
            i2c_buses[i].port, i2c_buses[i].sda_pin, i2c_buses[i].scl_pin, err ? "failed" : "done");
    }
    return err;
}

esp_err_t i2c_stop()
{
    esp_err_t err = ESP_OK;

    for(int i = 0; i != i2c_buses_count; i++) {
        err = i2c_driver_delete(i2c_buses[i].port);
        ESP_LOGI(__func__, "stopping I2C bus %i on SDA %i / SCL %i %s",
            i2c_buses[i].port, i2c_buses[i].sda_pin, i2c_buses[i].scl_pin, err ? "failed" : "done");
    }
    i2c_set_power(false);

    return err;
}

void i2c_set_power(bool state)
{
    for(int i = 0; i < i2c_buses_count; i++) {
        board_set_I2C_power(state); /// per bus?
    }
}

void i2c_set_default()
{
    switch(board.model) {
    case BOARD_MODEL_M5STACK_ATOM_LITE:
    case BOARD_MODEL_M5STACK_ATOM_MATRIX:
    case BOARD_MODEL_M5STACK_ATOM_ECHO:
        i2c_buses_count = 2;
        i2c_buses[0].port = 0;
        i2c_buses[0].sda_pin = 26;
        i2c_buses[0].scl_pin = 32;
        i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
        i2c_buses[1].port = 1;
        i2c_buses[1].sda_pin = 25;
        i2c_buses[1].scl_pin = 21;
        i2c_buses[1].speed = I2C_BUS_SPEED_DEFAULT;
        break;
    case BOARD_MODEL_M5STACK_ATOM_U:
        i2c_buses_count = 2;
        i2c_buses[0].port = 0;
        i2c_buses[0].sda_pin = 26;
        i2c_buses[0].scl_pin = 32;
        i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
        i2c_buses[1].port = 1;
        i2c_buses[1].sda_pin = 25;
        i2c_buses[1].scl_pin = 33;
        i2c_buses[1].speed = I2C_BUS_SPEED_DEFAULT;
        break;
    case BOARD_MODEL_M5STACK_ATOMS3:
    case BOARD_MODEL_M5STACK_ATOMS3_LITE:
        i2c_buses_count = 2;
        i2c_buses[0].port = 0;
        i2c_buses[0].sda_pin = 2;
        i2c_buses[0].scl_pin = 1;
        i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
        i2c_buses[1].port = 1;
        i2c_buses[1].sda_pin = 38;
        i2c_buses[1].scl_pin = 39;
        i2c_buses[1].speed = I2C_BUS_SPEED_DEFAULT;
        break;
    case BOARD_MODEL_M5STACK_M5STICKC:
    case BOARD_MODEL_M5STACK_M5STICKC_PLUS:
    case BOARD_MODEL_M5STACK_CORE2:
    case BOARD_MODEL_M5STACK_CORE2_AWS:
    case BOARD_MODEL_M5STACK_TOUGH:
    case BOARD_MODEL_M5STACK_M5STATION_BAT:
    case BOARD_MODEL_M5STACK_M5STATION_485:
        i2c_buses_count = 2;
        i2c_buses[0].port = 0;
        i2c_buses[0].sda_pin = 21;
        i2c_buses[0].scl_pin = 22;
        i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
        i2c_buses[1].port = 1;
        i2c_buses[1].sda_pin = 32;
        i2c_buses[1].scl_pin = 33;
        i2c_buses[1].speed = I2C_BUS_SPEED_DEFAULT;
        break;
    case BOARD_MODEL_ADAFRUIT_ESP32_FEATHER_V2:
        i2c_buses_count = 1;
        i2c_buses[0].port = 0;
        i2c_buses[0].sda_pin = 22;
        i2c_buses[0].scl_pin = 20;
        i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
        break;
    case BOARD_MODEL_ADAFRUIT_ESP32_S3_FEATHER:
        i2c_buses_count = 1;
        i2c_buses[0].port = 0;
        i2c_buses[0].sda_pin = 3;
        i2c_buses[0].scl_pin = 4;
        i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
        break;
    case BOARD_MODEL_ADAFRUIT_QT_PY_ESP32_PICO:
        i2c_buses_count = 1;
        i2c_buses[0].port = 0;
        i2c_buses[0].sda_pin = 22;
        i2c_buses[0].scl_pin = 19;
        i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
        break;
    case BOARD_MODEL_ADAFRUIT_QT_PY_ESP32_S3:
        i2c_buses_count = 1;
        i2c_buses[0].port = 0;
        i2c_buses[0].sda_pin = 41;
        i2c_buses[0].scl_pin = 40;
        i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
        break;
    case BOARD_MODEL_SEEEDSTUDIO_XIAO_ESP32S3:      // D4 D5
        i2c_buses_count = 1;
        i2c_buses[0].port = 0;
        i2c_buses[0].sda_pin = 5;
        i2c_buses[0].scl_pin = 6;
        i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
        break;
    case BOARD_MODEL_SEEEDSTUDIO_XIAO_ESP32C3:      // D4 D5
        i2c_buses_count = 1;
        i2c_buses[0].port = 0;
        i2c_buses[0].sda_pin = 6;
        i2c_buses[0].scl_pin = 7;
        i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
        break;
    case BOARD_MODEL_GENERIC:
        switch(board.processor) {
        case CHIP_ESP32:
            i2c_buses_count = 1;
            i2c_buses[0].port = 0;
            i2c_buses[0].sda_pin = 21;
            i2c_buses[0].scl_pin = 22;
            i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
            break;
        case CHIP_ESP32S3:
        case CHIP_ESP32C3:
            i2c_buses_count = 1;
            i2c_buses[0].port = 0;
            i2c_buses[0].sda_pin = 8;
            i2c_buses[0].scl_pin = 9;
            i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
            break;
        case CHIP_ESP32C6:
            i2c_buses_count = 1;
            i2c_buses[0].port = 0;
            i2c_buses[0].sda_pin = 23;
            i2c_buses[0].scl_pin = 22;
            i2c_buses[0].speed = I2C_BUS_SPEED_DEFAULT;
            break;
        default:
            break;
        }
        break;
    default:
        break;  // Do not enable I2C for unknown boards to avoid burning something connected on those pins! (M5Stack Atom Echo 👀)
    }
}

void i2c_detect_devices()
{
    uint8_t bus;
    uint8_t channel;
    uint8_t multiplexer;
    uint8_t channels_mask;
    uint8_t multiplexers_mask = 0;

    for(bus = 0; bus < i2c_buses_count; bus++) {
        multiplexers_mask = 0;
        for(multiplexer = 0; multiplexer < I2C_PCA9548_NUM_MAX; multiplexer++) {
            bool ok = true;
            channels_mask = 0xFF;
            ok = ok && i2c_write(i2c_buses[bus].port, I2C_PCA9548_ADDRESS + multiplexer, &channels_mask, 1);
            ok = ok && i2c_read(i2c_buses[bus].port, I2C_PCA9548_ADDRESS + multiplexer, &channels_mask, 1);
            ok = ok && channels_mask == 0xFF;
            channels_mask = 0;
            ok = ok && i2c_write(i2c_buses[bus].port, I2C_PCA9548_ADDRESS + multiplexer, &channels_mask, 1);
            ok = ok && i2c_read(i2c_buses[bus].port, I2C_PCA9548_ADDRESS + multiplexer, &channels_mask, 1);
            ok = ok && channels_mask == 0;
            multiplexers_mask |= ok ? 1 << multiplexer : 0;
        }
        ESP_LOGI(__func__, "multiplexers_mask for bus %i: %02x", bus, multiplexers_mask);

        if(multiplexers_mask) {
            for(multiplexer = 0; multiplexer < I2C_PCA9548_NUM_MAX; multiplexer++)
                if(multiplexers_mask & (1 << multiplexer))
                    for(channel = 0; channel < 8; channel++)
                        i2c_detect_channel(bus, multiplexer + 1, channel);
        }
        else
            i2c_detect_channel(bus, 0, 0);
    }
}

void i2c_detect_channel(device_bus_t bus, device_multiplexer_t multiplexer, device_channel_t channel)
{
    if(multiplexer) {
        uint8_t channels_mask = 1 << channel;
        i2c_write(i2c_buses[bus].port, I2C_PCA9548_ADDRESS + multiplexer - 1, &channels_mask, 1);
    }
    for(device_part_t part_index = 0; part_index < PART_NUM_MAX; part_index++) {
        if(parts[part_index].resource == RESOURCE_I2C)
            for(uint8_t address_index = 0; address_index < parts[part_index].id_span; address_index++) {
                device_t device = {
                    .resource = RESOURCE_I2C,
                    .bus = bus,
                    .multiplexer = multiplexer,
                    .channel = channel,
                    .address = parts[part_index].id_start + address_index,
                    .part = part_index,
                    .mask = parts[part_index].mask,
                    .status = DEVICE_STATUS_WORKING,
                    .persistent = false,
                    .timestamp = -1,
                };
                if(i2c_detect_device(device.bus, device.part, device.address)) {
                    int device_index = devices_get_or_append(&device);
                    if(device_index >= 0) {
                        char name[DEVICES_NAME_LENGTH];
                        devices_build_name(device_index, name, sizeof(name), '_');
                        ESP_LOGI(__func__, "Device found: %s", name);
                    }
                    else
                        ESP_LOGE(__func__, "DEVICES_NUM_MAX reached");
                }
            }
    }
    if(multiplexer) {
        uint8_t channels_mask = 0;
        i2c_write(i2c_buses[bus].port, I2C_PCA9548_ADDRESS + multiplexer - 1, &channels_mask, 1);
    }
}

bool i2c_detect_device(device_bus_t bus, device_part_t part, device_address_t address)
{
    switch(part) {
    case PART_SHT3X:
        return i2c_detect_sht3x(bus, address);
    case PART_SHT4X:
        return i2c_detect_sht4x(bus, address);
    case PART_HTU21D:
        return i2c_detect_htu21d(bus, address);
    case PART_HTU31D:
        return i2c_detect_htu31d(bus, address);
    case PART_MCP9808:
        return i2c_detect_mcp9808(bus, address);
    case PART_TMP117:
        return i2c_detect_tmp117(bus, address);
    case PART_BMP280:
        return i2c_detect_bmp280(bus, address);
    case PART_BMP388:
        return i2c_detect_bmp388(bus, address);
    case PART_LPS2X3X:
        return i2c_detect_lps2x3x(bus, address);
    case PART_DPS310:
        return i2c_detect_dps310(bus, address);
    case PART_MLX90614:
        return i2c_detect_mlx90614(bus, address);
    case PART_MCP960X:
        return i2c_detect_mcp960x(bus, address);
    case PART_BH1750:
        return i2c_detect_bh1750(bus, address);
    case PART_VEML7700:
        return i2c_detect_veml7700(bus, address);
    case PART_TSL2591:
        return i2c_detect_tsl2591(bus, address);
    case PART_SCD4X:
        return i2c_detect_scd4x(bus, address);
    case PART_SEN5X:
        return i2c_detect_sen5x(bus, address);
    default:
        return false;
    }
}

bool i2c_measure_device(devices_index_t device)
{
    bool ok = true;
    uint8_t channels_mask;

    if(devices[device].multiplexer) {
        channels_mask = 1 << devices[device].channel;
        i2c_write(i2c_buses[devices[device].bus].port, I2C_PCA9548_ADDRESS + devices[device].multiplexer - 1, &channels_mask, 1);
        /// delay?
    }
    switch(devices[device].part) {
    case PART_SHT3X:
        ok = i2c_measure_sht3x(device);
        break;
    case PART_SHT4X:
        ok = i2c_measure_sht4x(device);
        break;
    case PART_HTU21D:
        ok = i2c_measure_htu21d(device);
        break;
    case PART_HTU31D:
        ok = i2c_measure_htu31d(device);
        break;
    case PART_MCP9808:
        ok = i2c_measure_mcp9808(device);
        break;
    case PART_TMP117:
        ok = i2c_measure_tmp117(device);
        break;
    case PART_BMP280:
        ok = i2c_measure_bmp280(device);
        break;
    case PART_BMP388:
        ok = i2c_measure_bmp388(device);
        break;
    case PART_LPS2X3X:
        ok = i2c_measure_lps2x3x(device);
        break;
    case PART_DPS310:
        ok = i2c_measure_dps310(device);
        break;
    case PART_MLX90614:
        ok = i2c_measure_mlx90614(device);
        break;
    case PART_MCP960X:
        ok = i2c_measure_mcp960x(device);
        break;
    case PART_BH1750:
        ok = i2c_measure_bh1750(device);
        break;
    case PART_VEML7700:
        ok = i2c_measure_veml7700(device);
        break;
    case PART_TSL2591:
        ok = i2c_measure_tsl2591(device);
        break;
    case PART_SCD4X:
        ok = i2c_measure_scd4x(device);
        break;
    case PART_SEN5X:
        ok = i2c_measure_sen5x(device);
        break;
    default:
        ok = false;
    }
    if(devices[device].multiplexer) {
        channels_mask = 0;
        i2c_write(i2c_buses[devices[device].bus].port, I2C_PCA9548_ADDRESS + devices[device].multiplexer - 1, &channels_mask, 1);
    }

    if(ok)
        devices[device].timestamp = NOW;

    return ok;
}


// Devices //

int32_t twos_complement(int32_t value, uint8_t bits)
{
    return value & (uint32_t)1 << (bits - 1) ? value - ((uint32_t)1 << bits) : value;
}

bool sensirion_check_crc(uint8_t *buffer)
{
  static const uint8_t crc_table[16] = {
    0x00, 0x31, 0x62, 0x53, 0xc4, 0xf5, 0xa6, 0x97,
    0xb9, 0x88, 0xdb, 0xea, 0x7d, 0x4c, 0x1f, 0x2e
  };

  uint8_t crc = 0xff;
  crc ^= buffer[0];
  crc  = (crc << 4) ^ crc_table[crc >> 4];
  crc  = (crc << 4) ^ crc_table[crc >> 4];
  crc ^= buffer[1];
  crc  = (crc << 4) ^ crc_table[crc >> 4];
  crc  = (crc << 4) ^ crc_table[crc >> 4];
  return crc == buffer[2];
}

bool i2c_detect_sht3x(device_bus_t bus, device_address_t address)
{
    uint8_t reset_cmd[] = { 0x30, 0xA2 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, reset_cmd, sizeof(reset_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t status_cmd[] = { 0xF3, 0x2D };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, status_cmd, sizeof(status_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t raw_buf[2];
    if(i2c_master_read_from_device(i2c_buses[bus].port, address, raw_buf, sizeof(raw_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if((raw_buf[0] & 0xF0) != 0x80 || (raw_buf[1] & 0x1F) != 0x10)
        return false;
    return true;
}

bool i2c_measure_sht3x(devices_index_t device)
{
    uint8_t measure_cmd[] = { 0x24, 0x00 };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, measure_cmd, sizeof(measure_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(30 / portTICK_PERIOD_MS);

    uint8_t raw_buf[6];
    if(i2c_master_read_from_device(i2c_buses[devices[device].bus].port, devices[device].address, raw_buf, sizeof(raw_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if (!sensirion_check_crc(raw_buf) || !sensirion_check_crc(raw_buf + 3))
        return false;

    float temperature = (((raw_buf[0] << 8 | raw_buf[1]) * 175) / 65535.0) - 45;
    float humidity = (((raw_buf[3] << 8 | raw_buf[4]) * 100) / 65535.0);
    humidity = humidity > 100 ? 100 : (humidity < 0 ? 0 : humidity);

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C, %f %%", temperature, humidity);
    return measurements_append_from_device(device, 0, METRIC_Temperature, timestamp, UNIT_Cel, temperature) &&
           measurements_append_from_device(device, 1, METRIC_Humidity, timestamp, UNIT_RH, humidity);
}

bool i2c_detect_sht4x(device_bus_t bus, device_address_t address)
{
    uint8_t reset_cmd[] = { 0x94 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, reset_cmd, sizeof(reset_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t serial_cmd[] = { 0x89 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, serial_cmd, sizeof(serial_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t raw_buf[6];
    if(i2c_master_read_from_device(i2c_buses[bus].port, address, raw_buf, sizeof(raw_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;

    if (!sensirion_check_crc(raw_buf) || !sensirion_check_crc(raw_buf + 3))
        return false;
    return true;
}

bool i2c_measure_sht4x(devices_index_t device)
{
    uint8_t measure_cmd[] = { 0xFD };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, measure_cmd, sizeof(measure_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(30 / portTICK_PERIOD_MS);

    uint8_t raw_buf[6];
    if(i2c_master_read_from_device(i2c_buses[devices[device].bus].port, devices[device].address, raw_buf, sizeof(raw_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if (!sensirion_check_crc(raw_buf) || !sensirion_check_crc(raw_buf + 3))
        return false;

    float temperature = (((raw_buf[0] << 8 | raw_buf[1]) * 175) / 65535.0) - 45;
    float humidity = (((raw_buf[3] << 8 | raw_buf[4]) * 125) / 65535.0) - 6;
    humidity = humidity > 100 ? 100 : (humidity < 0 ? 0 : humidity);

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C, %f %%", temperature, humidity);
    return measurements_append_from_device(device, 0, METRIC_Temperature, timestamp, UNIT_Cel, temperature) &&
           measurements_append_from_device(device, 1, METRIC_Humidity, timestamp, UNIT_RH, humidity);
}

bool htu_check_crc(uint8_t *buffer)
{
  static const uint8_t crc_table[16] = {
    0x00, 0x31, 0x62, 0x53, 0xc4, 0xf5, 0xa6, 0x97,
    0xb9, 0x88, 0xdb, 0xea, 0x7d, 0x4c, 0x1f, 0x2e
  };

  uint8_t crc = 0x00;
  crc ^= buffer[0];
  crc  = (crc << 4) ^ crc_table[crc >> 4];
  crc  = (crc << 4) ^ crc_table[crc >> 4];
  crc ^= buffer[1];
  crc  = (crc << 4) ^ crc_table[crc >> 4];
  crc  = (crc << 4) ^ crc_table[crc >> 4];
  return crc == buffer[2];
}

bool i2c_detect_htu21d(device_bus_t bus, device_address_t address)
{
    uint8_t reset_cmd[] = { 0xFE };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, reset_cmd, sizeof(reset_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    return true;
}

bool i2c_measure_htu21d(devices_index_t device)
{
    uint8_t measure_t_cmd[] = { 0xF3 };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, measure_t_cmd, sizeof(measure_t_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(70 / portTICK_PERIOD_MS);

    uint8_t t_data[3];
    if(i2c_master_read_from_device(i2c_buses[devices[device].bus].port, devices[device].address, t_data, sizeof(t_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if (!htu_check_crc(t_data))
        return false;

    uint8_t measure_h_cmd[] = { 0xF5 };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, measure_h_cmd, sizeof(measure_h_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(30 / portTICK_PERIOD_MS);

    uint8_t h_data[3];
    if(i2c_master_read_from_device(i2c_buses[devices[device].bus].port, devices[device].address, h_data, sizeof(h_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if (!htu_check_crc(h_data))
        return false;

    float temperature = (((t_data[0] << 8 | t_data[1]) * 175.72) / 65536.0) - 46.85;
    float humidity = (((h_data[0] << 8 | h_data[1]) * 125) / 65536.0) - 6;
    humidity = humidity > 100 ? 100 : (humidity < 0 ? 0 : humidity);

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C, %f %%", temperature, humidity);
    return measurements_append_from_device(device, 0, METRIC_Temperature, timestamp, UNIT_Cel, temperature) &&
           measurements_append_from_device(device, 1, METRIC_Humidity, timestamp, UNIT_RH, humidity);
}

bool i2c_detect_htu31d(device_bus_t bus, device_address_t address)
{
    uint8_t reset_cmd[] = { 0x1E };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, reset_cmd, sizeof(reset_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    return true;
}

bool i2c_measure_htu31d(devices_index_t device)
{
    uint8_t measure_cmd[] = { 0x5E };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, measure_cmd, sizeof(measure_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(30 / portTICK_PERIOD_MS);

    uint8_t read_th_cmd[] = { 0x00 };
    uint8_t th_data[6];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, read_th_cmd, sizeof(read_th_cmd), th_data, sizeof(th_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if (!htu_check_crc(th_data) || !htu_check_crc(th_data + 3))
        return false;

    float temperature = (((th_data[0] << 8 | th_data[1]) * 165) / 65535.0) - 40;
    float humidity = (((th_data[3] << 8 | th_data[4]) * 100) / 65535.0);
    humidity = humidity > 100 ? 100 : (humidity < 0 ? 0 : humidity);

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C, %f %%", temperature, humidity);
    return measurements_append_from_device(device, 0, METRIC_Temperature, timestamp, UNIT_Cel, temperature) &&
           measurements_append_from_device(device, 1, METRIC_Humidity, timestamp, UNIT_RH, humidity);
}

bool i2c_detect_mcp9808(device_bus_t bus, device_address_t address)
{
    uint8_t device_id_cmd[] = { 0x07 };
    uint8_t device_id_data[2];
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, device_id_cmd, sizeof(device_id_cmd), device_id_data, sizeof(device_id_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(device_id_data[0] != 0x04 || (device_id_data[1] & 0xF0) != 0)
        return false;
    return true;
}

bool i2c_measure_mcp9808(devices_index_t device)
{
    uint8_t measure_cmd[] = { 0x05 };
    uint8_t measure_data[2];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, measure_cmd, sizeof(measure_cmd), measure_data, sizeof(measure_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;

    float temperature;
    measure_data[0] = measure_data[0] & 0x1F;
    if ((measure_data[0] & 0x10) == 0x10) {
        measure_data[0] = measure_data[0] & 0x0F;
        temperature = 256 - (measure_data[0] * 16.0 + measure_data[1] / 16.0);
    }
    else
        temperature = measure_data[0] * 16.0 + measure_data[1] / 16.0;

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C", temperature);
    return measurements_append_from_device(device, 0, METRIC_Temperature, timestamp, UNIT_Cel, temperature);
}

bool i2c_detect_tmp117(device_bus_t bus, device_address_t address)
{
    uint8_t device_id_cmd[] = { 0x0F };
    uint8_t device_id_data[2];
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, device_id_cmd, sizeof(device_id_cmd), device_id_data, sizeof(device_id_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(device_id_data[0] != 0x01 || device_id_data[1] != 0x17)
        return false;
    return true;
}

bool i2c_measure_tmp117(devices_index_t device)
{
    uint8_t measure_cmd[] = { 0x00 };
    uint8_t measure_data[2];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, measure_cmd, sizeof(measure_cmd), measure_data, sizeof(measure_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;

    float temperature = 0.007812 * (int16_t)(measure_data[0] << 8 | measure_data[1]);

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C", temperature);
    return measurements_append_from_device(device, 0, METRIC_Temperature, timestamp, UNIT_Cel, temperature);
}

bool i2c_detect_lps2x3x(device_bus_t bus, device_address_t address)
{
    uint8_t who_am_i_cmd[] = { 0x0F };
    uint8_t who_am_i_data[1];
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, who_am_i_cmd, sizeof(who_am_i_cmd), who_am_i_data, sizeof(who_am_i_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(who_am_i_data[0] != 0xB1 && who_am_i_data[0] != 0xB3)    // LPS22HB: B1, LPS22HH: B3, LPS33HW: B1, *LP25HB: BD
        return false;
    return true;
}

bool i2c_measure_lps2x3x(devices_index_t device)
{
    uint8_t one_shot_cmd[] = { 0x11, 0x13 };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, one_shot_cmd, sizeof(one_shot_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(30 / portTICK_PERIOD_MS);

    uint8_t pt_cmd[] = { 0x28 };
    uint8_t pt_data[5];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, pt_cmd, sizeof(pt_cmd), pt_data, sizeof(pt_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;

    float pressure = twos_complement((int32_t)((pt_data[2] << 16) | (pt_data[1] << 8) | pt_data[0]), 24) / 4096.0;
    float temperature = (int16_t)(pt_data[4] << 8 | pt_data[3]) / 100.0;

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C, %f hPa", temperature,  pressure);
    return measurements_append_from_device(device, 0, METRIC_Pressure, timestamp, UNIT_hPa, pressure) &&
           measurements_append_from_device(device, 1, METRIC_Temperature, timestamp, UNIT_Cel, temperature);
}

bool i2c_detect_bmp280(device_bus_t bus, device_address_t address)
{
    uint8_t device_id_cmd[] = { 0xD0 };
    uint8_t device_id_data[1];
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, device_id_cmd, sizeof(device_id_cmd), device_id_data, sizeof(device_id_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(device_id_data[0] != 0x58)
        return false;
    return true;
}

bool i2c_measure_bmp280(devices_index_t device)
{
    uint8_t ctrl_meas_cmd[] = { 0xF4, 0x25 };  // t oversampling x 1, p oversampling x 1, forced mode
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, ctrl_meas_cmd, sizeof(ctrl_meas_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t readout_cmd[] = { 0xF7 };
    uint8_t readout_data[6];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, readout_cmd, sizeof(readout_cmd), readout_data, sizeof(readout_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    int32_t raw_pressure = readout_data[0] << 12 | readout_data[1] << 4 | readout_data[2] >> 4;
    int32_t raw_temperature = readout_data[3] << 12 | readout_data[4] << 4 | readout_data[5] >> 4;

    uint8_t read_calibration_cmd[] = { 0x88 };
    uint8_t read_calibration_data[26];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, read_calibration_cmd, sizeof(read_calibration_cmd), read_calibration_data, sizeof(read_calibration_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    uint16_t T1  = read_calibration_data[0] | read_calibration_data[1] << 8;
    int16_t T2   = read_calibration_data[2] | read_calibration_data[3] << 8;
    int16_t T3   = read_calibration_data[4] | read_calibration_data[5] << 8;
    uint16_t P1  = read_calibration_data[6] | read_calibration_data[7] << 8;
    int16_t P2   = read_calibration_data[8] | read_calibration_data[9] << 8;
    int16_t P3   = read_calibration_data[10] | read_calibration_data[11] << 8;
    int16_t P4   = read_calibration_data[12] | read_calibration_data[13] << 8;
    int16_t P5   = read_calibration_data[14] | read_calibration_data[15] << 8;
    int16_t P6   = read_calibration_data[16] | read_calibration_data[17] << 8;
    int16_t P7   = read_calibration_data[18] | read_calibration_data[19] << 8;
    int16_t P8   = read_calibration_data[20] | read_calibration_data[21] << 8;
    int16_t P9   = read_calibration_data[22] | read_calibration_data[23] << 8;

    int32_t fine_temperature = ((((raw_temperature >> 3) - ((int32_t) T1 << 1)) * (int32_t) T2) >> 11) +
        ((((((raw_temperature >> 4) - (int32_t) T1) * ((raw_temperature >> 4) - (int32_t) T1)) >> 12) * (int32_t) T3) >> 14);
    float temperature = ((fine_temperature * 5 + 128) >> 8) / 100.0;

    int32_t var1, var2;
    var1 = (((int32_t) fine_temperature) / 2) - (int32_t) 64000;
    var2 = (((var1 / 4) * (var1 / 4)) / 2048) * ((int32_t) P6);
    var2 = var2 + ((var1 * ((int32_t) P5)) * 2);
    var2 = (var2 / 4) + (((int32_t) P4) * 65536);
    var1 = (((P3 * (((var1 / 4) * (var1 / 4)) / 8192)) / 8) + ((((int32_t) P2) * var1) / 2)) / 262144;
    var1 = ((((32768 + var1)) * ((int32_t) P1)) / 32768);
    uint32_t pressure_int = (uint32_t)(((int32_t)(1048576 - raw_pressure) - (var2 / 4096)) * 3125);
    if(var1 == 0)     // Avoid exception caused by division with zero
        return false;
    // Check for overflows against UINT32_MAX/2; if pres is left-shifted by 1
    pressure_int = pressure_int < 0x80000000 ? (pressure_int << 1) / ((uint32_t) var1) : (pressure_int / (uint32_t) var1) * 2;
    var1 = (((int32_t) P9) * ((int32_t) (((pressure_int / 8) * (pressure_int / 8)) / 8192))) / 4096;
    var2 = (((int32_t) (pressure_int / 4)) * ((int32_t) P8)) / 8192;
    pressure_int = (uint32_t) ((int32_t) pressure_int + ((var1 + var2 + P7) / 16));
    float pressure = pressure_int / 100.0;

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C, %f hPa", temperature,  pressure);
    return measurements_append_from_device(device, 0, METRIC_Pressure, timestamp, UNIT_hPa, pressure) &&
           measurements_append_from_device(device, 1, METRIC_Temperature, timestamp, UNIT_Cel, temperature);
}

bool i2c_detect_bmp388(device_bus_t bus, device_address_t address)
{
    uint8_t device_id_cmd[] = { 0x00 };
    uint8_t device_id_data[1];
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, device_id_cmd, sizeof(device_id_cmd), device_id_data, sizeof(device_id_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(device_id_data[0] != 0x50)
        return false;
    return true;
}

bool i2c_measure_bmp388(devices_index_t device)
{
    uint8_t pwr_ctrl_cmd[] = { 0x1B, 0x13 };  // launch forced measurement
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, pwr_ctrl_cmd, sizeof(pwr_ctrl_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t readout_cmd[] = { 0x04 };
    uint8_t readout_data[6];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, readout_cmd, sizeof(readout_cmd), readout_data, sizeof(readout_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    int32_t raw_pressure = readout_data[2] << 16 | readout_data[1] << 8 | readout_data[0];
    int32_t raw_temperature = readout_data[5] << 16 | readout_data[4] << 8 | readout_data[3];

    uint8_t calibration_cmd[] = { 0x31 };
    uint8_t calibration_data[21];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, calibration_cmd, sizeof(calibration_cmd), calibration_data, sizeof(calibration_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    uint16_t    t1  = (uint16_t)calibration_data[1] << 8 | calibration_data[0];
    uint16_t    t2  = (uint16_t)calibration_data[3] << 8 | calibration_data[2];
    int8_t      t3  = (int8_t)(calibration_data[4]);
    int16_t     p1  = (int16_t)((uint16_t)calibration_data[6] << 8 | calibration_data[5]);
    int16_t     p2  = (int16_t)((uint16_t)calibration_data[8] << 8 | calibration_data[7]);
    int8_t      p3  = (int8_t)(calibration_data[9]);
    int8_t      p4  = (int8_t)(calibration_data[10]);
    uint16_t    p5  = (uint16_t)calibration_data[12] << 8 | calibration_data[11];
    uint16_t    p6  = (uint16_t)calibration_data[14] << 8 | calibration_data[13];
    int8_t      p7  = (int8_t)(calibration_data[15]);
    int8_t      p8  = (int8_t)(calibration_data[16]);
    int16_t     p9  = (int16_t)((uint16_t)calibration_data[18]<<8 | calibration_data[17]);
    int8_t      p10 = (int8_t)(calibration_data[19]);
    int8_t      p11 = (int8_t)(calibration_data[20]);

    uint64_t t_partial_data1 = (uint64_t)(raw_temperature - (256 * (uint64_t)(t1)));
    uint64_t t_partial_data2 = (uint64_t)(t2 * t_partial_data1);
    uint64_t t_partial_data3 = (uint64_t)(t_partial_data1 * t_partial_data1);
    int64_t t_partial_data4 = (int64_t)(((int64_t)t_partial_data3) * ((int64_t)t3));
    int64_t t_partial_data5 = ((int64_t)(((int64_t)t_partial_data2) * 262144) + (int64_t)t_partial_data4);
    int64_t t_fine = (int64_t)(((int64_t)t_partial_data5) / 4294967296);
    float temperature = (int64_t)((t_fine * 25)  / 16384) / 100.0;

    int64_t p_partial_data1 = t_fine * t_fine;
    int64_t p_partial_data2 = p_partial_data1 / 64;
    int64_t p_partial_data3 = (p_partial_data2 * t_fine) / 256;
    int64_t p_partial_data4 = (p8 * p_partial_data3) / 32;
    int64_t p_partial_data5 = (p7 * p_partial_data1) * 16;
    int64_t p_partial_data6 = (p6 * t_fine) * 4194304;
    int64_t p_offset        = (int64_t)((int64_t)(p5) * (int64_t)140737488355328) + p_partial_data4 + p_partial_data5 + p_partial_data6;
    p_partial_data2 = (((int64_t)p4) * p_partial_data3) / 32;
    p_partial_data4 = (p3 * p_partial_data1) * 4;
    p_partial_data5 = ((int64_t)(p2) - 16384) * ((int64_t)t_fine) * 2097152;
    int64_t p_sensitivity   = (((int64_t)(p1) - 16384) * (int64_t)70368744177664) + p_partial_data2 + p_partial_data4 + p_partial_data5;
    p_partial_data1 = (p_sensitivity / 16777216) * raw_pressure;
    p_partial_data2 = (int64_t)(p10) * (int64_t)(t_fine);
    p_partial_data3 = p_partial_data2 + (65536 * (int64_t)(p9));
    p_partial_data4 = (p_partial_data3 * raw_pressure) / 8192;
    p_partial_data5 = (p_partial_data4 * raw_pressure) / 512;
    p_partial_data6 = (int64_t)((uint64_t)raw_pressure * (uint64_t)raw_pressure);
    p_partial_data2 = ((int64_t)(p11) * (int64_t)(p_partial_data6)) / 65536;
    p_partial_data3 = (p_partial_data2 * raw_pressure) / 128;
    p_partial_data4 = (p_offset / 4) + p_partial_data1 + p_partial_data5 + p_partial_data3;
    float pressure = (((uint64_t)p_partial_data4 * 25) / (uint64_t)1099511627776) / 10000.0;

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C, %f hPa", temperature, pressure);
    return measurements_append_from_device(device, 0, METRIC_Pressure, timestamp, UNIT_hPa, pressure) &&
           measurements_append_from_device(device, 1, METRIC_Temperature, timestamp, UNIT_Cel, temperature);
}

bool i2c_detect_dps310(device_bus_t bus, device_address_t address)
{
    uint8_t device_id_cmd[] = { 0x0D };
    uint8_t device_id_data[1];
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, device_id_cmd, sizeof(device_id_cmd), device_id_data, sizeof(device_id_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(device_id_data[0] != 0x10)
        return false;

    uint8_t press_conf_cmd[] = { 0x06, 0x01 };  // pressure oversampling x 2
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, press_conf_cmd, sizeof(press_conf_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    uint8_t temp_conf_cmd[] = { 0x07, 0x80 };  // temperature oversampling x 1, MEMS source
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, temp_conf_cmd, sizeof(temp_conf_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    return true;
}

bool i2c_measure_dps310(devices_index_t device)
{
    uint8_t temp_sample_cmd[] = { 0x08, 0x02 };  // one shot temperature sample
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, temp_sample_cmd, sizeof(temp_sample_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);
    uint8_t press_sample_cmd[] = { 0x08, 0x01 };  // one shot pressure sample
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, press_sample_cmd, sizeof(press_sample_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t read_pt_cmd[] = { 0x00 };
    uint8_t read_pt_data[6];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, read_pt_cmd, sizeof(read_pt_cmd), read_pt_data, sizeof(read_pt_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    int32_t raw_pressure = twos_complement(read_pt_data[0] << 16 | read_pt_data[1] << 8 | read_pt_data[2], 24);
    int32_t raw_temperature = twos_complement(read_pt_data[3] << 16 | read_pt_data[4] << 8 | read_pt_data[5], 24);

    uint8_t calibration_source_cmd[] = { 0x28, 0x80 };  // use coeficients for MEMS sensor
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, calibration_source_cmd, sizeof(calibration_source_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    uint8_t read_calibration_cmd[] = { 0x10 };
    uint8_t coeffs[17];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, read_calibration_cmd, sizeof(read_calibration_cmd), coeffs, sizeof(coeffs), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    int16_t c0 = twos_complement(((uint16_t)coeffs[0] << 4) | (((uint16_t)coeffs[1] >> 4) & 0x0F), 12);
    int16_t c1 = twos_complement((((uint16_t)coeffs[1] & 0x0F) << 8) | coeffs[2], 12);
    int32_t c00 = twos_complement(((uint32_t)coeffs[3] << 12) | ((uint32_t)coeffs[4] << 4) | (((uint32_t)coeffs[5] >> 4) & 0x0F), 20);
    int32_t c10 = twos_complement((((uint32_t)coeffs[5] & 0x0F) << 16) | ((uint32_t)coeffs[6] << 8) | (uint32_t)coeffs[7], 20);
    int16_t c01 = (int16_t)coeffs[8] << 8 | coeffs[9];
    int16_t c11 = (int16_t)coeffs[10] << 8 | coeffs[11];
    int16_t c20 = (int16_t)coeffs[12] << 8 | coeffs[13];
    int16_t c21 = (int16_t)coeffs[14] << 8 | coeffs[15];
    int16_t c30 = (int16_t)coeffs[16] << 8 | coeffs[17];

    float scaled_raw_temperature = (float)raw_temperature / 524288;
    float temperature = scaled_raw_temperature * c1 + c0 / 2.0;
    float pressure = ((float)raw_pressure / 1572864);
    pressure = (int32_t)c00 + pressure * ((int32_t)c10 + pressure * ((int32_t)c20 + pressure * (int32_t)c30)) +
                 scaled_raw_temperature * ((int32_t)c01 + pressure * ((int32_t)c11 + pressure * (int32_t)c21));
    pressure /= 100.0;

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C, %f hPa", temperature,  pressure);
    return measurements_append_from_device(device, 0, METRIC_Pressure, timestamp, UNIT_hPa, pressure) &&
           measurements_append_from_device(device, 1, METRIC_Temperature, timestamp, UNIT_Cel, temperature);
}

uint8_t mlx_crc(uint8_t *buffer, int length)
{
    static const uint8_t crc_table[16] = {
        0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D
    };

    uint8_t crc = 0;
    for(int i = 0; i != length; i++) {
        crc ^= buffer[i];
        crc  = (crc << 4) ^ crc_table[crc >> 4];
        crc  = (crc << 4) ^ crc_table[crc >> 4];
    }
    return crc;
}

bool i2c_detect_mlx90614(device_bus_t bus, device_address_t address)
{
    uint8_t t_ambient_cmd[] = { 0x06 };
    uint8_t t_ambient_data[6] = { address << 1, t_ambient_cmd[0], address << 1 | 1 };
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, t_ambient_cmd, sizeof(t_ambient_cmd), t_ambient_data + 3, sizeof(t_ambient_data) - 3, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(mlx_crc(t_ambient_data, sizeof(t_ambient_data) - 1) != t_ambient_data[sizeof(t_ambient_data) - 1])
        return false;
    return true;
}

bool i2c_measure_mlx90614(devices_index_t device)
{
    uint8_t t_ambient_cmd[] = { 0x06 };
    uint8_t t_ambient_data[6] = { devices[device].address << 1, t_ambient_cmd[0], devices[device].address << 1 | 1 };
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, t_ambient_cmd, sizeof(t_ambient_cmd), t_ambient_data + 3, sizeof(t_ambient_data) - 3, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(mlx_crc(t_ambient_data, sizeof(t_ambient_data) - 1) != t_ambient_data[sizeof(t_ambient_data) - 1])
        return false;
    float ambient_temperature = (t_ambient_data[4] << 8 | t_ambient_data[3]) / 50.0 - 273.15;

    uint8_t t_obj1_cmd[] = { 0x07 };
    uint8_t t_obj1_data[6] = { devices[device].address << 1, t_obj1_cmd[0], devices[device].address << 1 | 1 };
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, t_obj1_cmd, sizeof(t_obj1_cmd), t_obj1_data + 3, sizeof(t_obj1_data) - 3, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(mlx_crc(t_obj1_data, sizeof(t_obj1_data) - 1) != t_obj1_data[sizeof(t_obj1_data) - 1])
        return false;
    float object_temperature = (t_obj1_data[4] << 8 | t_obj1_data[3]) / 50.0 - 273.15;

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C object, %f C ambient", object_temperature, ambient_temperature);
    return measurements_append_from_device(device, 0, METRIC_InfraredTemperature, timestamp, UNIT_Cel, object_temperature) &&
           measurements_append_from_device(device, 1, METRIC_InternalTemperature, timestamp, UNIT_Cel, ambient_temperature);
}

bool i2c_detect_mcp960x(device_bus_t bus, device_address_t address)
{
    uint8_t device_id_cmd[] = { 0x20 };
    uint8_t device_id_data[2];
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, device_id_cmd, sizeof(device_id_cmd), device_id_data, sizeof(device_id_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(device_id_data[0] != 0x40 && device_id_data[0] != 0x41)
        return false;

    uint8_t sensor_cfg_cmd[] = { 0x05, 0x00 };  // sets K-type thermocouple and filtering to off
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, sensor_cfg_cmd, sizeof(sensor_cfg_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    return true;
}

bool i2c_measure_mcp960x(devices_index_t device)
{
    uint8_t hot_junction_cmd[] = { 0x00 };
    uint8_t hot_junction_data[2];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, hot_junction_cmd, sizeof(hot_junction_cmd), hot_junction_data, sizeof(hot_junction_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    float probe_temperature = ((int16_t) hot_junction_data[0] << 8 | hot_junction_data[1]) * 0.0625;

    uint8_t cold_junction_cmd[] = { 0x02 };
    uint8_t cold_junction_data[2];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, cold_junction_cmd, sizeof(cold_junction_cmd), cold_junction_data, sizeof(cold_junction_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    float ambient_temperature = ((int16_t) cold_junction_data[0] << 8 | cold_junction_data[1]) * 0.0625;

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f C probe, %f C ambient", probe_temperature, ambient_temperature);
    return measurements_append_from_device(device, 0, METRIC_ProbeTemperature, timestamp, UNIT_Cel, probe_temperature) &&
           measurements_append_from_device(device, 1, METRIC_InternalTemperature, timestamp, UNIT_Cel, ambient_temperature);
}

bool i2c_detect_bh1750(device_bus_t bus, device_address_t address)
{
    uint8_t power_on_cmd[] = { 0x01 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, power_on_cmd, sizeof(power_on_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    uint8_t measure_cmd[] = { 0x20 };  // one time, high resolution
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, measure_cmd, sizeof(measure_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    return true;
}

bool i2c_measure_bh1750(devices_index_t device)
{
    uint8_t power_on_cmd[] = { 0x01 };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, power_on_cmd, sizeof(power_on_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    uint8_t measure_cmd[] = { 0x20 };  // one time, high resolution
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, measure_cmd, sizeof(measure_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(130 / portTICK_PERIOD_MS);
    uint8_t measure_data[2];
    if(i2c_master_read_from_device(i2c_buses[devices[device].bus].port, devices[device].address, measure_data, sizeof(measure_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    float lux = (measure_data[0] << 8 | measure_data[1]) / 1.2;

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f lux", lux);
    return measurements_append_from_device(device, 0, METRIC_LightIntensity, timestamp, UNIT_lux, lux);
}

bool i2c_detect_veml7700(device_bus_t bus, device_address_t address)
{
    uint8_t configuration_cmd[] = { 0x00, 0x00, 0x18 }; // 100ms integration, 1/4 gain
    return i2c_master_write_to_device(i2c_buses[bus].port, address, configuration_cmd, sizeof(configuration_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS) == ESP_OK;
}

bool i2c_measure_veml7700(devices_index_t device)
{
    if(NOW < 1000000)
        vTaskDelay(100 / portTICK_PERIOD_MS);  // await for complete integration after power up or waking up from sleep
    uint8_t als_cmd[] = { 0x04 };
    uint8_t als_data[2];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, als_cmd, sizeof(als_cmd), als_data, sizeof(als_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    float lux = (als_data[1] << 8 | als_data[0]) * 0.2304;
    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f lux", lux);
    return measurements_append_from_device(device, 0, METRIC_LightIntensity, timestamp, UNIT_lux, lux);
}

bool i2c_detect_tsl2591(device_bus_t bus, device_address_t address)
{
    if(NOW < 1000000)
        vTaskDelay(100 / portTICK_PERIOD_MS);  // await for complete integration after power up or waking up from sleep
    uint8_t id_cmd[] = { 0x12 | 0x80 };
    uint8_t id_data[1];
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, id_cmd, sizeof(id_cmd), id_data, sizeof(id_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if(id_data[0] != 0x50)
        return false;
    uint8_t control_cmd[] = { 0x01 | 0x80, 0x00 };  // 1x gain, 100 ms integration
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, control_cmd, sizeof(control_cmd), id_data, sizeof(id_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    uint8_t enable_cmd[] = { 0x00 | 0x80, 0x03 };
    if(i2c_master_write_read_device(i2c_buses[bus].port, address, enable_cmd, sizeof(enable_cmd), id_data, sizeof(id_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    return true;
}

bool i2c_measure_tsl2591(devices_index_t device)
{
    uint8_t read_als_cmd[] = { 0x14 | 0x80 };
    uint8_t read_als_data[4];
    if(i2c_master_write_read_device(i2c_buses[devices[device].bus].port, devices[device].address, read_als_cmd, sizeof(read_als_cmd), read_als_data, sizeof(read_als_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    uint16_t channel0 = read_als_data[1] << 8 | read_als_data[0];
    uint16_t channel1 = read_als_data[3] << 8 | read_als_data[2];

    // Alternate lux calculation, see https://github.com/adafruit/Adafruit_TSL2591_Library/issues/14
    float cpl = (100.0 * 1.0) / 408.0;     // integration time in ms * gain / lux coefficient
    float lux = (((float)channel0 - (float)channel1)) * (1.0F - ((float)channel1 / (float)channel0)) / cpl;

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f lux", lux);
    return measurements_append_from_device(device, 0, METRIC_LightIntensity, timestamp, UNIT_lux, lux);
 }


bool i2c_detect_scd4x(device_bus_t bus, device_address_t address)
{
    uint8_t stop_periodic_measurement_cmd[] = { 0x3f, 0x86 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, stop_periodic_measurement_cmd, sizeof(stop_periodic_measurement_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(500 / portTICK_PERIOD_MS);

    uint8_t reinit_cmd[] = { 0x36, 0x46 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, reinit_cmd, sizeof(reinit_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(30 / portTICK_PERIOD_MS);

    uint8_t serial_cmd[] = { 0x36, 0x82 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, serial_cmd, sizeof(serial_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t raw_buf[9];
    if(i2c_master_read_from_device(i2c_buses[bus].port, address, raw_buf, sizeof(raw_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;

    if (!sensirion_check_crc(raw_buf) || !sensirion_check_crc(raw_buf + 3) || !sensirion_check_crc(raw_buf + 6))
        return false;

    uint8_t start_periodic_measurement_cmd[] = { 0x21, 0xb1 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, start_periodic_measurement_cmd, sizeof(start_periodic_measurement_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;

    if(application.next_measurement_time < esp_timer_get_time() + 6000000L)
        application.next_measurement_time = esp_timer_get_time() + 6000000L;

    return true;
}

bool i2c_measure_scd4x(devices_index_t device)
{
    uint8_t read_measurement_cmd[] = { 0xec, 0x05 };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, read_measurement_cmd, sizeof(read_measurement_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t raw_buf[9];
    if(i2c_master_read_from_device(i2c_buses[devices[device].bus].port, devices[device].address, raw_buf, sizeof(raw_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if (!sensirion_check_crc(raw_buf) || !sensirion_check_crc(raw_buf + 3) || !sensirion_check_crc(raw_buf + 6))
        return false;

    float co2 = raw_buf[0] << 8 | raw_buf[1];
    float temperature = (((raw_buf[3] << 8 | raw_buf[4]) * 175) / 65535.0) - 45;
    float humidity = ((raw_buf[6] << 8 | raw_buf[7]) * 100) / 65535.0;
    humidity = humidity > 100 ? 100 : (humidity < 0 ? 0 : humidity);

    time_t timestamp = NOW;
    ESP_LOGI(__func__, "%f CO2 ppm, %f C, %f %%", co2, temperature, humidity);
    return measurements_append_from_device(device, 0, METRIC_CO2, timestamp, UNIT_ppm, co2) &&
           measurements_append_from_device(device, 1, METRIC_Temperature, timestamp, UNIT_Cel, temperature) &&
           measurements_append_from_device(device, 2, METRIC_Humidity, timestamp, UNIT_RH, humidity);
}

bool i2c_detect_sen5x(device_bus_t bus, device_address_t address)
{
    uint8_t reset_cmd[] = { 0xD3, 0x04 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, reset_cmd, sizeof(reset_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(100 / portTICK_PERIOD_MS);

    uint8_t product_name_cmd[] = { 0xD0, 0x14 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, product_name_cmd, sizeof(product_name_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t raw_buf[9];
    if(i2c_master_read_from_device(i2c_buses[bus].port, address, raw_buf, sizeof(raw_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;

    if (!sensirion_check_crc(raw_buf) || !sensirion_check_crc(raw_buf + 3) || !sensirion_check_crc(raw_buf + 6))
        return false;

    ESP_LOGI(__func__, "product name: %c%c%c%c%c", raw_buf[0], raw_buf[1], raw_buf[3], raw_buf[4], raw_buf[6]);

    uint8_t start_measurement_cmd[] = { 0x00, 0x21 };
    if(i2c_master_write_to_device(i2c_buses[bus].port, address, start_measurement_cmd, sizeof(start_measurement_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;

    return true;
}

bool i2c_measure_sen5x(devices_index_t device)
{
    uint8_t product_name_cmd[] = { 0xD0, 0x14 };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, product_name_cmd, sizeof(product_name_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t product_name_data[9];
    if(i2c_master_read_from_device(i2c_buses[devices[device].bus].port, devices[device].address, product_name_data, sizeof(product_name_data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if (!sensirion_check_crc(product_name_data) || !sensirion_check_crc(product_name_data + 3) || !sensirion_check_crc(product_name_data + 6))
        return false;

    uint8_t read_data_ready_flag_cmd[] = { 0x02, 0x02 };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, read_data_ready_flag_cmd, sizeof(read_data_ready_flag_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t flag_buf[3];
    if(i2c_master_read_from_device(i2c_buses[devices[device].bus].port, devices[device].address, flag_buf, sizeof(flag_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if (!sensirion_check_crc(flag_buf))
        return false;
    if(!flag_buf[1])
        vTaskDelay(1000 / portTICK_PERIOD_MS);

    uint8_t read_measurement_cmd[] = { 0x03, 0xC4 };
    if(i2c_master_write_to_device(i2c_buses[devices[device].bus].port, devices[device].address, read_measurement_cmd, sizeof(read_measurement_cmd), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    vTaskDelay(20 / portTICK_PERIOD_MS);

    uint8_t raw_buf[24];
    if(i2c_master_read_from_device(i2c_buses[devices[device].bus].port, devices[device].address, raw_buf, sizeof(raw_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS))
        return false;
    if (!sensirion_check_crc(raw_buf) || !sensirion_check_crc(raw_buf + 3) || !sensirion_check_crc(raw_buf + 6) || !sensirion_check_crc(raw_buf + 9) ||
        !sensirion_check_crc(raw_buf + 12) || !sensirion_check_crc(raw_buf + 15) || !sensirion_check_crc(raw_buf + 18) || !sensirion_check_crc(raw_buf + 21))
        return false;

    float pm1_0 = (raw_buf[0] << 8 | raw_buf[1]) / 10.0;
    float pm2_5 = (raw_buf[3] << 8 | raw_buf[4]) / 10.0;
    float pm4_0 = (raw_buf[6] << 8 | raw_buf[7]) / 10.0;
    float pm10_0 = (raw_buf[9] << 8 | raw_buf[10]) / 10.0;
    float humidity = ((int16_t)(raw_buf[12] << 8 | raw_buf[13])) / 100.0;
    float temperature = ((int16_t)(raw_buf[15] << 8 | raw_buf[16])) / 200.0;
    float voc = ((int16_t)(raw_buf[18] << 8 | raw_buf[19])) / 10.0;
    float nox = ((int16_t)(raw_buf[21] << 8 | raw_buf[22])) / 10.0;

    voc = voc < 1 || voc > 500 ? 1 : voc;
    nox = nox < 1 || nox > 500 ? 1 : nox;

    time_t timestamp = NOW;

    switch(product_name_data[6]) {
    case '0':
        ESP_LOGI(__func__, "PM 1.0 %.0f ug/m3, PM 2.5 %.0f ug/m3, PM 4.0 %.0f ug/m3, PM 10 %.0f ug/m3",
                            pm1_0, pm2_5, pm4_0, pm10_0);
        return  measurements_append_from_device(device, 0, METRIC_PM1,   timestamp, UNIT_ug_m3, pm1_0) &&
                measurements_append_from_device(device, 1, METRIC_PM2o5, timestamp, UNIT_ug_m3, pm2_5) &&
                measurements_append_from_device(device, 2, METRIC_PM4,   timestamp, UNIT_ug_m3, pm4_0) &&
                measurements_append_from_device(device, 3, METRIC_PM10,  timestamp, UNIT_ug_m3, pm10_0);
    case '4':
        ESP_LOGI(__func__, "PM 1.0 %.0f ug/m3, PM 2.5 %.0f ug/m3, PM 4.0 %.0f ug/m3, PM 10 %.0f ug/m3, %f C, %f %%, VOC %f",
                            pm1_0, pm2_5, pm4_0, pm10_0, temperature, humidity, voc);
        return  measurements_append_from_device(device, 0, METRIC_PM1,   timestamp, UNIT_ug_m3, pm1_0) &&
                measurements_append_from_device(device, 1, METRIC_PM2o5, timestamp, UNIT_ug_m3, pm2_5) &&
                measurements_append_from_device(device, 2, METRIC_PM4,   timestamp, UNIT_ug_m3, pm4_0) &&
                measurements_append_from_device(device, 3, METRIC_PM10,  timestamp, UNIT_ug_m3, pm10_0) &&
                measurements_append_from_device(device, 4, METRIC_Temperature, timestamp, UNIT_Cel, temperature) &&
                measurements_append_from_device(device, 5, METRIC_Humidity, timestamp, UNIT_RH, humidity) &&
                measurements_append_from_device(device, 6, METRIC_VOC, timestamp, UNIT_NONE, voc);
    case '5':
        ESP_LOGI(__func__, "PM 1.0 %.0f ug/m3, PM 2.5 %.0f ug/m3, PM 4.0 %.0f ug/m3, PM 10 %.0f ug/m3, %f C, %f %%, VOC %f, NOx %f",
                            pm1_0, pm2_5, pm4_0, pm10_0, temperature, humidity, voc, nox);
        return  measurements_append_from_device(device, 0, METRIC_PM1,   timestamp, UNIT_ug_m3, pm1_0) &&
                measurements_append_from_device(device, 1, METRIC_PM2o5, timestamp, UNIT_ug_m3, pm2_5) &&
                measurements_append_from_device(device, 2, METRIC_PM4,   timestamp, UNIT_ug_m3, pm4_0) &&
                measurements_append_from_device(device, 3, METRIC_PM10,  timestamp, UNIT_ug_m3, pm10_0) &&
                measurements_append_from_device(device, 4, METRIC_Temperature, timestamp, UNIT_Cel, temperature) &&
                measurements_append_from_device(device, 5, METRIC_Humidity, timestamp, UNIT_RH, humidity) &&
                measurements_append_from_device(device, 6, METRIC_VOC, timestamp, UNIT_NONE, voc) &&
                measurements_append_from_device(device, 7, METRIC_NOx, timestamp, UNIT_NONE, nox);
    default:
        return 0;
    }
}
