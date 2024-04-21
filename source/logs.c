// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

#include <esp_system.h>
#include <esp_log.h>

#include "bigpostman.h"
#include "logs.h"
#include "schema.h"

uint32_t log_row_index;
uint32_t log_column_index;
char log_data[LOG_MAX_ROWS][LOG_MAX_COLUMNS];

void logs_init()
{
    int logging_vprintf(const char *fmt, va_list list) {
        char buffer[256];
        int length = vsnprintf(buffer, sizeof(buffer), fmt, list);
        for(int i = 0; i < length; i++) {
            if(buffer[i] == '\n') {
                log_row_index = (log_row_index + 1) % LOG_MAX_ROWS;
                log_column_index = 0;
                memset(log_data[log_row_index], 0, LOG_MAX_COLUMNS);
            }
            else if(log_column_index < LOG_MAX_COLUMNS - 1)
                log_data[log_row_index][log_column_index++] = buffer[i];
        }
        return 0;
    }

	log_row_index = 0;
	log_column_index = 0;
    memset(log_data, 0, sizeof(log_data));

    esp_log_set_vprintf(logging_vprintf);
    esp_log_level_set("*", ESP_LOG_INFO);
}


static bool write_resource_schema(bp_pack_t *writer)
{
    bool ok = true;
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_put_integer(writer, SCHEMA_LIST | SCHEMA_MAXIMUM_ELEMENTS);
        ok = ok && bp_create_container(writer, BP_LIST);
            ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
            ok = ok && bp_put_integer(writer, LOG_MAX_COLUMNS);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, LOG_MAX_ROWS);
    ok = ok && bp_finish_container(writer);
    return ok;
}

bool logs_schema_handler(char *resource_name, bp_pack_t *writer)
{
    bool ok = true;

    // GET
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);            // Path
            ok = ok && bp_put_string(writer, resource_name);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, SCHEMA_GET_RESPONSE);     // Methods
        ok = ok && write_resource_schema(writer);                   // Schema
    ok = ok && bp_finish_container(writer);

    return ok;
}


uint32_t logs_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    if(method == PM_GET) {
        bool ok = true;
        ok = ok && bp_create_container(writer, BP_LIST);
        for(int i = 0; i < LOG_MAX_ROWS && ok; i++) {
            if(log_data[(log_row_index + i) % LOG_MAX_ROWS][0]) {
                ok = ok && bp_put_string(writer, log_data[(log_row_index + i) % LOG_MAX_ROWS]);
            }
        }
        ok = ok && bp_finish_container(writer);
        return ok ? PM_205_Content : PM_500_Internal_Server_Error;
    }
    else
        return PM_405_Method_Not_Allowed;
}

