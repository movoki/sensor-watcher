// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bigpacks.h"
#include "devices.h"

#define BLE_MEASUREMENTS_NUM_MAX        64

typedef struct {
    esp_err_t      error;
    device_rssi_t  minimum_rssi;
    uint8_t        scan_duration;    // seconds
    bool           running;
    bool           enabled;
} ble_t;

typedef struct {
    device_address_t    address;
    time_t              time;
    float               value;
    devices_index_t     device;
    device_part_t       part;
    device_parameter_t  parameter;
    uint8_t             metric;
    uint8_t             unit;
} ble_measurement_t;

extern ble_t ble;
extern ble_measurement_t ble_measurements[];
extern uint32_t ble_measurements_count;

bool ble_init();
bool ble_start();
bool ble_stop();
bool ble_read_from_nvs();
bool ble_write_to_nvs();
bool ble_start_scan();
bool ble_is_scanning();
bool ble_stop_scan();
void ble_host_task(void *param);
void ble_merge_measurements();
bool ble_measurements_update(devices_index_t device, device_parameter_t parameter, uint8_t metric, time_t timestamp, uint8_t unit, float value);
uint32_t ble_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);



