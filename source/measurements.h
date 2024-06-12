// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef measurements_h
#define measurements_h

#define MEASUREMENTS_NUM_MAX		64
#define MEASUREMENTS_PATH_LENGTH	128

#include <time.h>

#include "devices.h"
#include "bigpacks.h"
#include "nodes.h"
#include "pbuf.h"

typedef uint64_t measurement_descriptor_t;
typedef uint8_t  measurement_tag_t;
typedef uint16_t measurement_metric_t;
typedef uint8_t  measurement_unit_t;
typedef time_t   measurement_timestamp_t;
typedef float    measurement_value_t;

typedef struct {
	node_address_t	    	node;
	device_address_t  	    address;
	measurement_timestamp_t	timestamp;
	measurement_value_t     value;
	device_part_t     	    part;
	measurement_metric_t    metric;
	resource_t			    resource;
	device_bus_t	  	    bus;
	device_multiplexer_t    multiplexer;
	device_channel_t   	    channel;
	device_parameter_t	    parameter;
	measurement_unit_t      unit;
} measurement_t;

typedef struct {		// for LoRa, 32 bytes
	uint64_t node;
	uint64_t descriptor;		// unit:8 metric:12 parameter:8 part:12 channel:4 multiplexer:3 bus:3 resource:6 tag:8 (MSB -> LSB)
    uint64_t address;
    uint32_t timestamp;
    float    value;
} __attribute__((packed)) measurement_frame_t;

typedef struct {		// for BLE ADV, 24 bytes
	uint64_t descriptor;		// unit:8 metric:12 parameter:8 part:12 channel:4 multiplexer:3 bus:3 resource:6 tag:8 (MSB -> LSB)
    uint64_t address;
    uint32_t timestamp;
    float    value;
} __attribute__((packed)) measurement_adv_t;


typedef uint8_t measurements_index_t;
extern bool measurements_full;
extern measurements_index_t measurements_count;
extern measurement_t measurements[];

void measurements_init();
void measurements_measure();
bool measurements_entry_to_senml_row(measurements_index_t index, pbuf_t *buf);
bool measurements_entry_to_postman(measurements_index_t index, char *buffer, size_t *buffer_size, char *id, char *key);
bool measurements_entry_to_template_row(measurements_index_t index, pbuf_t *buf, char *template_row, char *template_path_separator);
bool measurements_entry_to_frame(measurements_index_t index, measurement_frame_t *frame);
bool measurements_entry_to_adv(measurements_index_t index, measurement_adv_t *adv);
measurement_descriptor_t measurements_build_descriptor(measurement_tag_t tag, resource_t resource, device_bus_t bus,
    										device_multiplexer_t multiplexer, device_channel_t channel,
    										device_part_t part, device_parameter_t parameter,
    										measurement_metric_t metric, measurement_unit_t unit);
bool measurements_build_path(pbuf_t *buf, measurements_index_t measurement, char separator);
bool measurements_pack(bp_pack_t *bp);
bool measurements_put_signature(bp_pack_t *bp, char *id, char *key);
bool measurements_to_senml(char *buffer, size_t *buffer_size);
bool measurements_to_postman(char *buffer, size_t *buffer_size, char *id, char *key);
bool measurements_to_template(char *buffer, size_t *buffer_size, char *template_header, char *template_row, char *template_row_separator, char *template_path_separator, char *template_footer);
bool measurements_append(node_address_t node,           resource_t resource,   device_bus_t bus,
                         device_multiplexer_t multiplexer,  device_channel_t channel,     device_address_t address,
                         device_part_t part,                device_parameter_t parameter, measurement_metric_t metric,
                         measurement_timestamp_t timestamp, measurement_unit_t unit,      float value);
bool measurements_append_from_device(devices_index_t device, device_parameter_t parameter, measurement_metric_t metric,
                                     measurement_timestamp_t timestamp, measurement_unit_t unit, float value);
bool measurements_append_with_descriptor(node_address_t node, measurement_descriptor_t descriptor, device_address_t address,
                                   measurement_timestamp_t timestamp, measurement_value_t value);
bool measurements_append_from_frame(measurement_frame_t *frame);
bool measurements_append_from_adv(node_address_t node, measurement_adv_t *adv);
bool measurements_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t measurements_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

#endif
