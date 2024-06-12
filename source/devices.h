// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef devices_h
#define devices_h

#include <time.h>
#include <stddef.h>

#include "enums.h"
#include "bigpacks.h"

#define DEVICES_NUM_MAX 			64
#define DEVICES_PARAMETERS_NUM_MAX	9		// For RuuviTags
#define DEVICES_PATH_LENGTH			40
#define DEVICES_MASK_ALL_ENABLED 	0

typedef uint64_t device_address_t;
typedef uint16_t device_part_t;
typedef uint16_t device_mask_t;
typedef uint8_t  device_bus_t;
typedef uint8_t  device_multiplexer_t;
typedef uint8_t  device_channel_t;
typedef uint8_t  device_parameter_t;
typedef uint8_t  device_status_t;
typedef int8_t   device_rssi_t;

typedef struct {
	char 	 			*label;
	device_mask_t 		mask;
	resource_t			resource;
	uint8_t  			id_start;
	uint8_t  			id_span;
	uint8_t  			parameters;
} part_t;

extern const part_t parts[];

typedef struct {
	device_address_t  	  address;
	time_t    	      	  timestamp;
	float     	          offsets[DEVICES_PARAMETERS_NUM_MAX];
	device_mask_t  	      mask;
	device_part_t     	  part;
	resource_t			  resource;
	device_bus_t	  	  bus;
	device_multiplexer_t  multiplexer;
	device_channel_t   	  channel;
	device_rssi_t    	  rssi;
	device_status_t	  	  status;
	bool      	      	  persistent;
} device_t;

typedef uint8_t devices_index_t;
extern device_t devices[];
extern devices_index_t devices_count;

void devices_init();
bool devices_read_from_nvs();
bool devices_write_to_nvs();

void devices_buses_start();
void devices_buses_stop();
bool devices_measure_all();

int devices_get(device_t *device);
int devices_append(device_t *device);
int devices_get_or_append(device_t *device);
int devices_update_or_append(device_t *device);

bool devices_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t devices_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);
void devices_build_path(devices_index_t device, char *path, size_t path_size, char separator);

#endif