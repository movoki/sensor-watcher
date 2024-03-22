// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef measurements_h
#define measurements_h

#define MEASUREMENTS_NUM_MAX		64
#define MEASUREMENTS_NAME_LENGTH	128

#include <time.h>

#include "devices.h"
#include "bigpostman.h"
#include "pbuf.h"

typedef uint8_t  measurement_unit_t;
typedef uint16_t measurement_metric_t;
typedef uint64_t measurement_node_t;

typedef struct {
	measurement_node_t	  node;
	device_address_t  	  address;
	time_t    			  time;

	float 	  			  value;

	device_part_t     	  part;
	measurement_metric_t  metric;

	resource_t			  resource;
	device_bus_t	  	  bus;
	device_multiplexer_t  multiplexer;
	device_channel_t   	  channel;
	device_parameter_t	  parameter;
	measurement_unit_t    unit;
} measurement_t;

typedef struct {
	uint64_t node;

    uint8_t  resource;
    uint8_t  bus;
    uint8_t  multiplexer;
    uint8_t  channel;

    uint16_t part;
    uint16_t metric;

    uint64_t address;

    uint8_t  parameter;
    uint8_t  unit;
    uint16_t magic;

    uint32_t time;
    float    value;
} __attribute__((packed)) measurement_frame_t;


typedef uint8_t measurements_index_t;
extern bool measurements_full;
extern measurements_index_t measurements_count;
extern measurement_t measurements[];

void measurements_init();
void measurements_measure();
bool measurements_entry_to_senml_row(measurements_index_t index, pbuf_t *buf);
bool measurements_entry_to_bigpostman(measurements_index_t index, char *buffer, size_t *buffer_size, char *id, char *key);
bool measurements_entry_to_template_row(measurements_index_t index, pbuf_t *buf, char *template_row, char *template_name_separator);
void measurements_entry_to_frame(measurements_index_t index, measurement_frame_t *frame);
bool measurements_build_name(pbuf_t *buf, measurements_index_t measurement, char separator);
bool measurements_pack(bp_pack_t *bp);
bool measurements_put_signature(bp_pack_t *bp, char *id, char *key);
bool measurements_to_senml(char *buffer, size_t *buffer_size);
bool measurements_to_bigpacks(char *buffer, size_t *buffer_size, char *id, char *key);
bool measurements_to_bigpostman(char *buffer, size_t *buffer_size, char *id, char *key);
bool measurements_to_template(char *buffer, size_t *buffer_size, char *template_header, char *template_row, char *template_row_separator, char *template_path_separator, char *template_footer);
bool measurements_append(measurement_node_t node,          resource_t resource,   device_bus_t bus,
                         device_multiplexer_t multiplexer, device_channel_t channel,     device_address_t address,
                         device_part_t part,               device_parameter_t parameter, measurement_metric_t metric,
                         time_t time,                      measurement_unit_t unit,      float value);

bool measurements_append_from_device(devices_index_t device, device_parameter_t parameter, measurement_metric_t metric, time_t time, measurement_unit_t unit, float value);
bool measurements_append_from_frame(measurement_frame_t *frame);
uint32_t measurements_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

#endif
