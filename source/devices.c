// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "ble.h"
#include "devices.h"
#include "enums.h"
#include "i2c.h"
#include "measurements.h"
#include "now.h"
#include "onewire.h"

RTC_DATA_ATTR device_t devices[DEVICES_NUM_MAX] = {{0}};
RTC_DATA_ATTR devices_index_t devices_count = 0;

const part_t parts[PART_NUM_MAX] = {
    [PART_NONE]            { .label = "",          .resource = RESOURCE_NONE,    .id_start = 0,    .id_span = 0, .parameters=0, .mask = 0 },
    [PART_SHT3X]           { .label = "SHT3X",     .resource = RESOURCE_I2C,     .id_start = 0x44, .id_span = 2, .parameters=2, .mask = 0 },
    [PART_SHT4X]           { .label = "SHT4X",     .resource = RESOURCE_I2C,     .id_start = 0x44, .id_span = 1, .parameters=2, .mask = 0 },
    [PART_HTU21D]          { .label = "HTU21D",    .resource = RESOURCE_I2C,     .id_start = 0x40, .id_span = 1, .parameters=2, .mask = 0 },
    [PART_HTU31D]          { .label = "HTU31D",    .resource = RESOURCE_I2C,     .id_start = 0x40, .id_span = 2, .parameters=2, .mask = 0 },
    [PART_MCP9808]         { .label = "MCP9808",   .resource = RESOURCE_I2C,     .id_start = 0x18, .id_span = 8, .parameters=1, .mask = 0 },
    [PART_TMP117]          { .label = "TMP117",    .resource = RESOURCE_I2C,     .id_start = 0x48, .id_span = 4, .parameters=1, .mask = 0 },
    [PART_BMP280]          { .label = "BMP280",    .resource = RESOURCE_I2C,     .id_start = 0x76, .id_span = 2, .parameters=2, .mask = 0 },
    [PART_BMP388]          { .label = "BMP388",    .resource = RESOURCE_I2C,     .id_start = 0x76, .id_span = 2, .parameters=2, .mask = 0 },
    [PART_LPS2X3X]         { .label = "LPS2X3X",   .resource = RESOURCE_I2C,     .id_start = 0x5C, .id_span = 2, .parameters=2, .mask = 0 },
    [PART_DPS310]          { .label = "DPS310",    .resource = RESOURCE_I2C,     .id_start = 0x76, .id_span = 2, .parameters=2, .mask = 0 },
    [PART_MLX90614]        { .label = "MLX90614",  .resource = RESOURCE_I2C,     .id_start = 0x5A, .id_span = 1, .parameters=2, .mask = 0 },
    [PART_MCP960X]         { .label = "MCP960X",   .resource = RESOURCE_I2C,     .id_start = 0x60, .id_span = 8, .parameters=2, .mask = 0 },
    [PART_BH1750]          { .label = "BH1750",    .resource = RESOURCE_I2C,     .id_start = 0x23, .id_span = 1, .parameters=1, .mask = 0 },
    [PART_VEML7700]        { .label = "VEML7700",  .resource = RESOURCE_I2C,     .id_start = 0x10, .id_span = 1, .parameters=1, .mask = 0 },
    [PART_TSL2591]         { .label = "TSL2591",   .resource = RESOURCE_I2C,     .id_start = 0x29, .id_span = 1, .parameters=1, .mask = 0 },
    [PART_SCD4X]           { .label = "SCD4X",     .resource = RESOURCE_I2C,     .id_start = 0x62, .id_span = 1, .parameters=3, .mask = 0 },
    [PART_SEN5X]           { .label = "SEN5X",     .resource = RESOURCE_I2C,     .id_start = 0x69, .id_span = 1, .parameters=8, .mask = 0 },
    [PART_DS18B20]         { .label = "DS18B20",   .resource = RESOURCE_ONEWIRE, .id_start = 0x28, .id_span = 1, .parameters=1, .mask = 0 },
    [PART_TMP1826]         { .label = "TMP1826",   .resource = RESOURCE_ONEWIRE, .id_start = 0x26, .id_span = 1, .parameters=1, .mask = 0 },
    [PART_RUUVITAG]        { .label = "RuuviTag",  .resource = RESOURCE_BLE,     .id_start = 0x00, .id_span = 0, .parameters=9, .mask = 0x0007 },
    [PART_MINEW_S1]        { .label = "MinewS1",   .resource = RESOURCE_BLE,     .id_start = 0x00, .id_span = 0, .parameters=3, .mask = 0x0003 },
    [PART_XIAOMI_LYWSDCGQ] { .label = "LYWSDCGQ",  .resource = RESOURCE_BLE,     .id_start = 0x00, .id_span = 0, .parameters=3, .mask = 0x0003 },
};

void devices_init()
{
    devices_count = 0;
    memset(devices, 0, sizeof(devices));
    devices_read_from_nvs();
    ESP_LOGI(__func__, "devices_read_from_nvs ended @ %lli", esp_timer_get_time());
    devices_detect_all();  
}

bool devices_read_from_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;
    char nvs_key[16];
    devices_index_t devices_fixed_count = 0;
    size_t length;

    err = nvs_open("devices", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        ok = ok && !nvs_get_u8(handle, "count", &devices_fixed_count);
        ESP_LOGI(__func__, "Fixed devices found in NVS: %i", devices_fixed_count);
        for(uint8_t i = 0; i < devices_fixed_count && ok; i++) {
            device_t device = {
                .rssi = 0,
                .updated = -1,
                .status = true,
                .fixed = true,
            };
            snprintf(nvs_key, sizeof(nvs_key), "%u_resource", i % 255);
            ok = ok && !nvs_get_u8(handle, nvs_key, &device.resource);
            snprintf(nvs_key, sizeof(nvs_key), "%u_bus", i % 255);
            ok = ok && !nvs_get_u8(handle, nvs_key, &(device.bus));
            snprintf(nvs_key, sizeof(nvs_key), "%u_multiplexer", i % 255);
            ok = ok && !nvs_get_u8(handle, nvs_key, &(device.multiplexer));
            snprintf(nvs_key, sizeof(nvs_key), "%u_channel", i % 255);
            ok = ok && !nvs_get_u8(handle, nvs_key, &(device.channel));
            snprintf(nvs_key, sizeof(nvs_key), "%u_address", i % 255);
            ok = ok && !nvs_get_u64(handle, nvs_key, &(device.address));
            snprintf(nvs_key, sizeof(nvs_key), "%u_part", i % 255);
            ok = ok && !nvs_get_u16(handle, nvs_key, &(device.part));


            snprintf(nvs_key, sizeof(nvs_key), "%u_mask", i % 255);
            ok = ok && !nvs_get_u16(handle, nvs_key, &(device.mask));
            snprintf(nvs_key, sizeof(nvs_key), "%u_offsets", i % 255);
            length = sizeof(device.offsets);
            ok = ok && !nvs_get_blob(handle, nvs_key, device.offsets, &length);

            ok = ok && devices_append(&device) >= 0;
            ESP_LOGI(__func__, "device %i: %s", i, ok ? "ok" : "fail");
        }

        if(!ok) {
            memset(devices, 0, sizeof(devices));
            devices_count = 0;
        }

        nvs_close(handle);
        ESP_LOGI(__func__, "%s", ok ? "done" : "failed");
        return ok;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

device_address_t hex_to_address(const char* hex)
{
    uint8_t i, j;
    uint8_t bytes[sizeof(device_address_t)];
    size_t  hex_length = strlen(hex);

    for(i = 0, j = 0; i < sizeof(bytes) && j + 1 < hex_length; ++i, j+=2)
        bytes[i] = (((hex[j+0] & 0xf) + (hex[j+0] >> 6) * 9) << 4) | ((hex[j+1] & 0xf) + (hex[j+1] >> 6) * 9);
    return (uint64_t) bytes[0] << 56 | (uint64_t) bytes[1] << 48 | (uint64_t) bytes[2] << 40 | (uint64_t) bytes[3] << 32 |
           (uint64_t) bytes[4] << 24 | (uint64_t) bytes[5] << 16 | (uint64_t) bytes[6] << 8  | (uint64_t) bytes[7];
}

bool devices_write_to_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;
    char nvs_key[16];
    devices_index_t devices_fixed_count = 0;

    err = nvs_open("devices", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        for(uint8_t i = 0; i < devices_count && ok; i++) {
            if(devices[i].fixed) {
                snprintf(nvs_key, sizeof(nvs_key), "%u_resource", i % 255);
                ok = ok && !nvs_set_u8(handle, nvs_key, devices[i].resource);
                snprintf(nvs_key, sizeof(nvs_key), "%u_bus", i % 255);
                ok = ok && !nvs_set_u8(handle, nvs_key, devices[i].bus);
                snprintf(nvs_key, sizeof(nvs_key), "%u_multiplexer", i % 255);
                ok = ok && !nvs_set_u8(handle, nvs_key, devices[i].multiplexer);
                snprintf(nvs_key, sizeof(nvs_key), "%u_channel", i % 255);
                ok = ok && !nvs_set_u8(handle, nvs_key, devices[i].channel);
                snprintf(nvs_key, sizeof(nvs_key), "%u_address", i % 255);
                ok = ok && !nvs_set_u64(handle, nvs_key, devices[i].address);
                snprintf(nvs_key, sizeof(nvs_key), "%u_part", i % 255);
                ok = ok && !nvs_set_u16(handle, nvs_key, devices[i].part);

                snprintf(nvs_key, sizeof(nvs_key), "%u_mask", i % 255);
                ok = ok && !nvs_set_u16(handle, nvs_key, devices[i].mask);
                snprintf(nvs_key, sizeof(nvs_key), "%u_offsets", i % 255);
                ok = ok && !nvs_set_blob(handle, nvs_key, devices[i].offsets, sizeof(devices[i].offsets));

                devices_fixed_count += 1;
            }
        }
        ok = ok && !nvs_set_u8(handle, "count", devices_fixed_count);
        ok = ok && !nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(__func__, "%s", ok ? "done" : "failed");
        return ok;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

void devices_build_name(devices_index_t device, char *name, size_t name_size, char separator)
{
    switch(devices[device].resource) {
    case RESOURCE_I2C:
    case RESOURCE_ONEWIRE:
    case RESOURCE_BLE:
        snprintf(name, name_size, "%s%c%i%c%i%c%i%c%016llX%c%s",
            resource_labels[devices[device].resource],
            separator,
            devices[device].bus,
            separator,
            devices[device].multiplexer,
            separator,
            devices[device].channel,
            separator,
            devices[device].address,
            separator,
            parts[devices[device].part].label);
        break;
    default:
    }
}

bool devices_parse_name(char *name, device_t *device, char separator)
{
    int i, count;
    char *items[6];
    for(i = 0, count = 0; name[i]; i++) {
        if(!i)
            items[count++] = name;
        if(name[i] == separator) {
            items[count++] = name + i + 1;
            name[i] = 0;
        }
    }
    if(count != 6)
        return false;

    for(device->resource = 0; device->resource < RESOURCE_NUM_MAX; device->resource++)
        if(!strcmp(items[0], resource_labels[device->resource]))
            break;
    if(device->resource == RESOURCE_NUM_MAX)
        return false;

    for(device->part = 0; device->part < PART_NUM_MAX; device->part++)
        if(!strcmp(items[5], parts[device->part].label))
            break;
    if(device->part == PART_NUM_MAX)
        return false;

    device->bus = atoi(items[1]);
    device->multiplexer = atoi(items[2]);
    device->channel = atoi(items[3]);
    device->address = hex_to_address(items[4]);

    return true;
}

int devices_get(device_t *device)
{
    for(int i = 0; i < devices_count; i++) {
        if(devices[i].resource == device->resource && devices[i].bus == device->bus && devices[i].multiplexer == device->multiplexer &&
           devices[i].channel == device->channel && devices[i].address == device->address && devices[i].part == device->part)
            return i;
    }
    return -1;
}

int devices_append(device_t *device)
{
    if(devices_count < DEVICES_NUM_MAX) {
        memcpy(&devices[devices_count], device, sizeof(device_t));
        devices_count += 1;
        return devices_count - 1;
    }
    else
        return -1;
}

int devices_get_or_append(device_t *device)
{
    int device_index = devices_get(device);
    return device_index >= 0 ? device_index : devices_append(device);
}

uint32_t devices_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;
    char name[DEVICES_NAME_LENGTH];
    char address[sizeof(device_address_t) * 2 + 1];

    if(method == PM_GET) {
        bp_create_container(writer, BP_LIST);
        for(devices_index_t i = 0; i < devices_count && ok; i++) {
            devices_build_name(i, name, sizeof(name), '_');
            snprintf(address, sizeof(address), "%016llX", devices[i].address);
            ok = ok && bp_create_container(writer, BP_MAP);
                ok = ok && bp_put_string(writer, "id");
                ok = ok && bp_put_integer(writer, i);
                ok = ok && bp_put_string(writer, "fixed");
                ok = ok && bp_put_boolean(writer, devices[i].fixed);
                ok = ok && bp_put_string(writer, "name");
                ok = ok && bp_put_string(writer, name);
                ok = ok && bp_put_string(writer, "mask");
                ok = ok && bp_put_integer(writer, (bp_integer_t) devices[i].mask);
                ok = ok && bp_put_string(writer, "offsets");
                ok = ok && bp_create_container(writer, BP_LIST);
                    for(int j = 0; j < parts[devices[i].part].parameters; j++)
                        ok = ok && bp_put_float(writer, (bp_integer_t) devices[i].offsets[j]);
                ok = ok && bp_finish_container(writer);

                ok = ok && bp_put_string(writer, "rssi");
                ok = ok && bp_put_integer(writer, devices[i].rssi);
                ok = ok && bp_put_string(writer, "updated");
                ok = ok && bp_put_big_integer(writer, devices[i].updated);
                ok = ok && bp_put_string(writer, "status");
                ok = ok && bp_put_boolean(writer, devices[i].status);
            ok = ok && bp_finish_container(writer);
        }
        ok = ok && bp_finish_container(writer);
        return ok ? PM_205_Content : PM_500_Internal_Server_Error;
    }
    else if(method == PM_POST) {
        if(!bp_close(reader) || !bp_next(reader) || !bp_is_map(reader) || !bp_open(reader))
                return PM_400_Bad_Request;

        device_t device = {
            .status = true,
            .updated = -1,
        };

        while(ok && bp_next(reader)) {
            if(bp_match(reader, "name")) {
                bp_get_string(reader, name, sizeof(name) / sizeof(bp_type_t));
                devices_parse_name(name, &device, '_');
            }
            else if(bp_match(reader, "fixed"))
                device.fixed = bp_get_boolean(reader);
            else if(bp_match(reader, "mask"))
                device.mask = bp_get_integer(reader);
            else if(bp_match(reader, "offsets")) {
                if(bp_open(reader)) {
                    for(int p = 0; p < DEVICES_PARAMETERS_NUM_MAX; p++)
                        device.offsets[p] = bp_next(reader) ? bp_get_float(reader) : 0.0;
                    bp_close(reader);
                }
            }
            else bp_next(reader);
        }
        bp_close(reader);

        if(!device.resource || !device.address || !device.part)  /// there are valid zero addresses?
            return PM_400_Bad_Request;

        return devices_append(&device) >= 0 && devices_write_to_nvs() ? PM_201_Created : PM_500_Internal_Server_Error;
    }
    else if(method == PM_PUT) {
        int index;
        if(!bp_next(reader) || !bp_is_integer(reader) || (index = bp_get_integer(reader)) > devices_count - 1 || index < 0 ||
           !bp_close(reader) || !bp_next(reader) || !bp_is_map(reader) || !bp_open(reader))
                return PM_400_Bad_Request;

        while(bp_next(reader)) {
            if(bp_match(reader, "fixed"))
                devices[index].fixed = bp_get_boolean(reader);
            else if(bp_match(reader, "mask"))
                devices[index].mask = bp_get_integer(reader);
            else if(bp_match(reader, "offsets")) {
                if(bp_open(reader)) {
                    for(int p = 0; p < DEVICES_PARAMETERS_NUM_MAX; p++)
                        devices[index].offsets[p] = bp_next(reader) ? bp_get_float(reader) : 0.0;
                    bp_close(reader);
                }
                else
                    memset(devices[index].offsets, 0, sizeof(devices[index].offsets));
            }
            else bp_next(reader);
        }
        bp_close(reader);
        return devices_write_to_nvs() ? PM_204_Changed : PM_500_Internal_Server_Error;
    }
    else
        return PM_405_Method_Not_Allowed;
}

void devices_buses_init()
{
    i2c_init();
    onewire_init();
}

void devices_buses_start()
{
    i2c_start();
    onewire_start();
}

void devices_buses_stop()
{
    i2c_stop();
    onewire_stop();
}

void devices_detect_all()
{
    i2c_detect_devices();
    ESP_LOGI(__func__, "i2c_detect_devices ended @ %lli", esp_timer_get_time());
    onewire_detect_devices();
    ESP_LOGI(__func__, "onewire_detect_devices ended @ %lli", esp_timer_get_time());
}

bool devices_measure_all()
{
    bool ok = true;
    uint8_t device;

    for(device = 0; device < devices_count; device++) {
        switch(devices[device].resource) {
        case RESOURCE_I2C:
            devices[device].status = i2c_measure_device(device);
            ok = ok && devices[device].status;
            break;
        case RESOURCE_ONEWIRE:
            devices[device].status = onewire_measure_device(device);
            ok = ok && devices[device].status;
            break;
        default:
            break;
        }
    }
    return ok;
}
