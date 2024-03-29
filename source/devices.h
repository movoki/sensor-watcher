// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef devices_h
#define devices_h

#include "enums.h"
#include "bigpostman.h"

#define DEVICES_NUM_MAX 			64
#define DEVICES_PARAMETERS_NUM_MAX	9		// For RuuviTags
#define DEVICES_NAME_LENGTH			40
#define DEVICES_MASK_ALL_ENABLED 	0

typedef uint64_t device_address_t;
typedef uint16_t device_part_t;
typedef uint16_t device_mask_t;
typedef uint8_t  device_bus_t;
typedef uint8_t  device_multiplexer_t;
typedef uint8_t  device_channel_t;
typedef uint8_t  device_parameter_t;
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
	time_t    	      	  updated;
	float     	          offsets[DEVICES_PARAMETERS_NUM_MAX];  /// reduce to 1/2 using 8.8 fixed point?
	device_mask_t  	      mask;
	device_part_t     	  part;
	resource_t			  resource;
	device_bus_t	  	  bus;
	device_multiplexer_t  multiplexer;
	device_channel_t   	  channel;
	device_rssi_t    	  rssi;
	bool	  	      	  status;
	bool      	      	  fixed;
} device_t;

typedef uint8_t devices_index_t;
extern device_t devices[];
extern devices_index_t devices_count;
extern int64_t devices_first_measurement_time;

void devices_init();
bool devices_read_from_nvs();
bool devices_write_to_nvs();

void devices_buses_init();
void devices_buses_start();
void devices_buses_stop();
void devices_detect_all();
bool devices_measure_all();

int devices_get(device_t *device);
int devices_append(device_t *device);
int devices_get_or_append(device_t *device);

uint32_t devices_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);
void devices_build_name(devices_index_t device, char *name, size_t name_size, char separator);

#endif