// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <time.h>
#include <nvs_flash.h>
#include <esp_bt.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_gap.h>

#include "ble.h"
#include "board.h"
#include "devices.h"
#include "enums.h"
#include "measurements.h"
#include "now.h"
#include "postman.h"
#include "schema.h"
#include "wifi.h"

#if MYNEWT_VAL(BLE_EXT_ADV)
    #define USE_BLE_EXT_ADV
#endif

#define BLE_ADDR_TYPE_PUBLIC 0x00

ble_t ble;
uint32_t ble_measurements_count = 0;
measurement_frame_t ble_measurements[BLE_MEASUREMENTS_NUM_MAX] = {{0}};


bool ble_init()
{
    ble.running = false;
    ble.error = ESP_OK;

    ble.receive = false;
    ble.send = false;
    ble.persistent_only = false;
    ble.mode = BLE_MODE_LEGACY;
    ble.minimum_rssi = -100;
    ble.scan_duration = 45;


    #ifdef CONFIG_IDF_TARGET_ESP32
        ble.power_level = 5;    // +3 dBm
    #else
        ble.power_level = 9;    // +3 dBm
    #endif

    ble_read_from_nvs();
    return ble.receive || ble.send ? ble_start() : true;
}

bool ble_start()
{
    if(!ble.running) {
        ble.running = (ble.error = nimble_port_init()) == ESP_OK;
        if(ble.running) {
            nimble_port_freertos_init(ble_host_task);
            esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ble.power_level);
        }
        else
            ESP_LOGE(__func__, "Failed to start nimble %d ", ble.error);
    }
    return ble.running;
}

bool ble_stop()
{
    if(ble.running) {
        if((ble.error = nimble_port_stop()) == ESP_OK)
            nimble_port_deinit();
        else
            ESP_LOGE(__func__, "Failed to stop nimble %d ", ble.error);
        ble.running = ble.error != ESP_OK;
    }
    return !ble.running;
}

void ble_host_task(void *param)
{
    nimble_port_run();              // This function will return only when nimble_port_stop() is executed
    nimble_port_freertos_deinit();
}

bool ble_read_from_nvs()
{
    nvs_handle_t handle;

    if(nvs_open("ble", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_get_u8(handle, "receive", (uint8_t *) &ble.receive);
        nvs_get_u8(handle, "send", (uint8_t *) &ble.send);
        nvs_get_u8(handle, "persistent_only", (uint8_t *) &ble.persistent_only);
        nvs_get_u8(handle, "mode", &ble.mode);
        nvs_get_i8(handle, "minimum_rssi", &ble.minimum_rssi);
        nvs_get_u8(handle, "scan_duration", &ble.scan_duration);
        nvs_get_u8(handle, "power_level", &ble.power_level);
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
        ok = ok && !nvs_set_u8(handle, "receive", ble.receive);
        ok = ok && !nvs_set_u8(handle, "send", ble.send);
        ok = ok && !nvs_set_u8(handle, "persistent_only", ble.persistent_only);
        ok = ok && !nvs_set_u8(handle, "mode", ble.mode);
        ok = ok && !nvs_set_i8(handle, "minimum_rssi", ble.minimum_rssi);
        ok = ok && !nvs_set_u8(handle, "scan_duration", ble.scan_duration);
        ok = ok && !nvs_set_u8(handle, "power_level", ble.power_level);
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

static bool write_resource_schema(bp_pack_t *writer)
{
    bool ok = true;
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_put_integer(writer, SCHEMA_MAP);
        ok = ok && bp_create_container(writer, BP_MAP);

            ok = ok && bp_put_string(writer, "receive");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "send");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "persistent_only");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "mode");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_VALUES);
                ok = ok && bp_create_container(writer, BP_LIST);
                for(int i = 0; i < BLE_MODE_NUM_MAX; i++)
                    ok = ok && bp_put_string(writer, ble_mode_labels[i]);
                ok = ok && bp_finish_container(writer);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "minimum_rssi");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_MINIMUM | SCHEMA_MAXIMUM);
                ok = ok && bp_put_integer(writer, -128);
                ok = ok && bp_put_integer(writer, 127);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "scan_duration");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_MINIMUM);
                ok = ok && bp_put_integer(writer, 0);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "power_level");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_MINIMUM | SCHEMA_MAXIMUM);
                #ifdef CONFIG_IDF_TARGET_ESP32
                    ok = ok && bp_put_integer(writer, 0);
                    ok = ok && bp_put_integer(writer,7);
                #elif CONFIG_IDF_TARGET_ESP32C6
                    ok = ok && bp_put_integer(writer, 3);
                    ok = ok && bp_put_integer(writer,15);
                #else
                    ok = ok && bp_put_integer(writer, 0);
                    ok = ok && bp_put_integer(writer,15);
                #endif
            ok = ok && bp_finish_container(writer);

        ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);
    return ok;
}

bool ble_schema_handler(char *resource_name, bp_pack_t *writer)
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

uint32_t ble_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;
    uint32_t response = 0;

    if(method == PM_GET) {
        ok = ok && bp_create_container(writer, BP_MAP);
            ok = ok && bp_put_string(writer, "receive");
            ok = ok && bp_put_boolean(writer, ble.receive);
            ok = ok && bp_put_string(writer, "send");
            ok = ok && bp_put_boolean(writer, ble.send);
            ok = ok && bp_put_string(writer, "persistent_only");
            ok = ok && bp_put_boolean(writer, ble.persistent_only);
            ok = ok && bp_put_string(writer, "mode");
            ok = ok && bp_put_string(writer, ble_mode_labels[ble.mode < BLE_MODE_NUM_MAX ? ble.mode : 0]);
            ok = ok && bp_put_string(writer, "minimum_rssi");
            ok = ok && bp_put_integer(writer, ble.minimum_rssi);
            ok = ok && bp_put_string(writer, "scan_duration");
            ok = ok && bp_put_integer(writer, ble.scan_duration);
            ok = ok && bp_put_string(writer, "power_level");
            ok = ok && bp_put_integer(writer, ble.power_level);
        ok = ok && bp_finish_container(writer);
        response = ok ? PM_205_Content : PM_500_Internal_Server_Error;

    }
    else if(method == PM_PUT) {
        if(!bp_close(reader) || !bp_next(reader) || !bp_is_map(reader) || !bp_open(reader))
            response = PM_400_Bad_Request;
        else {
            while(bp_next(reader)) {
                if(bp_match(reader, "receive"))
                    ble.receive = bp_get_boolean(reader);
                else if(bp_match(reader, "send"))
                    ble.send = bp_get_boolean(reader);
                else if(bp_match(reader, "persistent_only"))
                    ble.persistent_only = bp_get_boolean(reader);
                else if(bp_match(reader, "mode")) {
                    int i;
                    for(i = 0; i < BLE_MODE_NUM_MAX; i++)
                        if(bp_equals(reader, ble_mode_labels[i]))
                            break;
                    if(i < BLE_MODE_NUM_MAX)
                        ble.mode = i;
                    else
                        ok = false;
                }
                else if(bp_match(reader, "minimum_rssi"))
                    ble.minimum_rssi = bp_get_integer(reader);
                else if(bp_match(reader, "scan_duration"))
                    ble.scan_duration = bp_get_integer(reader);
                else if(bp_match(reader, "power_level"))
                    ble.power_level = bp_get_integer(reader);
                else bp_next(reader);
            }
            bp_close(reader);
            #ifdef CONFIG_IDF_TARGET_ESP32
                ok = ok && ble.power_level < 8;
            #elif CONFIG_IDF_TARGET_ESP32C6
                ok = ok && ble.power_level > 2 && ble.power_level  < 16;
            #else
                ok = ok && ble.power_level < 16;
            #endif
            ok = ok && ble_write_to_nvs();
            ok = ok && ((ble.receive || ble.send) && !ble.running ? ble_start() : true);
            ok = ok && (!(ble.receive || ble.send) && ble.running ? ble_stop() : true);
            ok = ok && (ble.running ? esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ble.power_level) == ESP_OK : true);
            response = ok ? PM_204_Changed : PM_500_Internal_Server_Error;
        }
    }
    else
        response = PM_405_Method_Not_Allowed;

    return response;
}


void ble_handle_adv(device_address_t address, device_rssi_t rssi, const uint8_t *data, uint8_t length)
{
    int node_index;
    int device_index;
    device_part_t part = 0;
    const uint8_t minew_s1_prefix[] = { 0x02, 0x01, 0x06, 0x03, 0x03, 0xe1, 0xff, 0x10, 0x16, 0xe1, 0xff, 0xa1, 0x01 };

    if(rssi < ble.minimum_rssi)
        return;

    // SensorWatcher node
    if((length == 28 || length == 36) && data[1] == 0xFF && data[2] == 0x57 && data[3] == 0x53) {
        node_t node = {
            .address = address,
            .timestamp = -1,
        };

        if((node_index = nodes_get(&node)) < 0) {
            if(ble.persistent_only)
                return;
            if((node_index = nodes_append(&node)) < 0) {
                ESP_LOGI(__func__, "Cannot add discovered node %016llX", address);
                return;
            }
        }
        if(ble.persistent_only && !nodes[node_index].persistent)
            return;

        nodes[node_index].rssi = rssi;
        nodes[node_index].timestamp = NOW;

        if(length == 28) {
            measurement_adv_t adv;
            memcpy(&adv, data + 4, sizeof(adv));    // because data may not be aligned to 64-bit
            ble_measurements_update(address, adv.path, adv.address, adv.timestamp, adv.value);
        }
        else if(length == 36) {
            measurement_frame_t frame;
            memcpy(&frame, data + 4, sizeof(frame));    // because data may not be aligned to 64-bit
            ble_measurements_update(frame.node, frame.path, frame.address, frame.timestamp, frame.value);
        }
        return;
    }
    else if(length == 31 && data[5] == 0x99 && data[6] == 0x04 && data[7] == 0x05)  // Ruvitag 5 (Raw V2)
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
        .status = DEVICE_STATUS_WORKING,
        .persistent = false,
        .timestamp = -1,
    };

    if((device_index = devices_get(&device)) < 0) {
        if(ble.persistent_only)
            return;
        if((device_index = devices_append(&device)) < 0) {
            ESP_LOGI(__func__, "Cannot add discovered BLE device %s %016llX", parts[part].label, address);
            return;
        }
    }
    if(ble.persistent_only && !devices[device_index].persistent)
        return;

    time_t timestamp = NOW;
    device_mask_t device_mask = devices[device_index].mask ? devices[device_index].mask : ~0;
    devices[device_index].rssi = rssi;
    devices[device_index].timestamp = timestamp;
    devices[device_index].status = DEVICE_STATUS_WORKING;

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

        if(device_mask & 1 << 0) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 0, METRIC_Temperature,   UNIT_Cel ), address, timestamp, temperature);
        if(device_mask & 1 << 1) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 1, METRIC_Humidity,      UNIT_RH  ), address, timestamp, humidity > 100.0 ? 100.0 : humidity);
        if(device_mask & 1 << 2) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 2, METRIC_Pressure,      UNIT_hPa ), address, timestamp, pressure);
        if(device_mask & 1 << 3) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 3, METRIC_Movements,     UNIT_NONE), address, timestamp, movements);
        if(device_mask & 1 << 4) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 4, METRIC_AccelerationX, UNIT_m_s2), address, timestamp, acceleration_x);
        if(device_mask & 1 << 5) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 5, METRIC_AccelerationY, UNIT_m_s2), address, timestamp, acceleration_y);
        if(device_mask & 1 << 6) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 6, METRIC_AccelerationZ, UNIT_m_s2), address, timestamp, acceleration_z);
        if(device_mask & 1 << 7) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 7, METRIC_BatteryLevel,  UNIT_V   ), address, timestamp, battery);
        if(device_mask & 1 << 8) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 8, METRIC_RSSI,          UNIT_dBm ), address, timestamp, rssi);
        break;
    }
    case PART_XIAOMI_LYWSDCGQ: {
        if(data[18] == 0x0D && data[19] == 0x10 && data[20] == 0x04) {   // 0x100D frame, temperature & humidity
            float temperature = (int16_t)(((uint16_t)data[22] << 8) | data[21]) / 10.0;
            float humidity = (int16_t)(((uint16_t)data[24] << 8) | data[23]) / 10.0;

            if(device_mask & 1 << 0) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 0, METRIC_Temperature, UNIT_Cel), address, timestamp,  temperature);
            if(device_mask & 1 << 1) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 1, METRIC_Humidity,    UNIT_RH ), address, timestamp,  humidity > 100.0 ? 100.0 : humidity);
        }
        else if(data[18] == 0x0A && data[19] == 0x10 && data[20] == 0x01)   // 0x100A frame, battery level
            if(device_mask & 1 << 2) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 2, METRIC_BatteryLevel, UNIT_ratio), address, timestamp, data[21] / 100.0);
        break;
    }
    case PART_MINEW_S1: {
        float temperature = (int16_t)(((uint16_t)data[14] << 8) | data[15]) / 256.0;
        float humidity = (int16_t)(((uint16_t)data[16] << 8) | data[17]) / 256.0;

        if(device_mask & 1 << 0) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 0, METRIC_Temperature,  UNIT_Cel  ),  address, timestamp, temperature);
        if(device_mask & 1 << 1) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 1, METRIC_Humidity,     UNIT_RH   ),  address, timestamp, humidity > 100.0 ? 100.0 : humidity);
        if(device_mask & 1 << 2) ble_measurements_update(board.id, measurements_build_path(0, RESOURCE_BLE, 0, 0, 0, part, 2, METRIC_BatteryLevel, UNIT_ratio),  address, timestamp, data[13] / 100.0);
        break;
    }
    default:
        break;
    }
    return;
}

int ble_gap_event_handler(struct ble_gap_event *event, void *arg)
{
    uint64_t address;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        address = (uint64_t)event->disc.addr.val[0] << 0  | (uint64_t)event->disc.addr.val[1] << 8  |
                  (uint64_t)event->disc.addr.val[2] << 16 | (uint64_t)event->disc.addr.val[3] << 40 |
                  (uint64_t)event->disc.addr.val[4] << 48 | (uint64_t)event->disc.addr.val[5] << 56 | 0x000000FFFF000000;
        ble_handle_adv(address, event->disc.rssi, event->disc.data, event->disc.length_data);
        // esp_log_buffer_hex("BLE ADV:", event->disc.data,  event->disc.length_data);
        break;
    #ifdef USE_BLE_EXT_ADV
    case BLE_GAP_EVENT_EXT_DISC:
        address = (uint64_t)event->ext_disc.addr.val[0] << 0  | (uint64_t)event->ext_disc.addr.val[1] << 8  |
                  (uint64_t)event->ext_disc.addr.val[2] << 16 | (uint64_t)event->ext_disc.addr.val[3] << 40 |
                  (uint64_t)event->ext_disc.addr.val[4] << 48 | (uint64_t)event->ext_disc.addr.val[5] << 56 | 0x000000FFFF000000;
        ble_handle_adv(address, event->ext_disc.rssi, event->ext_disc.data, event->ext_disc.length_data);
        // esp_log_buffer_hex("BLE EXT ADV:", event->ext_disc.data,  event->ext_disc.length_data);
        break;
    #endif
    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(__func__, "discovery complete; reason=%d\n", event->disc_complete.reason);
        break;
    default:
        break;
    }

    return 0;
}

bool ble_start_scan()
{
    ble_measurements_count = 0;

    #ifndef USE_BLE_EXT_ADV
        struct ble_gap_disc_params disc_params = {
            .filter_duplicates = 0,
            .passive = 1,
            .itvl = BLE_GAP_SCAN_FAST_PERIOD,
            .window = BLE_GAP_SCAN_FAST_WINDOW,
            .filter_policy = 0,
            .limited = 0,
        };
        if((ble.error = ble_gap_disc(BLE_ADDR_TYPE_PUBLIC, BLE_HS_FOREVER, &disc_params, ble_gap_event_handler, NULL)) != 0) {
            ESP_LOGE(__func__, "Error initiating GAP discovery procedure; err=%d\n", ble.error);
            return false;
        }
    #else
        struct ble_gap_ext_disc_params ext_disc_params = {
            .passive = 1,
            .itvl = BLE_GAP_SCAN_FAST_PERIOD,
            .window = BLE_GAP_SCAN_FAST_WINDOW,
        };
        if((ble.error = ble_gap_ext_disc(BLE_ADDR_TYPE_PUBLIC, 0, 0, 0, 0, 0,
                                         ble.mode == BLE_MODE_LONG_RANGE ? NULL : &ext_disc_params,
                                         ble.mode == BLE_MODE_LONG_RANGE ? &ext_disc_params : NULL,
                                         ble_gap_event_handler, NULL)) != 0) {
            ESP_LOGE(__func__, "Error initiating extended GAP discovery procedure; err=%d\n", ble.error);
            return false;
        }
    #endif
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

bool ble_measurements_update(node_address_t node, measurement_path_t path, device_address_t address, measurement_timestamp_t timestamp, measurement_value_t value)
{
    int i;
    for(i = 0; i < ble_measurements_count; i++) {
        if(ble_measurements[i].path == path && ble_measurements[i].address == address && ble_measurements[i].node == node) {
            ble_measurements[ble_measurements_count].timestamp = timestamp > 1680000000 ? timestamp : 0;
            ble_measurements[ble_measurements_count].value = value;
            return true;
        }
    }
    if(i == ble_measurements_count && ble_measurements_count < BLE_MEASUREMENTS_NUM_MAX) {
        ble_measurements[ble_measurements_count].node = node;
        ble_measurements[ble_measurements_count].path = path;
        ble_measurements[ble_measurements_count].address = address;
        ble_measurements[ble_measurements_count].timestamp = timestamp > 1680000000 ? timestamp : 0;
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
        measurements_append_from_frame(&ble_measurements[i]);
}

bool ble_send_measurements()
{
    int err = 0;
    bool ok = true;
    measurements_index_t index = 0;
    measurements_index_t count = measurements_full ? MEASUREMENTS_NUM_MAX : measurements_count;

    #ifndef USE_BLE_EXT_ADV
        struct ble_gap_adv_params adv_params = {
            .conn_mode = BLE_GAP_CONN_MODE_NON,
            .disc_mode = BLE_GAP_DISC_MODE_GEN,
            .itvl_min = BLE_GAP_ADV_ITVL_MS(20),    // milliseconds
            .itvl_max = BLE_GAP_ADV_ITVL_MS(30),
        };
    #else
        struct ble_gap_ext_adv_params adv_ext_params_legacy = {
            .legacy_pdu = 1,
            .own_addr_type = BLE_OWN_ADDR_PUBLIC,
            .sid = 0,
            .primary_phy = BLE_HCI_LE_PHY_1M,
            .secondary_phy = BLE_HCI_LE_PHY_1M,
            .tx_power = 127,
            .itvl_min = BLE_GAP_ADV_ITVL_MS(20),    // milliseconds
            .itvl_max = BLE_GAP_ADV_ITVL_MS(30),
        };
        struct ble_gap_ext_adv_params adv_ext_params_extended= {
            .own_addr_type = BLE_OWN_ADDR_PUBLIC,
            .sid = 0,
            .primary_phy = BLE_HCI_LE_PHY_1M,
            .secondary_phy = BLE_HCI_LE_PHY_1M,
            .tx_power = 127,
            .itvl_min = BLE_GAP_ADV_ITVL_MS(20),    // milliseconds
            .itvl_max = BLE_GAP_ADV_ITVL_MS(30),
        };
        struct ble_gap_ext_adv_params adv_ext_params_long_range= {
            .own_addr_type = BLE_OWN_ADDR_PUBLIC,
            .sid = 0,
            .primary_phy = BLE_HCI_LE_PHY_CODED,
            .secondary_phy = BLE_HCI_LE_PHY_CODED,
            .tx_power = 127,
            .itvl_min = BLE_GAP_ADV_ITVL_MS(20),    // milliseconds
            .itvl_max = BLE_GAP_ADV_ITVL_MS(30),
        };
    #endif

    for(int n = 0; n != count && ok; n++) {
        index = measurements_full ? (measurements_count + n) % MEASUREMENTS_NUM_MAX : n;
        #ifndef USE_BLE_EXT_ADV
            uint64_t raw_adv[4] = { 0x5357FF1B00000000 };

            if(measurements_entry_to_adv(index, (measurement_adv_t *) (raw_adv + 1))) {
                ok = ok && (err = ble_gap_adv_set_data((uint8_t *) raw_adv + 4, sizeof(raw_adv) - 4)) == ESP_OK;
                ok = ok && (err = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL)) == ESP_OK;
                vTaskDelay (120 / portTICK_PERIOD_MS);
                ok = ok && (err = ble_gap_adv_stop()) == ESP_OK;
                vTaskDelay (20 / portTICK_PERIOD_MS);
            }
        #else
            switch(ble.mode) {
                case BLE_MODE_LEGACY: {
                    struct os_mbuf *mbuf;
                    uint8_t instance = 0;
                    uint64_t adv[4] = { 0x5357FF1B00000000 };

                    if(measurements_entry_to_adv(index, (measurement_adv_t *) (adv + 1))) {
                        mbuf = os_msys_get_pkthdr(sizeof(adv) - 4, 0);
                        ok = ok && (err = os_mbuf_append(mbuf, (uint8_t *) adv + 4, sizeof(adv) - 4)) == ESP_OK;
                        ok = ok && (err = ble_gap_ext_adv_configure(instance, &adv_ext_params_legacy, NULL, NULL, NULL)) == ESP_OK;
                        ok = ok && (err = ble_gap_ext_adv_set_data(instance, mbuf)) == ESP_OK;
                        ok = ok && (err = ble_gap_ext_adv_start(instance, 0, 0)) == ESP_OK;
                        vTaskDelay (120 / portTICK_PERIOD_MS);
                        ok = ok && (err = ble_gap_ext_adv_stop(instance)) == ESP_OK;
                        vTaskDelay (20 / portTICK_PERIOD_MS);
                        if(!ok)
                            os_mbuf_free_chain(mbuf);
                    }
                    break;
                }
                case BLE_MODE_EXTENDED: {
                    struct os_mbuf *mbuf;
                    uint8_t instance = 0;
                    uint64_t adv[5] = { 0x5357FF1B00000000 };

                    if(measurements_entry_to_frame(index, (measurement_frame_t *) (adv + 1))) {
                        mbuf = os_msys_get_pkthdr(sizeof(adv) - 4, 0);
                        ok = ok && (err = os_mbuf_append(mbuf, (uint8_t *) adv + 4, sizeof(adv) - 4)) == ESP_OK;
                        ok = ok && (err = ble_gap_ext_adv_configure(instance, &adv_ext_params_extended, NULL, NULL, NULL)) == ESP_OK;
                        ok = ok && (err = ble_gap_ext_adv_set_data(instance, mbuf)) == ESP_OK;
                        ok = ok && (err = ble_gap_ext_adv_start(instance, 0, 0)) == ESP_OK;
                        vTaskDelay (120 / portTICK_PERIOD_MS);
                        ok = ok && (err = ble_gap_ext_adv_stop(instance)) == ESP_OK;
                        vTaskDelay (20 / portTICK_PERIOD_MS);
                        if(!ok)
                            os_mbuf_free_chain(mbuf);
                    }
                    break;
                }
                case BLE_MODE_LONG_RANGE: {
                    struct os_mbuf *mbuf;
                    uint8_t instance = 0;
                    uint64_t adv[5] = { 0x5357FF1B00000000 };

                    if(measurements_entry_to_frame(index, (measurement_frame_t *) (adv + 1))) {
                        mbuf = os_msys_get_pkthdr(sizeof(adv) - 4, 0);
                        ok = ok && (err = os_mbuf_append(mbuf, (uint8_t *) adv + 4, sizeof(adv) - 4)) == ESP_OK;
                        ok = ok && (err = ble_gap_ext_adv_configure(instance, &adv_ext_params_long_range, NULL, NULL, NULL)) == ESP_OK;
                        ok = ok && (err = ble_gap_ext_adv_set_data(instance, mbuf)) == ESP_OK;
                        ok = ok && (err = ble_gap_ext_adv_start(instance, 0, 0)) == ESP_OK;
                        vTaskDelay (120 / portTICK_PERIOD_MS);
                        ok = ok && (err = ble_gap_ext_adv_stop(instance)) == ESP_OK;
                        vTaskDelay (20 / portTICK_PERIOD_MS);
                        if(!ok)
                            os_mbuf_free_chain(mbuf);
                    }
                    break;
                }
            }
        #endif
        if(!ok)
            ESP_LOGI(__func__, "sending measurement %i failed with error %i", n, err);
    }
    return ok;
}
