// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_crt_bundle.h>
#include <mqtt_client.h>

#include "postman.h"
#include "enums.h"
#include "application.h"
#include "backends.h"
#include "schema.h"

backend_t backends[BACKENDS_NUM_MAX];
bool backends_started;
uint8_t backends_modified;

void backends_init()
{
    backends_started = false;
    backends_modified = 0;
    memset(backends, 0, sizeof(backends));
    backends_read_from_nvs();
}

bool backends_read_from_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;
    char nvs_key[16];
    size_t length;

    err = nvs_open("backends", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        memset(backends, 0, sizeof(backends));
        for(uint8_t i = 0; i < BACKENDS_NUM_MAX && ok; i++) {
            snprintf(nvs_key, sizeof(nvs_key), "%u_service", i % 255);
            length = BACKEND_SERVICE_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].service, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_auth", i % 255);
            ok = ok && !nvs_get_u8(handle, nvs_key, &(backends[i].auth));

            snprintf(nvs_key, sizeof(nvs_key), "%u_format", i % 255);
            ok = ok && !nvs_get_u8(handle, nvs_key, &(backends[i].format));

            snprintf(nvs_key, sizeof(nvs_key), "%u_uri", i % 255);
            length = BACKEND_URI_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].uri, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_server_cert", i % 255);
            length = BACKEND_PEM_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].server_cert, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_user", i % 255);
            length = BACKEND_PEM_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].user, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_key", i % 255);
            length = BACKEND_PEM_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].key, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_client_id", i % 255);
            length = BACKEND_CLIENT_ID_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].client_id, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_out_topic", i % 255);
            length = BACKEND_TOPIC_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].output_topic, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_in_topic", i % 255);
            length = BACKEND_TOPIC_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].input_topic, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_content", i % 255);
            length = BACKEND_CONTENT_TYPE_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].content_type, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_tmpl_header", i % 255);
            length = BACKEND_TEMPLATE_HEADER_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].template_header, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_tmpl_row", i % 255);
            length = BACKEND_TEMPLATE_ROW_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].template_row, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_tmpl_r_sep", i % 255);
            length = BACKEND_TEMPLATE_SEPARATOR_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].template_row_separator, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_tmpl_n_sep", i % 255);
            length = BACKEND_TEMPLATE_SEPARATOR_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].template_name_separator, &length);

            snprintf(nvs_key, sizeof(nvs_key), "%u_tmpl_footer", i % 255);
            length = BACKEND_TEMPLATE_FOOTER_LENGTH;
            ok = ok && !nvs_get_str(handle, nvs_key, backends[i].template_footer, &length);
        }

        if(!ok)
            memset(backends, 0, sizeof(backends));

        nvs_close(handle);
        ESP_LOGI(__func__, "%s", ok ? "done" : "failed");
        return ok;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

bool backends_write_to_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;
    char nvs_key[16];

    err = nvs_open("backends", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        for(uint8_t i = 0; i < BACKENDS_NUM_MAX && ok; i++) {
            snprintf(nvs_key, sizeof(nvs_key), "%u_service", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].service);
            snprintf(nvs_key, sizeof(nvs_key), "%u_auth", i % 255);
            ok = ok && !nvs_set_u8(handle, nvs_key, backends[i].auth);
            snprintf(nvs_key, sizeof(nvs_key), "%u_format", i % 255);
            ok = ok && !nvs_set_u8(handle, nvs_key, backends[i].format);
            snprintf(nvs_key, sizeof(nvs_key), "%u_uri", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].uri);
            snprintf(nvs_key, sizeof(nvs_key), "%u_server_cert", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].server_cert);
            snprintf(nvs_key, sizeof(nvs_key), "%u_user", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].user);
            snprintf(nvs_key, sizeof(nvs_key), "%u_key", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].key);
            snprintf(nvs_key, sizeof(nvs_key), "%u_client_id", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].client_id);
            snprintf(nvs_key, sizeof(nvs_key), "%u_out_topic", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].output_topic);
            snprintf(nvs_key, sizeof(nvs_key), "%u_in_topic", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].input_topic);
            snprintf(nvs_key, sizeof(nvs_key), "%u_content", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].content_type);
            snprintf(nvs_key, sizeof(nvs_key), "%u_tmpl_header", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].template_header);
            snprintf(nvs_key, sizeof(nvs_key), "%u_tmpl_row", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].template_row);
            snprintf(nvs_key, sizeof(nvs_key), "%u_tmpl_r_sep", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].template_row_separator);
            snprintf(nvs_key, sizeof(nvs_key), "%u_tmpl_n_sep", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].template_name_separator);
            snprintf(nvs_key, sizeof(nvs_key), "%u_tmpl_footer", i % 255);
            ok = ok && !nvs_set_str(handle, nvs_key, backends[i].template_footer);
        }
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
        ok = ok && bp_put_integer(writer, SCHEMA_LIST | SCHEMA_INDEX | SCHEMA_READ_ONLY);
        ok = ok && bp_create_container(writer, BP_LIST);
            ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_IDENTIFIER);
        ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);
    return ok;
}

static bool write_item_schema(bp_pack_t *writer)
{
    bool ok = true;
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_put_integer(writer, SCHEMA_MAP);
        ok = ok && bp_create_container(writer, BP_MAP);

            ok = ok && bp_put_string(writer, "status");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_READ_ONLY | SCHEMA_VALUES);
                ok = ok && bp_create_container(writer, BP_LIST);
                for(int i = 0; i < BACKEND_STATUS_NUM_MAX; i++)
                    ok = ok && bp_put_string(writer, backend_status_labels[i]);
                ok = ok && bp_finish_container(writer);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "error");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_READ_ONLY);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "message");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_READ_ONLY | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_MESSAGE_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "service");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_SERVICE_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "uri");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_URI_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "format");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_VALUES);
                ok = ok && bp_create_container(writer, BP_LIST);
                for(int i = 0; i < BACKEND_FORMAT_NUM_MAX; i++)
                    ok = ok && bp_put_string(writer, backend_format_labels[i]);
                ok = ok && bp_finish_container(writer);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "auth");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_VALUES);
                ok = ok && bp_create_container(writer, BP_LIST);
                for(int i = 0; i < BACKEND_AUTH_NUM_MAX; i++)
                    ok = ok && bp_put_string(writer, backend_auth_labels[i]);
                ok = ok && bp_finish_container(writer);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "user");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_USER_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "key");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_KEY_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "server_cert");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_PEM_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "output_topic");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_TOPIC_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "input_topic");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_TOPIC_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "client_id");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_CLIENT_ID_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "content_type");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_CONTENT_TYPE_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "template_header");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_TEMPLATE_HEADER_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "template_row");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_TEMPLATE_ROW_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "template_row_separator");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_TEMPLATE_SEPARATOR_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "template_name_separator");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_TEMPLATE_SEPARATOR_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "template_footer");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, BACKEND_TEMPLATE_FOOTER_LENGTH);
            ok = ok && bp_finish_container(writer);

        ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);
    return ok;
}

bool backends_schema_handler(char *resource_name, bp_pack_t *writer)
{
    bool ok = true;

    // GET
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);                                // Path
            ok = ok && bp_put_string(writer, resource_name);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, SCHEMA_GET_RESPONSE);                         // Methods
        ok = ok && write_resource_schema(writer);                                       // Schema
    ok = ok && bp_finish_container(writer);

    // GET item / PUT item
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);                                // Path
            ok = ok && bp_put_string(writer, resource_name);
            ok = ok && bp_put_none(writer);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, SCHEMA_GET_RESPONSE | SCHEMA_PUT_REQUEST);    // Methods
        ok = ok && write_item_schema(writer);                                           // Schema
    ok = ok && bp_finish_container(writer);

    return ok;
}

uint32_t backends_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;
    uint32_t index;

    if(method == PM_GET) {
        if(bp_next(reader)) {
            if(!bp_is_integer(reader) || (index = bp_get_integer(reader)) > BACKENDS_NUM_MAX - 1)
                return PM_400_Bad_Request;

            ok = ok && backend_pack(writer, index);
        }
        else {
            ok = ok && bp_create_container(writer, BP_LIST);
            for(index = 0; index != BACKENDS_NUM_MAX && ok; index++)
                ok = ok && bp_put_integer(writer, index);
            ok = ok && bp_finish_container(writer);
        }
        return ok ? PM_205_Content : PM_500_Internal_Server_Error;
    }
    else if(method == PM_PUT) {
        if(!bp_next(reader) || !bp_is_integer(reader) || (index = bp_get_integer(reader)) > BACKENDS_NUM_MAX - 1 ||
           !bp_close(reader) || !bp_next(reader) || !bp_is_map(reader))
            return PM_400_Bad_Request;

        backends_stop();
        memset(&backends[index], 0, sizeof(backend_t));
        if(bp_get_content_length(reader))
            ok = ok && backend_unpack(reader, index);

        if(ok) {
            backends_modified |= 1 << index;   // to force sending to HTTP backends for status update
            backends_start();
            return backends_write_to_nvs() ? PM_204_Changed : PM_500_Internal_Server_Error;
        }
        else {
            backends_read_from_nvs();
            backends_start();
            return PM_400_Bad_Request;
        }
    }
    else
        return PM_405_Method_Not_Allowed;
}

bool backend_pack(bp_pack_t *writer, uint32_t index)
{
    bool ok = true;

    ok = ok && bp_create_container(writer, BP_MAP);
    ok = ok && bp_put_string(writer, "status") && bp_put_string(writer, backend_status_labels[backends[index].status]);
    ok = ok && bp_put_string(writer, "error") && bp_put_integer(writer, backends[index].error);
    ok = ok && bp_put_string(writer, "message") && bp_put_string(writer, backends[index].message);

    ok = ok && bp_put_string(writer, "service") && bp_put_string(writer, backends[index].service);
    ok = ok && bp_put_string(writer, "uri") && bp_put_string(writer, backends[index].uri);
    ok = ok && bp_put_string(writer, "format") && bp_put_string(writer, backend_format_labels[backends[index].format < BACKEND_FORMAT_NUM_MAX ? backends[index].format : 0]);
    ok = ok && bp_put_string(writer, "auth") && bp_put_string(writer, backend_auth_labels[backends[index].auth < BACKEND_AUTH_NUM_MAX ? backends[index].auth : 0]);
    ok = ok && bp_put_string(writer, "user") && bp_put_string(writer, backends[index].user);
    ok = ok && bp_put_string(writer, "key") && bp_put_string(writer, backends[index].key);
    ok = ok && bp_put_string(writer, "server_cert") && bp_put_string(writer, backends[index].server_cert);
    ok = ok && bp_put_string(writer, "output_topic") && bp_put_string(writer, backends[index].output_topic);
    ok = ok && bp_put_string(writer, "input_topic") && bp_put_string(writer, backends[index].input_topic);
    ok = ok && bp_put_string(writer, "client_id") && bp_put_string(writer, backends[index].client_id);
    ok = ok && bp_put_string(writer, "content_type") && bp_put_string(writer, backends[index].content_type);
    ok = ok && bp_put_string(writer, "template_header") && bp_put_string(writer, backends[index].template_header);
    ok = ok && bp_put_string(writer, "template_row") && bp_put_string(writer, backends[index].template_row);
    ok = ok && bp_put_string(writer, "template_row_separator") && bp_put_string(writer, backends[index].template_row_separator);
    ok = ok && bp_put_string(writer, "template_name_separator") && bp_put_string(writer, backends[index].template_name_separator);
    ok = ok && bp_put_string(writer, "template_footer") && bp_put_string(writer, backends[index].template_footer);
    ok = ok && bp_finish_container(writer);

    return ok;
}

bool backend_unpack(bp_pack_t *reader, uint32_t index)
{
    bool ok = true;

    ok = ok && bp_open(reader);
    while(ok && bp_next(reader)) {
        if(bp_match(reader, "auth")) {
            int i;
            for(i = 0; i < BACKEND_AUTH_NUM_MAX; i++)
                if(bp_equals(reader, backend_auth_labels[i]))
                    break;
            if(i < BACKEND_AUTH_NUM_MAX)
                backends[index].auth = i;
            else
                ok = false;
        }
        else if(bp_match(reader, "format")) {
            int i;
            for(i = 0; i < BACKEND_FORMAT_NUM_MAX; i++)
                if(bp_equals(reader, backend_format_labels[i]))
                    break;
            if(i < BACKEND_FORMAT_NUM_MAX)
                backends[index].format = i;
            else
                ok = false;
        }
        else if(bp_match(reader, "service"))
            ok = ok && bp_get_string(reader, backends[index].service, BACKEND_SERVICE_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "uri"))
            ok = ok && bp_get_string(reader, backends[index].uri, BACKEND_URI_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "server_cert"))
            ok = ok && bp_get_string(reader, backends[index].server_cert, BACKEND_PEM_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "user"))
            ok = ok && bp_get_string(reader, backends[index].user, BACKEND_PEM_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "key"))
            ok = ok && bp_get_string(reader, backends[index].key, BACKEND_PEM_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "client_id"))
            ok = ok && bp_get_string(reader, backends[index].client_id, BACKEND_CLIENT_ID_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "output_topic"))
            ok = ok && bp_get_string(reader, backends[index].output_topic, BACKEND_TOPIC_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "input_topic"))
            ok = ok && bp_get_string(reader, backends[index].input_topic, BACKEND_TOPIC_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "content_type"))
            ok = ok && bp_get_string(reader, backends[index].content_type, BACKEND_CONTENT_TYPE_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "template_header"))
            ok = ok && bp_get_string(reader, backends[index].template_header, BACKEND_TEMPLATE_HEADER_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "template_row"))
            ok = ok && bp_get_string(reader, backends[index].template_row, BACKEND_TEMPLATE_ROW_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "template_row_separator"))
            ok = ok && bp_get_string(reader, backends[index].template_row_separator, BACKEND_TEMPLATE_SEPARATOR_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "template_name_separator"))
            ok = ok && bp_get_string(reader, backends[index].template_name_separator, BACKEND_TEMPLATE_SEPARATOR_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else if(bp_match(reader, "template_footer"))
            ok = ok && bp_get_string(reader, backends[index].template_footer, BACKEND_TEMPLATE_FOOTER_LENGTH / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
        else bp_next(reader);
    }
    bp_close(reader);

    return ok;
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    backend_t *backend = handler_args;
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        break;
    case MQTT_EVENT_DISCONNECTED:
        if(backend->status == BACKEND_STATUS_ONLINE) {
            backend->status = BACKEND_STATUS_OFFLINE;
            backend->error = 0;
        }
        break;
    // case MQTT_EVENT_DATA:
    //     printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    //     printf("DATA=%.*s\r\n", event->data_len, event->data);   /// send this to postman
    //     break;
    case MQTT_EVENT_ERROR:
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            if(event->error_handle->esp_tls_last_esp_err) {
                backend->status = BACKEND_STATUS_ERROR;
                backend->error = event->error_handle->esp_tls_last_esp_err;
                backend->message[0] = 0;
            }
            else if(event->error_handle->esp_tls_stack_err) {
                backend->status = BACKEND_STATUS_ERROR;
                backend->error = event->error_handle->esp_tls_stack_err + BACKEND_ERROR_TLS_STACK_BASE;
                backend->message[0] = 0;
            }
            else if(event->error_handle->esp_transport_sock_errno) {
                backend->status = BACKEND_STATUS_ERROR;
                backend->error = event->error_handle->esp_transport_sock_errno + BACKEND_ERROR_TRANSPORT_SOCK_BASE;
                backend->message[0] = 0;
            }
        }
        else if(event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            backend->status = BACKEND_STATUS_ERROR;
            backend->error = event->error_handle->connect_return_code + BACKEND_ERROR_MQTT_RETURN_CODE_BASE;
            backend->message[0] = 0;
        }
        break;
    default:
        break;
    }
}

void backends_start()
{
    if(!backends_started) {
        for(int i = 0; i != BACKENDS_NUM_MAX; i++) {
            if(backends[i].uri[0] == 'm') {
                const esp_mqtt_client_config_t mqtt_cfg = {
                    .broker = {
                        .address.uri = backends[i].uri,
                        .verification = {
                            .certificate = backends[i].server_cert[0] ? backends[i].server_cert : NULL,
                            .crt_bundle_attach = backends[i].server_cert[0] ? NULL : esp_crt_bundle_attach,
                        },
                    },
                    .credentials = {
                        .username = backends[i].auth == BACKEND_AUTH_BASIC ? backends[i].user : NULL,
                        .client_id = backends[i].client_id[0] ? backends[i].client_id : NULL,
                        .authentication = {
                            .password = backends[i].auth == BACKEND_AUTH_BASIC ? backends[i].key : NULL,
                            .certificate = backends[i].auth == BACKEND_AUTH_X509 ? backends[i].user : NULL,
                            .key = backends[i].auth == BACKEND_AUTH_X509 ? backends[i].key : NULL,
                        },
                    },
                };
                if(backends[i].handle) {
                    esp_mqtt_client_destroy(backends[i].handle);
                    backends[i].handle = NULL;
                }
                esp_err_t err = ESP_OK;
                err = err ? err : ((backends[i].handle = esp_mqtt_client_init(&mqtt_cfg)) ? ESP_OK : ESP_ERR_INVALID_ARG); /// this crash the micro if uri contains an *
                err = err ? err : esp_mqtt_client_register_event(backends[i].handle, ESP_EVENT_ANY_ID, mqtt_event_handler, &backends[i]);
                err = err ? err : esp_mqtt_client_start(backends[i].handle);
                backends[i].status = err ? BACKEND_STATUS_ERROR : BACKEND_STATUS_ONLINE;
                backends[i].error = err;
            }
        }
        backends_started = true;
    }
}

void backends_stop()
{
    if(backends_started) {
        for(int i = 0; i != BACKENDS_NUM_MAX; i++)
            if(backends[i].uri[0] == 'm' && backends[i].handle) {
                esp_mqtt_client_destroy(backends[i].handle);
                backends[i].handle = NULL;
                backends[i].status = BACKEND_STATUS_OFFLINE;
                backends[i].error = 0;
            }
        backends_started = false;
    }
}

void backends_clear_status()
{
    for(int i = 0; i != BACKENDS_NUM_MAX; i++) {
        backends[i].status = BACKEND_STATUS_OFFLINE;
        backends[i].error = 0;
        backends[i].message[0] = 0;
    }
}
