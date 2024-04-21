// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef logs_h
#define logs_h

#include "bigpacks.h"

#define LOG_MAX_ROWS    64
#define LOG_MAX_COLUMNS 90

extern uint32_t log_row_index;
extern uint32_t log_column_index;
extern char log_data[][LOG_MAX_COLUMNS];

void logs_init();
bool logs_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t logs_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

#endif
