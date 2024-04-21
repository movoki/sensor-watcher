// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef onewire_h
#define onewire_h

#define ONEWIRE_BUSES_NUM_MAX 2

#include "onewire_bus.h"

#include "bigpacks.h"
#include "devices.h"
#include "enums.h"

typedef struct {
	uint8_t 	data_pin;
	uint8_t 	power_pin;
	void		*handle;
} onewire_bus_t;

extern onewire_bus_t onewire_buses[];
extern uint8_t onewire_buses_count;

void onewire_init();
bool onewire_read_from_nvs();
bool onewire_write_to_nvs();
esp_err_t onewire_start();
esp_err_t onewire_stop();
esp_err_t onewire_send_command(onewire_bus_handle_t bus, uint64_t address, uint8_t command);
void onewire_set_power(bool state);
void onewire_set_default();
bool onewire_put_schema(bp_pack_t *writer);
bool onewire_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t onewire_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);
void onewire_detect_devices();
bool onewire_measure_device(devices_index_t device);
bool onewire_measure_ds18b20(devices_index_t device);
bool onewire_measure_tmp1826(devices_index_t device);

#endif
