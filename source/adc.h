// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef adc_h
#define adc_h

#include "enums.h"
#include "bigpostman.h"

#define ADC_CHANNELS_NUM_MAX 10

typedef struct {
	int32_t channels;
	float multiplier;
	uint8_t power_pin;
} adc_t;

extern adc_t adc;

void adc_init();
bool adc_read_from_nvs();
bool adc_write_to_nvs();
bool adc_measure();
bool adc_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t adc_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

#endif
