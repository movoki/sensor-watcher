// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef nodes_h
#define nodes_h

#include "bigpacks.h"

#define NODES_NUM_MAX 			64

typedef uint64_t node_address_t;
typedef int8_t   node_rssi_t;

typedef struct {
	node_address_t address;
	time_t    	   timestamp;
	node_rssi_t    rssi;
	bool      	   persistent;
} node_t;

typedef uint8_t nodes_index_t;
extern node_t nodes[];
extern nodes_index_t nodes_count;

void nodes_init();
bool nodes_read_from_nvs();
bool nodes_write_to_nvs();

int nodes_get(node_t *node);
int nodes_append(node_t *node);
int nodes_get_or_append(node_t *node);
int nodes_update_or_append(node_t *node);

bool nodes_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t nodes_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

#endif