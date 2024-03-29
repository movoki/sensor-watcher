// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include <esp_log.h>
#include <esp_check.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "onewire_bus.h"
#include "onewire_cmd.h"
#include "onewire_crc.h"
#include "onewire_device.h"

#include "board.h"
#include "devices.h"
#include "measurements.h"
#include "now.h"
#include "onewire.h"
#include "wifi.h"

onewire_bus_t onewire_buses[ONEWIRE_BUSES_NUM_MAX] = {{0}};
uint8_t onewire_buses_count = 0;

void onewire_init()
{
    onewire_buses_count = 0;
    memset(onewire_buses, 0, sizeof(onewire_buses));
    onewire_read_from_nvs();
    if(!onewire_buses_count)
        onewire_set_default();
    onewire_start();
}

bool onewire_read_from_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;
    char nvs_key[24];

    err = nvs_open("onewire", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        ok = ok && !nvs_get_u8(handle, "count", &onewire_buses_count);
        for(uint8_t i = 0; i < onewire_buses_count && ok; i++) {
            snprintf(nvs_key, sizeof(nvs_key), "%u_data_pin", i);
            ok = ok && !nvs_get_u8(handle, nvs_key, &(onewire_buses[i].data_pin));
            snprintf(nvs_key, sizeof(nvs_key), "%u_power_pin", i);
            ok = ok && !nvs_get_u8(handle, nvs_key, &(onewire_buses[i].power_pin));
        }

        if(!ok)
            onewire_buses_count = 0;

        nvs_close(handle);
        ESP_LOGI(__func__, "%s, count = %i", ok ? "done" : "failed", onewire_buses_count);
        return ok;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

bool onewire_write_to_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;
    char nvs_key[16];

    err = nvs_open("onewire", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        for(uint8_t i = 0; i < onewire_buses_count && ok; i++) {
            snprintf(nvs_key, sizeof(nvs_key), "%u_data_pin", i % 255);
            ok = ok && !nvs_set_u8(handle, nvs_key, onewire_buses[i].data_pin);
            snprintf(nvs_key, sizeof(nvs_key), "%u_power_pin", i % 255);
            ok = ok && !nvs_set_u8(handle, nvs_key, onewire_buses[i].power_pin);
        }
        ok = ok && !nvs_set_u8(handle, "count", onewire_buses_count);
        ok = ok && !nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(__func__, "%s, count = %i", ok ? "done" : "failed", onewire_buses_count);
        return ok;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

uint32_t onewire_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;

    if(method == PM_GET) {
        ok = ok && bp_create_container(writer, BP_LIST);
        for(uint32_t i=0; i != onewire_buses_count && ok; i++) {
            ok = ok && bp_create_container(writer, BP_MAP);
            ok = ok && bp_put_string(writer, "data_pin") && bp_put_integer(writer, onewire_buses[i].data_pin);
            ok = ok && bp_put_string(writer, "power_pin") && bp_put_integer(writer, onewire_buses[i].power_pin);
            ok = ok && bp_finish_container(writer);
        }
        ok = ok && bp_finish_container(writer);
        return ok ? PM_205_Content : PM_500_Internal_Server_Error;
    }
    else if(method == PM_PUT) {
        if(!bp_close(reader) || !bp_next(reader) || !bp_is_list(reader))
            return PM_400_Bad_Request;
        else {
            onewire_stop();
            onewire_buses_count = 0;
            memset(onewire_buses, 0, sizeof(onewire_buses));
            if(bp_open(reader)) {
                while(ok && onewire_buses_count < ONEWIRE_BUSES_NUM_MAX && bp_next(reader)) {
                    ok = ok && bp_open(reader);
                    while(ok && bp_next(reader)) {
                        if(bp_match(reader, "data_pin"))
                            onewire_buses[onewire_buses_count].data_pin = bp_get_integer(reader);
                        else if(bp_match(reader, "power_pin"))
                            onewire_buses[onewire_buses_count].power_pin = bp_get_integer(reader);
                        else bp_next(reader);
                    }
                    bp_close(reader);
                    if(ok) {
                        if(onewire_buses[onewire_buses_count].data_pin< GPIO_NUM_MAX && (onewire_buses[onewire_buses_count].power_pin < GPIO_NUM_MAX || onewire_buses[onewire_buses_count].power_pin == 0xFF))
                            onewire_buses_count += 1;
                        else
                            ok = false;
                    }
                }
                bp_close(reader);
            }

            if(ok) {
                ok = ok && onewire_write_to_nvs();
                if(!onewire_buses_count)
                    onewire_set_default();
                onewire_start();
                return ok ? PM_204_Changed : PM_500_Internal_Server_Error;
            }
            else {
                onewire_buses_count = 0;
                memset(onewire_buses, 0, sizeof(onewire_buses));
                onewire_read_from_nvs();
                if(!onewire_buses_count)
                    onewire_set_default();
                onewire_start();
                return PM_400_Bad_Request;
            }
        }
    }
    else
        return PM_405_Method_Not_Allowed;
}


esp_err_t onewire_start()
{
    esp_err_t err = ESP_OK;

    onewire_set_power(true);
    for(int i = 0; i != onewire_buses_count; i++) {
        ESP_RETURN_ON_FALSE(!onewire_buses[i].handle, ESP_ERR_INVALID_STATE, __func__, "handle not NULL when starting 1-Wire bus");

        if(onewire_buses[i].power_pin != 0xFF) {
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = 1ULL << onewire_buses[i].power_pin;
            gpio_config(&io_conf);
            // gpio_set_drive_capability(onewire_buses[i].power_pin, GPIO_DRIVE_CAP_3);
        }
        onewire_bus_config_t bus_config = { .bus_gpio_num = onewire_buses[i].data_pin };
        onewire_bus_rmt_config_t rmt_config = { .max_rx_bytes = 20 };
        err = err ? err : onewire_new_bus_rmt(&bus_config, &rmt_config, (onewire_bus_handle_t *) &(onewire_buses[i].handle));
        ESP_LOGI(__func__, "starting 1-Wire bus %i on data:%i / power:%i %s",
            i, onewire_buses[i].data_pin, onewire_buses[i].power_pin, err ? "failed" : "done");
    }
    return err;
}

esp_err_t onewire_stop()
{
    esp_err_t err = ESP_OK;

    for(int i = 0; i != onewire_buses_count; i++) {
        err = onewire_bus_del((onewire_bus_handle_t)onewire_buses[i].handle);
        ESP_LOGI(__func__, "stopping 1-Wire bus %i %s", i, err ? "failed" : "done");
        onewire_buses[i].handle = NULL;
    }
    onewire_set_power(false);

    return err;
}

void onewire_set_power(bool state)
{
    for(int i = 0; i < onewire_buses_count; i++) {
        if(onewire_buses[i].power_pin != 0xFF)
            gpio_set_level(onewire_buses[i].power_pin, state);
    }
}

void onewire_set_default()
{
    switch(board.model) {
    case BOARD_MODEL_ADAFRUIT_ESP32_FEATHER_V2: // A0 A1
    case BOARD_MODEL_ADAFRUIT_QT_PY_ESP32_PICO: // A0 A1
        onewire_buses_count = 1;
        onewire_buses[0].data_pin = 26;
        onewire_buses[0].power_pin = 25;
        break;
    case BOARD_MODEL_ADAFRUIT_ESP32_S3_FEATHER: // A0 A1
    case BOARD_MODEL_ADAFRUIT_QT_PY_ESP32_S3:   // A0 A1
        onewire_buses_count = 1;
        onewire_buses[0].data_pin = 18;
        onewire_buses[0].power_pin = 17;
        break;
    case BOARD_MODEL_M5STACK_ATOM_LITE:         // PORT-B on AtomPortABC
    case BOARD_MODEL_M5STACK_ATOM_MATRIX:       // PORT-B on AtomPortABC
    case BOARD_MODEL_M5STACK_ATOM_ECHO:         // PORT-B on AtomPortABC
        onewire_buses_count = 1;
        onewire_buses[0].data_pin = 33;
        onewire_buses[0].power_pin = 23;
        break;
    case BOARD_MODEL_M5STACK_ATOM_U:
        onewire_buses_count = 1;
        onewire_buses[0].data_pin = 22;
        onewire_buses[0].power_pin = 21;
        break;
    case BOARD_MODEL_M5STACK_ATOMS3:            // PORT-B on AtomPortABC
    case BOARD_MODEL_M5STACK_ATOMS3_LITE:
        onewire_buses_count = 1;
        onewire_buses[0].data_pin = 8;
        onewire_buses[0].power_pin = 7;
        break;
    case BOARD_MODEL_M5STACK_M5STICKC:
    case BOARD_MODEL_M5STACK_M5STICKC_PLUS:
    case BOARD_MODEL_M5STACK_CORE2:             // PORT-B
    case BOARD_MODEL_M5STACK_CORE2_AWS:         // PORT-B
    case BOARD_MODEL_M5STACK_TOUGH:             // PORT-B
    case BOARD_MODEL_M5STACK_M5STATION_BAT:     // PORT-B2
    case BOARD_MODEL_M5STACK_M5STATION_485:     // PORT-B2
        onewire_buses_count = 1;
        onewire_buses[0].data_pin = 36;
        onewire_buses[0].power_pin = 26;
        break;
    case BOARD_MODEL_SEEEDSTUDIO_XIAO_ESP32S3:  // D0 D1
        onewire_buses_count = 1;
        onewire_buses[0].data_pin = 1;
        onewire_buses[0].power_pin = 2;
        break;
    case BOARD_MODEL_GENERIC_ESP32:
    default:
        break;
    }
}


// Devices

void onewire_detect_devices()
{
    for(uint8_t bus = 0; bus < onewire_buses_count; bus++) {
        if(onewire_buses[bus].handle) {
            onewire_device_t onewire_device;
            onewire_device_iter_handle_t iter = NULL;
            esp_err_t result = ESP_OK;

            if(onewire_new_device_iter((onewire_bus_handle_t) onewire_buses[bus].handle, &iter) != ESP_OK) {
                ESP_LOGI(__func__, "Device iterator creation failed");
                continue;
            }
            do {
                result = onewire_device_iter_get_next(iter, &onewire_device);
                if(result == ESP_OK) {
                    for(device_part_t part_index = 0; part_index < PART_NUM_MAX; part_index++) {
                        if(parts[part_index].resource == RESOURCE_ONEWIRE && parts[part_index].id_start == (onewire_device.address & 0xFF)) {
                            device_t device = {
                                .resource = RESOURCE_ONEWIRE,
                                .bus = bus,
                                .multiplexer = 0,
                                .channel = 0,
                                .address = onewire_device.address,
                                .part = part_index,
                                .mask = parts[part_index].mask,
                                .status = true,
                                .fixed = false,
                                .updated = -1,
                            };
                            int device_index = devices_get_or_append(&device);
                            if(device_index >= 0) {
                                char name[DEVICES_NAME_LENGTH];
                                devices_build_name(device_index, name, sizeof(name), '_');
                                ESP_LOGI(__func__, "Device found: %s", name);
                            }
                            else
                                ESP_LOGE(__func__, "DEVICES_NUM_MAX reached");
                            break;
                        }
                    }
                }
            } while (result != ESP_ERR_NOT_FOUND);
            onewire_del_device_iter(iter);
        }
    }
}

bool onewire_measure_device(devices_index_t device)
{
    bool ok = true;

    switch(devices[device].part) {
    case PART_DS18B20:
        ok = onewire_measure_ds18b20(device);
        break;
    case PART_TMP1826:
        ok = onewire_measure_tmp1826(device);
        break;
    default:
        ok = false;
    }

    if(ok)
        devices[device].updated = NOW;

    return ok;
}

esp_err_t onewire_send_command(onewire_bus_handle_t bus, uint64_t address, uint8_t command)
{
    uint8_t buffer[] = {
        ONEWIRE_CMD_MATCH_ROM,
        (address >> 0) & 0xFF,
        (address >> 8) & 0xFF,
        (address >> 16) & 0xFF,
        (address >> 24) & 0xFF,
        (address >> 32) & 0xFF,
        (address >> 40) & 0xFF,
        (address >> 48) & 0xFF,
        (address >> 56) & 0xFF,
        command
    };
    return onewire_bus_write_bytes(bus, buffer, sizeof(buffer));
}

#define DS18B20_CMD_CONVERT_TEMP      0x44
#define DS18B20_CMD_READ_SCRATCHPAD   0xBE

bool onewire_measure_ds18b20(devices_index_t device)
{
    uint8_t scratchpad[9];

    if(onewire_bus_reset(onewire_buses[devices[device].bus].handle) != ESP_OK) {
        ESP_LOGE(__func__, "bus %i reset failed", devices[device].bus);
        return false;
    }
    if(onewire_send_command(onewire_buses[devices[device].bus].handle, devices[device].address, DS18B20_CMD_CONVERT_TEMP) != ESP_OK) {
        ESP_LOGE(__func__, "send DS18B20_CMD_CONVERT_TEMP command to address %016llX in bus %i failed", devices[device].address, devices[device].bus);
        return false;
    }
    vTaskDelay (760 / portTICK_PERIOD_MS);  // for 12-bits resolution

    if(onewire_bus_reset(onewire_buses[devices[device].bus].handle) != ESP_OK) {
        ESP_LOGE(__func__, "bus %i reset failed", devices[device].bus);
        return false;
    }
    if(onewire_send_command(onewire_buses[devices[device].bus].handle, devices[device].address, DS18B20_CMD_READ_SCRATCHPAD) != ESP_OK) {
        ESP_LOGE(__func__, "send DS18B20_CMD_READ_SCRATCHPAD command to address %016llX in bus %i failed", devices[device].address, devices[device].bus);
        return false;
    }
    if(onewire_bus_read_bytes(onewire_buses[devices[device].bus].handle, scratchpad, sizeof(scratchpad)) != ESP_OK) {
        ESP_LOGE(__func__, "read scratchpad from address %016llX in bus %i failed", devices[device].address, devices[device].bus);
        return false;
    }
    if(onewire_crc8(0, scratchpad, 8) != scratchpad[8]) {
        ESP_LOGE(__func__, "scratchpad CRC error for address %016llX in bus %i", devices[device].address, devices[device].bus);
        return false;
    }

    float temperature = (((int16_t)scratchpad[1] << 8) | scratchpad[0])  / 16.0f;

    ESP_LOGI(__func__, "%f C", temperature);
    return measurements_append_from_device(device, 0, METRIC_Temperature, NOW, UNIT_Cel, temperature);
}


#define TMP1826_CMD_CONVERT_TEMP      0x44
#define TMP1826_CMD_READ_SCRATCHPAD   0xBE

bool onewire_measure_tmp1826(devices_index_t device)
{
    uint8_t scratchpad[18];

    if(onewire_bus_reset(onewire_buses[devices[device].bus].handle) != ESP_OK) {
        ESP_LOGE(__func__, "bus %i reset failed", devices[device].bus);
        return false;
    }
    if(onewire_send_command(onewire_buses[devices[device].bus].handle, devices[device].address, TMP1826_CMD_CONVERT_TEMP) != ESP_OK) {
        ESP_LOGE(__func__, "send TMP1826_CMD_CONVERT_TEMP command to address %016llX in bus %i failed", devices[device].address, devices[device].bus);
        return false;
    }
    vTaskDelay (20 / portTICK_PERIOD_MS);

    if(onewire_bus_reset(onewire_buses[devices[device].bus].handle) != ESP_OK) {
        ESP_LOGE(__func__, "bus %i reset failed", devices[device].bus);
        return false;
    }
    if(onewire_send_command(onewire_buses[devices[device].bus].handle, devices[device].address, TMP1826_CMD_READ_SCRATCHPAD) != ESP_OK) {
        ESP_LOGE(__func__, "send TMP1826_CMD_READ_SCRATCHPAD command to address %016llX in bus %i failed", devices[device].address, devices[device].bus);
        return false;
    }
    if(onewire_bus_read_bytes(onewire_buses[devices[device].bus].handle, scratchpad, sizeof(scratchpad)) != ESP_OK) {
        ESP_LOGE(__func__, "read scratchpad from address %016llX in bus %i failed", devices[device].address, devices[device].bus);
        return false;
    }
    if(onewire_crc8(0, scratchpad, 8) != scratchpad[8]) {
        ESP_LOGE(__func__, "scratchpad CRC error for address %016llX in bus %i", devices[device].address, devices[device].bus);
        return false;
    }

    float temperature = (((int16_t)scratchpad[1] << 8) | scratchpad[0])  / 16.0f;

    ESP_LOGI(__func__, "%f C", temperature);
    return measurements_append_from_device(device, 0, METRIC_Temperature, NOW, UNIT_Cel, temperature);
}

