// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef board_h
#define board_h

#include "bigpacks.h"

typedef struct {
	uint64_t id;
    uint32_t processor;
	uint32_t model;
	uint32_t log_level;
	uint16_t cpu_frequency;
	uint8_t  antenna;
	bool 	 diagnostics;
} board_t;

extern board_t board;

void board_init();
void board_configure();
void board_stop();
bool board_read_from_nvs();
bool board_write_to_nvs();
bool board_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t board_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

void board_measure();
void board_set_led(uint32_t color);
void board_set_I2C_power(bool state);

#endif