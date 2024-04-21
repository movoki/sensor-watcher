// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include "bigpostman.h"
#include "nodes.h"
#include "now.h"
#include "schema.h"

RTC_DATA_ATTR node_t nodes[NODES_NUM_MAX] = {{0}};
RTC_DATA_ATTR nodes_index_t nodes_count = 0;


void nodes_init()
{
    nodes_count = 0;
    memset(nodes, 0, sizeof(nodes));
    nodes_read_from_nvs();
}

bool nodes_read_from_nvs()
{
    esp_err_t err;
    bool ok = true;
    char nvs_key[16];
    nvs_handle_t handle;
    nodes_index_t nodes_persistent_count = 0;

    err = nvs_open("nodes", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        ok = ok && !nvs_get_u8(handle, "count", &nodes_persistent_count);
        ESP_LOGI(__func__, "Fixed nodes found in NVS: %i", nodes_persistent_count);
        for(uint8_t i = 0; i < nodes_persistent_count && ok; i++) {
            node_t node = {
                .timestamp = -1,
                .persistent = true,
            };
            snprintf(nvs_key, sizeof(nvs_key), "%u_address", i % 255);
            ok = ok && !nvs_get_u64(handle, nvs_key, &(node.address));
            ok = ok && nodes_append(&node) >= 0;
        }
        if(!ok) {
            memset(nodes, 0, sizeof(nodes));
            nodes_count = 0;
        }
        nvs_close(handle);
        ESP_LOGI(__func__, "%s", ok ? "done" : "failed");
        return ok;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

static node_address_t hex_to_address(const char* hex)
{
    uint8_t i, j;
    uint8_t bytes[sizeof(node_address_t)];
    size_t  hex_length = strlen(hex);

    for(i = 0, j = 0; i < sizeof(bytes) && j + 1 < hex_length; ++i, j+=2)
        bytes[i] = (((hex[j+0] & 0xf) + (hex[j+0] >> 6) * 9) << 4) | ((hex[j+1] & 0xf) + (hex[j+1] >> 6) * 9);
    return (uint64_t) bytes[0] << 56 | (uint64_t) bytes[1] << 48 | (uint64_t) bytes[2] << 40 | (uint64_t) bytes[3] << 32 |
           (uint64_t) bytes[4] << 24 | (uint64_t) bytes[5] << 16 | (uint64_t) bytes[6] << 8  | (uint64_t) bytes[7];
}

bool nodes_write_to_nvs()
{
    esp_err_t err;
    bool ok = true;
    char nvs_key[16];
    nvs_handle_t handle;
    nodes_index_t nodes_persistent_count = 0;

    err = nvs_open("nodes", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        for(uint8_t i = 0; i < nodes_count && ok; i++) {
            if(nodes[i].persistent) {
                snprintf(nvs_key, sizeof(nvs_key), "%u_address", i % 255);
                ok = ok && !nvs_set_u64(handle, nvs_key, nodes[i].address);
                nodes_persistent_count += 1;
            }
        }
        ok = ok && !nvs_set_u8(handle, "count", nodes_persistent_count);
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


int nodes_get(node_t *node)
{
    for(int i = 0; i < nodes_count; i++) {
        if(nodes[i].address == node->address)
            return i;
    }
    return -1;
}

int nodes_append(node_t *node)
{
    if(nodes_count < NODES_NUM_MAX) {
        memcpy(&nodes[nodes_count], node, sizeof(node_t));
        nodes_count += 1;
        return nodes_count - 1;
    }
    else
        return -1;
}

int nodes_get_or_append(node_t *node)
{
    int node_index = nodes_get(node);
    return node_index >= 0 ? node_index : nodes_append(node);
}

static bool write_get_response_schema(bp_pack_t *writer)
{
    bool ok = true;
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_put_integer(writer, SCHEMA_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);
            ok = ok && bp_put_integer(writer, SCHEMA_MAP);
            ok = ok && bp_create_container(writer, BP_MAP);

                ok = ok && bp_put_string(writer, "id");
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_IDENTIFIER | SCHEMA_READ_ONLY);
                ok = ok && bp_finish_container(writer);

                ok = ok && bp_put_string(writer, "address");
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                    ok = ok && bp_put_integer(writer, sizeof(node_address_t) * 2 + 1);
                ok = ok && bp_finish_container(writer);

                ok = ok && bp_put_string(writer, "persistent");
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
                ok = ok && bp_finish_container(writer);

                ok = ok && bp_put_string(writer, "timestamp");
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_READ_ONLY);
                ok = ok && bp_finish_container(writer);

                ok = ok && bp_put_string(writer, "rssi");
                ok = ok && bp_create_container(writer, BP_LIST);
                    ok = ok && bp_put_integer(writer, SCHEMA_INTEGER | SCHEMA_READ_ONLY);
                ok = ok && bp_finish_container(writer);

            ok = ok && bp_finish_container(writer);
        ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);
    return ok;
}

static bool write_post_item_request_schema(bp_pack_t *writer)
{
    bool ok = true;
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_put_integer(writer, SCHEMA_MAP);
        ok = ok && bp_create_container(writer, BP_MAP);

            ok = ok && bp_put_string(writer, "address");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, sizeof(node_address_t) * 2 + 1);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "persistent");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
            ok = ok && bp_finish_container(writer);

        ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);
    return ok;
}

static bool write_put_item_request_schema(bp_pack_t *writer)
{
    bool ok = true;
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_put_integer(writer, SCHEMA_MAP);
        ok = ok && bp_create_container(writer, BP_MAP);

            ok = ok && bp_put_string(writer, "persistent");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
            ok = ok && bp_finish_container(writer);

        ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);
    return ok;
}

bool nodes_schema_handler(char *resource_name, bp_pack_t *writer)
{
    bool ok = true;

    // GET
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);                            // Path
            ok = ok && bp_put_string(writer, resource_name);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, SCHEMA_GET_RESPONSE);                     // Methods
        ok = ok && write_get_response_schema(writer);                               // Schema
    ok = ok && bp_finish_container(writer);

    // POST
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);                            // Path
            ok = ok && bp_put_string(writer, resource_name);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, SCHEMA_POST_REQUEST);                     // Methods
        ok = ok && write_post_item_request_schema(writer);                          // Schema
    ok = ok && bp_finish_container(writer);

    // PUT
    ok = ok && bp_create_container(writer, BP_LIST);
        ok = ok && bp_create_container(writer, BP_LIST);                            // Path
            ok = ok && bp_put_string(writer, resource_name);
            ok = ok && bp_put_none(writer);
        ok = ok && bp_finish_container(writer);
        ok = ok && bp_put_integer(writer, SCHEMA_PUT_REQUEST);                      // Methods
        ok = ok && write_put_item_request_schema(writer);                           // Schema
    ok = ok && bp_finish_container(writer);

    return ok;
}

uint32_t nodes_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;
    char address[sizeof(node_address_t) * 2 + 4];

    if(method == PM_GET) {
        bp_create_container(writer, BP_LIST);
        for(nodes_index_t i = 0; i < nodes_count && ok; i++) {
            snprintf(address, sizeof(address), "%016llX", nodes[i].address);
            ok = ok && bp_create_container(writer, BP_MAP);
                ok = ok && bp_put_string(writer, "id");
                ok = ok && bp_put_integer(writer, i);
                ok = ok && bp_put_string(writer, "address");
                ok = ok && bp_put_string(writer, address);
                ok = ok && bp_put_string(writer, "persistent");
                ok = ok && bp_put_boolean(writer, nodes[i].persistent);
                ok = ok && bp_put_string(writer, "timestamp");
                ok = ok && bp_put_big_integer(writer, nodes[i].timestamp);
                ok = ok && bp_put_string(writer, "rssi");
                ok = ok && bp_put_integer(writer, nodes[i].rssi);
            ok = ok && bp_finish_container(writer);
        }
        ok = ok && bp_finish_container(writer);
        return ok ? PM_205_Content : PM_500_Internal_Server_Error;
    }
    else if(method == PM_POST) {
        if(!bp_close(reader) || !bp_next(reader) || !bp_is_map(reader) || !bp_open(reader))
            return PM_400_Bad_Request;

        node_t node = {
            .timestamp = -1,
        };

        while(ok && bp_next(reader)) {
            if(bp_match(reader, "address")) {
                bp_get_string(reader, address, sizeof(address) / sizeof(bp_type_t));
                node.address = hex_to_address(address);
            }
            else if(bp_match(reader, "persistent"))
                node.persistent = bp_get_boolean(reader);
            else bp_next(reader);
        }
        bp_close(reader);

        if(!node.address)
            return PM_400_Bad_Request;

        return nodes_append(&node) >= 0 && nodes_write_to_nvs() ? PM_201_Created : PM_500_Internal_Server_Error;
    }
    else if(method == PM_PUT) {
        int index;
        if(!bp_next(reader) || !bp_is_integer(reader) || (index = bp_get_integer(reader)) > nodes_count - 1 || index < 0 ||
           !bp_close(reader) || !bp_next(reader) || !bp_is_map(reader) || !bp_open(reader))
                return PM_400_Bad_Request;

        while(bp_next(reader)) {
            if(bp_match(reader, "persistent"))
                nodes[index].persistent = bp_get_boolean(reader);
            else bp_next(reader);
        }
        bp_close(reader);
        return nodes_write_to_nvs() ? PM_204_Changed : PM_500_Internal_Server_Error;
    }
    else
        return PM_405_Method_Not_Allowed;
}

