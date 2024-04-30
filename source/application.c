// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <nvs_flash.h>

#include "application.h"
#include "board.h"
#include "enums.h"
#include "measurements.h"
#include "now.h"
#include "postman.h"
#include "schema.h"
#include "wifi.h"

application_t application;

void application_init()
{
    application.last_measurement_time = 0;
    application.next_measurement_time = 0;

    application.diagnostics = false;
    application.sampling_period = 600;
    application_read_from_nvs();
}

bool application_read_from_nvs()
{
    esp_err_t err;
    nvs_handle_t handle;

    err = nvs_open("application", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        nvs_get_u8(handle, "queue", (uint8_t *) &(application.queue));
        nvs_get_u8(handle, "sleep", (uint8_t *) &(application.sleep));
        nvs_get_u8(handle, "diagnostics", (uint8_t *) &(application.diagnostics));
        nvs_get_u32(handle, "sampling_period", &(application.sampling_period));
        nvs_close(handle);
        ESP_LOGI(__func__, "done");
        return true;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

bool application_write_to_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;

    err = nvs_open("application", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        ok = ok && !nvs_set_u8(handle, "queue", application.queue);
        ok = ok && !nvs_set_u8(handle, "sleep", application.sleep);
        ok = ok && !nvs_set_u8(handle, "diagnostics", application.diagnostics);
        ok = ok && !nvs_set_u32(handle, "sampling_period", application.sampling_period);
        ok = ok && !nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(__func__, "%s", ok ? "done" : "failed");
        return ok;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

static bool write_resource_schema(bp_pack_t *writer)
{
    bool ok = true;
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_put_integer(writer, SCHEMA_MAP);
        ok = ok && bp_create_container(writer, BP_MAP);

            ok = ok && bp_put_string(writer, "id");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_READ_ONLY);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "name");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_READ_ONLY);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "version");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_READ_ONLY);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "free_heap");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_READ_ONLY);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "minimum_free_heap");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_READ_ONLY);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "time");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_READ_ONLY);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "up_time");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_READ_ONLY);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "sampling_period");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_MINIMUM);
                ok = ok && bp_put_integer(writer, 0);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "queue");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "diagnostics");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "sleep");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
            ok = ok && bp_finish_container(writer);

        ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);
    return ok;
}

bool application_schema_handler(char *resource_name, bp_pack_t *writer)
{
    bool ok = true;

    // GET / PUT
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);                                // Path
            ok = ok && bp_put_string(writer, resource_name);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, SCHEMA_GET_RESPONSE | SCHEMA_PUT_REQUEST);    // Methods
        ok = ok && write_resource_schema(writer);                                       // Schema
    ok = ok && bp_finish_container(writer);

    return ok;
}

uint32_t application_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;
    uint32_t response = 0;

    if(method == PM_GET) {
        ok = ok && bp_create_container(writer, BP_MAP);

        ok = ok && bp_put_string(writer, "id");
        ok = ok && bp_put_string(writer, APP_ID);
        ok = ok && bp_put_string(writer, "name");
        ok = ok && bp_put_string(writer, APP_NAME);
        ok = ok && bp_put_string(writer, "version");
        ok = ok && bp_put_integer(writer, APP_VERSION);
        ok = ok && bp_put_string(writer, "free_heap");
        ok = ok && bp_put_integer(writer, esp_get_free_heap_size());
        ok = ok && bp_put_string(writer, "minimum_free_heap");
        ok = ok && bp_put_integer(writer, esp_get_minimum_free_heap_size());
        ok = ok && bp_put_string(writer, "time");
        ok = ok && bp_put_big_integer(writer, NOW);
        ok = ok && bp_put_string(writer, "up_time");
        ok = ok && bp_put_integer(writer, esp_timer_get_time() / 1000000L);

        ok = ok && bp_put_string(writer, "sampling_period");
        ok = ok && bp_put_integer(writer, application.sampling_period);
        ok = ok && bp_put_string(writer, "queue");
        ok = ok && bp_put_boolean(writer, application.queue);
        ok = ok && bp_put_string(writer, "diagnostics");
        ok = ok && bp_put_boolean(writer, application.diagnostics);
        ok = ok && bp_put_string(writer, "sleep");
        ok = ok && bp_put_boolean(writer, application.sleep);

        ok = ok && bp_finish_container(writer);
        response = ok ? PM_205_Content : PM_500_Internal_Server_Error;

    }
    else if(method == PM_PUT) {
        if(!bp_close(reader) || !bp_next(reader) || !bp_is_map(reader) || !bp_open(reader))
            response = PM_400_Bad_Request;
        else {
            while(bp_next(reader)) {
                if(bp_match(reader, "sampling_period")) {
                    application.sampling_period = bp_get_integer(reader);
                    application.next_measurement_time = application.last_measurement_time + application.sampling_period * 1000000L;
                }
                else if(bp_match(reader, "queue"))
                    application.queue = bp_get_boolean(reader);
                else if(bp_match(reader, "diagnostics"))
                    application.diagnostics = bp_get_boolean(reader);
                else if(bp_match(reader, "sleep"))
                    application.sleep = bp_get_boolean(reader);
                else bp_next(reader);
            }
            bp_close(reader);
            
            ok = ok && application_write_to_nvs();
            response = ok ? PM_204_Changed : PM_500_Internal_Server_Error;
        }
    }
    else
        response = PM_405_Method_Not_Allowed;

    return response;
}

void application_measure()
{
    if(application.diagnostics) {
        measurements_append(board.id, RESOURCE_APPLICATION, 0, 0, 0, 0, 0, 0, METRIC_UpTime, NOW, UNIT_s, esp_timer_get_time() / 1000000L);
        measurements_append(board.id, RESOURCE_APPLICATION, 0, 0, 0, 0, 0, 0, METRIC_MinimumFreeHeap, NOW, UNIT_B, esp_get_minimum_free_heap_size());
    }
}

