// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef application_h
#define application_h

#define APP_ID         "com.movoki.sensor_watcher"
#define APP_NAME       "SensorWatcher"
#define APP_VERSION    0x0008

#include "bigpacks.h"

typedef struct {
	int64_t last_measurement_time;
	int64_t next_measurement_time;
	uint32_t sampling_period;
	bool sleep;
	bool diagnostics;
	bool queue;
} application_t;

extern application_t application;

void application_init();
bool application_read_from_nvs();
bool application_write_to_nvs();
bool application_verify_license();
void application_measure();
bool application_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t application_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

#endif