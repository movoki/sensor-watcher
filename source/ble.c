// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <time.h>
#include <nvs_flash.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_gap.h>

#include "ble.h"
#include "devices.h"
#include "enums.h"
#include "measurements.h"

#define BLE_ADDR_TYPE_PUBLIC 0x00

ble_t ble;
uint32_t ble_measurements_count = 0;
ble_measurement_t ble_measurements[BLE_MEASUREMENTS_NUM_MAX] = {{0}};


bool ble_init()
{
    ble.enabled = false;
    ble.error = ESP_OK;
    ble.minimum_rssi = -100;
    ble.scan_duration = 45;

    ble_read_from_nvs();

    if(ble.enabled) {
        if((ble.error = nimble_port_init()) != ESP_OK) {
            ESP_LOGE(__func__, "Failed to init nimble %d ", ble.error);
            return false;
        }
        nimble_port_freertos_init(ble_host_task);
    }

    return true;
}

bool ble_read_from_nvs()
{
    nvs_handle_t handle;

    if(nvs_open("ble", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_get_u8(handle, "enabled", (uint8_t *) &(ble.enabled));
        nvs_get_i8(handle, "minimum_rssi", &(ble.minimum_rssi));
        nvs_get_u8(handle, "scan_duration", &(ble.scan_duration));
        nvs_close(handle);
        ESP_LOGI(__func__, "done");
        return true;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

bool ble_write_to_nvs()
{
    bool ok = true;
    nvs_handle_t handle;

    if(nvs_open("ble", NVS_READWRITE, &handle) == ESP_OK) {
        ok = ok && !nvs_set_u8(handle, "enabled", ble.enabled);
        ok = ok && !nvs_set_i8(handle, "minimum_rssi", ble.minimum_rssi);
        ok = ok && !nvs_set_u8(handle, "scan_duration", ble.scan_duration);
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

uint32_t ble_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;
    uint32_t response = 0;

    if(method == PM_GET) {
        ok = ok && bp_create_container(writer, BP_MAP);
            ok = ok && bp_put_string(writer, "enabled");
            ok = ok && bp_put_boolean(writer, ble.enabled);
            ok = ok && bp_put_string(writer, "minimum_rssi");
            ok = ok && bp_put_integer(writer, ble.minimum_rssi);
            ok = ok && bp_put_string(writer, "scan_duration");
            ok = ok && bp_put_integer(writer, ble.scan_duration);
        ok = ok && bp_finish_container(writer);
        response = ok ? PM_205_Content : PM_500_Internal_Server_Error;

    }
    else if(method == PM_PUT) {
        if(!bp_close(reader) || !bp_next(reader) || !bp_is_map(reader) || !bp_open(reader))
            response = PM_400_Bad_Request;
        else {
            while(bp_next(reader)) {
                if(bp_match(reader, "enabled"))
                    ble.enabled = bp_get_boolean(reader);
                else if(bp_match(reader, "minimum_rssi"))
                    ble.minimum_rssi = bp_get_integer(reader);
                else if(bp_match(reader, "scan_duration"))
                    ble.scan_duration = bp_get_integer(reader);
                else bp_next(reader);
            }
            bp_close(reader);
            ok = ok && ble_write_to_nvs();
            response = ok ? PM_204_Changed : PM_500_Internal_Server_Error;
        }
    }
    else
        response = PM_405_Method_Not_Allowed;

    return response;
}


void ble_handle_adv(device_address_t address, device_rssi_t rssi, const uint8_t *data, uint8_t length)
{
    device_part_t part = 0;
    const uint8_t minew_s1_prefix[] = { 0x02, 0x01, 0x06, 0x03, 0x03, 0xe1, 0xff, 0x10, 0x16, 0xe1, 0xff, 0xa1, 0x01 };

    if(rssi < ble.minimum_rssi)
        return;

    if(length == 31 && data[5] == 0x99 && data[6] == 0x04 && data[7] == 0x05)           // Ruvitag 5 (Raw V2)
        part = PART_RUUVITAG;
    else if(data[5] == 0x95 && data[6] == 0xFE && data[7] == 0x50 && data[8] == 0x20 && data[9] == 0xAA && data[10] == 0x01)  // Xiaomi LYWSDCGQ
        part = PART_XIAOMI_LYWSDCGQ;
    else if(length == 24 && !memcmp(data, minew_s1_prefix, sizeof(minew_s1_prefix)))    // Minew S1 - HT frame
        part = PART_MINEW_S1;
    else
        return;

    device_t device = {
        .resource = RESOURCE_BLE,
        .bus = 0,
        .multiplexer = 0,
        .channel = 0,
        .address = address,
        .part = part,
        .mask = parts[part].mask,
        .status = true,
        .fixed = false,
        .updated = -1,
    };

    int device_index = devices_get_or_append(&device);
    if(device_index < 0) {
        ESP_LOGI(__func__, "Cannot add discovered BLE device %s %016llX", parts[part].label, address);
        return;
    }

    time_t timestamp = time(NULL);
    devices[device_index].rssi = rssi;
    devices[device_index].updated = timestamp;
    devices[device_index].status = true;

    switch(part) {
    case PART_RUUVITAG: {
        float temperature = (int16_t)((data[8] << 8) | data[9]) * 0.005;
        float humidity = ((data[10] << 8) | data[11]) * 0.0025;
        float pressure = (((data[12] << 8) | data[13]) + 50000) / 100.0;
        float acceleration_x = (int16_t)((data[14] << 8) | data[15]) / 1000.0 * 9.80665;
        float acceleration_y = (int16_t)((data[16] << 8) | data[17]) / 1000.0 * 9.80665;
        float acceleration_z = (int16_t)((data[18] << 8) | data[19]) / 1000.0 * 9.80665;
        float battery = ((uint16_t)((data[20] << 3) | (data[21] >> 5)) + 1600) / 1000.0;
        uint8_t movements = data[22];

        ble_measurements_update(device_index, 0, METRIC_Temperature,   timestamp, UNIT_Cel,  temperature);
        ble_measurements_update(device_index, 1, METRIC_Humidity,      timestamp, UNIT_RH,   humidity > 100.0 ? 100.0 : humidity);
        ble_measurements_update(device_index, 2, METRIC_Pressure,      timestamp, UNIT_hPa,  pressure);
        ble_measurements_update(device_index, 3, METRIC_Movements,     timestamp, UNIT_NONE, movements);
        ble_measurements_update(device_index, 4, METRIC_AccelerationX, timestamp, UNIT_m_s2, acceleration_x);
        ble_measurements_update(device_index, 5, METRIC_AccelerationY, timestamp, UNIT_m_s2, acceleration_y);
        ble_measurements_update(device_index, 6, METRIC_AccelerationZ, timestamp, UNIT_m_s2, acceleration_z);
        ble_measurements_update(device_index, 7, METRIC_BatteryLevel,  timestamp, UNIT_V,    battery);
        ble_measurements_update(device_index, 8, METRIC_RSSI,          timestamp, UNIT_dBm,  rssi);
        break;
    }
    case PART_XIAOMI_LYWSDCGQ: {
        if(data[18] == 0x0D && data[19] == 0x10 && data[20] == 0x04) {   // 0x100D frame, temperature & humidity
            float temperature = (int16_t)(((uint16_t)data[22] << 8) | data[21]) / 10.0;
            float humidity = (int16_t)(((uint16_t)data[24] << 8) | data[23]) / 10.0;

            ble_measurements_update(device_index, 0, METRIC_Temperature,   timestamp, UNIT_Cel,   temperature);
            ble_measurements_update(device_index, 1, METRIC_Humidity,      timestamp, UNIT_RH,    humidity > 100.0 ? 100.0 : humidity);
        }
        else if(data[18] == 0x0A && data[19] == 0x10 && data[20] == 0x01)   // 0x100A frame, battery level
            ble_measurements_update(device_index, 2, METRIC_BatteryLevel,  timestamp, UNIT_ratio, data[21] / 100.0);
        break;
    }
    case PART_MINEW_S1: {
        float temperature = (int16_t)(((uint16_t)data[14] << 8) | data[15]) / 256.0;
        float humidity = (int16_t)(((uint16_t)data[16] << 8) | data[17]) / 256.0;

        ble_measurements_update(device_index, 0, METRIC_Temperature,   timestamp, UNIT_Cel,   temperature);
        ble_measurements_update(device_index, 1, METRIC_Humidity,      timestamp, UNIT_RH,    humidity > 100.0 ? 100.0 : humidity);
        ble_measurements_update(device_index, 2, METRIC_BatteryLevel,  timestamp, UNIT_ratio, data[13] / 100.0);
        break;
    }
    default:
        break;
    }
    return;
}

int ble_gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        uint64_t address = (uint64_t)event->disc.addr.val[0] << 0  | (uint64_t)event->disc.addr.val[1] << 8  |
                           (uint64_t)event->disc.addr.val[2] << 16 | (uint64_t)event->disc.addr.val[3] << 40 |
                           (uint64_t)event->disc.addr.val[4] << 48 | (uint64_t)event->disc.addr.val[5] << 56 | 0x000000FFFF000000;
        ble_handle_adv(address, event->disc.rssi, event->disc.data, event->disc.length_data);
        // esp_log_buffer_hex("BLE ADV:", event->disc.data,  event->disc.length_data);
        break;
    // case BLE_GAP_EVENT_EXT_DISC:
    //     ble_handle_adv(address, event->disc.rssi, event->disc.data, event->disc.length_data);
    //     break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(__func__, "discovery complete; reason=%d\n", event->disc_complete.reason);
        break;
    default:
        break;
    }

    return 0;
}

void ble_host_task(void *param)
{
    ESP_LOGI(__func__, "BLE Host Task Started");
    nimble_port_run();              // This function will return only when nimble_port_stop() is executed
    nimble_port_freertos_deinit();
}


bool ble_start_scan()
{
    ble_measurements_count = 0;

    struct ble_gap_disc_params disc_params = {
        .filter_duplicates = 0,
        .passive = 1,
        .itvl = 0,
        .window = 0,
        .filter_policy = 0,
        .limited = 0,
    };
    if((ble.error = ble_gap_disc(BLE_ADDR_TYPE_PUBLIC, BLE_HS_FOREVER, &disc_params, ble_gap_event_handler, NULL)) != 0) {
        ESP_LOGE(__func__, "Error initiating GAP discovery procedure; err=%d\n", ble.error);
        return false;
    }
    return true;
}

bool ble_is_scanning()
{
    return ble_gap_disc_active() == 1;
}

bool ble_stop_scan()
{
    return ble_gap_disc_cancel() == 0;
}

bool ble_measurements_update(devices_index_t device, device_parameter_t parameter, uint8_t metric, time_t timestamp, uint8_t unit, float value)
{
    int i;
    for(i = 0; i < ble_measurements_count; i++) {
        if(ble_measurements[i].device == device && ble_measurements[i].parameter == parameter) {
            ble_measurements[ble_measurements_count].time = timestamp > 1680000000 ? timestamp : 0;
            ble_measurements[ble_measurements_count].value = value;
            return true;
        }
    }
    if(i == ble_measurements_count && ble_measurements_count < BLE_MEASUREMENTS_NUM_MAX &&
      (!devices[device].mask || (devices[device].mask & (1 << parameter)))) {
        ble_measurements[ble_measurements_count].device = device;
        ble_measurements[ble_measurements_count].parameter = parameter;
        ble_measurements[ble_measurements_count].metric = metric;
        ble_measurements[ble_measurements_count].time = timestamp > 1680000000 ? timestamp : 0;
        ble_measurements[ble_measurements_count].unit = unit;
        ble_measurements[ble_measurements_count].value = value;
        ble_measurements_count += 1;
        return true;
    }
    else
        return false;
}

void ble_merge_measurements()
{
    for(int i = 0; i < ble_measurements_count; i++)
        measurements_append_from_device(ble_measurements[i].device, ble_measurements[i].parameter,
            ble_measurements[i].metric, ble_measurements[i].time, ble_measurements[i].unit, ble_measurements[i].value);
}