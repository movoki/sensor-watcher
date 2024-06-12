// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <esp_system.h>

#include "bigpacks.h"
#include "devices.h"
#include "measurements.h"

#define BLE_MEASUREMENTS_NUM_MAX        64

typedef struct {
    bool           receive;
    bool           send;
    bool           persistent_only;
    uint8_t        mode;
    uint8_t        power_level;
    uint8_t        scan_duration;    // seconds
    device_rssi_t  minimum_rssi;

    esp_err_t      error;
    bool           running;
} ble_t;

extern ble_t ble;
extern measurement_frame_t ble_measurements[];
extern uint32_t ble_measurements_count;

bool ble_init();
bool ble_start();
bool ble_stop();
bool ble_read_from_nvs();
bool ble_write_to_nvs();
bool ble_start_scan();
bool ble_is_scanning();
bool ble_stop_scan();
bool ble_send_measurements();
void ble_host_task(void *param);
void ble_merge_measurements();
bool ble_schema_handler(char *resource_name, bp_pack_t *writer);
bool ble_measurements_update(node_address_t node, measurement_descriptor_t descriptor, device_address_t address, measurement_timestamp_t timestamp, measurement_value_t value);
uint32_t ble_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);



