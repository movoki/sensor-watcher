// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <nvs_flash.h>

#include "application.h"
#include "bigpostman.h"
#include "enums.h"
#include "measurements.h"
#include "now.h"
#include "schema.h"
#include "wifi.h"

wifi_t wifi;

void wifi_init()
{
    esp_err_t err = ESP_OK;
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();

    wifi.status = WIFI_STATUS_DISCONNECTED;
    wifi.reconnected = false;
    wifi.disconnected = false;
	wifi.ssid[0] = 0;
	wifi.password[0] = 0;
    wifi.diagnostics = false;
    wifi.mac = 0;

    err = err ? err : (wifi_read_from_nvs() ? ESP_OK : ESP_FAIL);

    err = err ? err : esp_netif_init();
    err = err ? err : ((wifi.netif = esp_netif_create_default_wifi_sta()) ? ESP_OK : ESP_FAIL);
    err = err ? err : esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, wifi.netif, NULL);
    err = err ? err : esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, wifi.netif, NULL);
    err = err ? err : esp_event_handler_instance_register(IP_EVENT, IP_EVENT_GOT_IP6, &wifi_event_handler, wifi.netif, NULL);
    err = err ? err : esp_wifi_init(&config);
    err = err ? err : esp_wifi_set_storage(WIFI_STORAGE_FLASH); /// make an option to configure this
    err = err ? err : esp_wifi_set_mode(WIFI_MODE_STA);
    err = err ? err : esp_wifi_start();
    err = err ? err : esp_read_mac((uint8_t *) &wifi.mac, ESP_MAC_WIFI_STA);
    // err = err ? err : esp_base_mac_addr_get((uint8_t *) &wifi.mac);
    err = err ? err : (wifi_connect() ? ESP_OK : ESP_FAIL);

    wifi.mac = __builtin_bswap64(wifi.mac);
    if((wifi.mac & 0xFFFF) == 0)
        wifi.mac = (wifi.mac & 0xFFFFFF0000000000) | 0x000000FFFF000000 | ((wifi.mac & 0x000000FFFFFF0000) >> 16);

    ESP_LOGI(__func__, "%s", err ? "failed" : "done");
}

void wifi_start()
{
    wifi.status = WIFI_STATUS_DISCONNECTED;
    wifi.reconnected = false;
    wifi.disconnected = false;
    esp_wifi_start();
    wifi_connect();
}

void wifi_stop()
{
    wifi.status = WIFI_STATUS_DISCONNECTED;
    wifi.reconnected = false;
    wifi.disconnected = false;
    esp_wifi_stop();
}

bool wifi_connect()
{
    esp_err_t err = ESP_OK;

	if(wifi.ssid[0]) {
	    wifi_config_t wifi_config = {};
	    wifi_config.sta.threshold.authmode = WIFI_AUTH_WEP;
	    strncpy((char *) wifi_config.sta.ssid, wifi.ssid, sizeof(wifi_config.sta.ssid));
	    strncpy((char *) wifi_config.sta.password, wifi.password, sizeof(wifi_config.sta.password));

	    err = err ? err : esp_wifi_disconnect();
	    err = err ? err : esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
	    err = err ? err : esp_wifi_connect();

        if(err != ESP_OK) {
            wifi.status = WIFI_STATUS_ERROR;
        }

	    ESP_LOGI(__func__, "%s", err ? "failed" : "done");
   	}
   	else {
        esp_wifi_disconnect();
        wifi.status = WIFI_STATUS_DISCONNECTED;
   		err = ESP_FAIL;
	    ESP_LOGI(__func__, "ssid is blank");
   	}
   	return err == ESP_OK;
}

void wifi_event_handler(void* args, esp_event_base_t base, int32_t id, void* event_data)
{
    if(base == WIFI_EVENT) {
        switch(id) {
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(__func__, "wifi connected @ %lli", esp_timer_get_time());
            // esp_netif_create_ip6_linklocal((esp_netif_t *)args);
            wifi.status = WIFI_STATUS_CONNECTED;
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(__func__, "wifi disconnected");
            wifi.status = WIFI_STATUS_DISCONNECTED;
            wifi.disconnected = true;
            // This is a workaround as ESP32 WiFi libs don't currently auto-reassociate.
            if(wifi.ssid[0] && wifi.password[0]) {
                vTaskDelay (2000 / portTICK_PERIOD_MS);
                esp_wifi_connect();
            }
            break;
        default:
            break;
        }
    }
    else if(base == IP_EVENT) {
        switch(id) {
        case IP_EVENT_STA_GOT_IP:
            ;ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(__func__, "got ip " IPSTR " @ %lli", IP2STR(&event->ip_info.ip), esp_timer_get_time());
            wifi.status = WIFI_STATUS_ONLINE;
            wifi.reconnected = true;
            break;
        case IP_EVENT_GOT_IP6:
            ip_event_got_ip6_t *evt = (ip_event_got_ip6_t *)event_data;
            ESP_LOGI(__func__, "Got IPv6 address " IPV6STR " @ %lli", IPV62STR(evt->ip6_info.ip), esp_timer_get_time());
            // wifi.status = WIFI_STATUS_ONLINE;
            // wifi.reconnected = true;
            break;
        }
    }
}


bool wifi_read_from_nvs()
{
    size_t size;
    esp_err_t err;
    nvs_handle_t handle;

    err = nvs_open("wifi", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
        size = sizeof(wifi.ssid);
        if(nvs_get_str(handle, "ssid", wifi.ssid, &size) != ESP_OK)
            wifi.ssid[0] = 0;
        size = sizeof(wifi.password);
        if(nvs_get_str(handle, "password", wifi.password, &size) != ESP_OK)
            wifi.password[0] = 0;
        nvs_get_u8(handle, "diagnostics", (uint8_t *) &(wifi.diagnostics));
        nvs_close(handle);
        ESP_LOGI(__func__, "done");
        return true;
    }
    else {
        ESP_LOGI(__func__, "nvs_open failed");
        return false;
    }
}

bool wifi_write_to_nvs()
{
    esp_err_t err;
    bool ok = true;
    nvs_handle_t handle;

    err = nvs_open("wifi", NVS_READWRITE, &handle);
    if(err == ESP_OK) {
		ok = ok && !nvs_set_str(handle, "ssid", wifi.ssid);
		ok = ok && !nvs_set_str(handle, "password", wifi.password);
        ok = ok && !nvs_set_u8(handle, "diagnostics", wifi.diagnostics);
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

            ok = ok && bp_put_string(writer, "status");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_VALUES);
                ok = ok && bp_create_container(writer, BP_LIST);
                for(int i = 0; i < WIFI_STATUS_NUM_MAX; i++)
                    ok = ok && bp_put_string(writer, wifi_status_labels[i]);
                ok = ok && bp_finish_container(writer);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "mac_address");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, sizeof(wifi.mac) * 2 + 1);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "ssid");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, WIFI_SSID_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "password");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_STRING | SCHEMA_MAXIMUM_BYTES);
                ok = ok && bp_put_integer(writer, WIFI_PASSWORD_LENGTH);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "diagnostics");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_BOOLEAN);
            ok = ok && bp_finish_container(writer);

            ok = ok && bp_put_string(writer, "rssi");
            ok = ok && bp_create_container(writer, BP_LIST);
                ok = ok && bp_put_integer(writer, SCHEMA_INTEGER);
            ok = ok && bp_finish_container(writer);

        ok = ok && bp_finish_container(writer);
    ok = ok && bp_finish_container(writer);
    return ok;
}

bool wifi_schema_handler(char *resource_name, bp_pack_t *writer)
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

uint32_t wifi_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer)
{
    bool ok = true;
    uint32_t response = 0;

    if(method == PM_GET) {
        int rssi = 0;
        char mac_str[17];

        snprintf(mac_str, sizeof(mac_str), "%016llX", wifi.mac);
        if(wifi.status >= WIFI_STATUS_CONNECTED)
            esp_wifi_sta_get_rssi(&rssi);

        ok = ok && bp_create_container(writer, BP_MAP);
        ok = ok && bp_put_string(writer, "status");
        ok = ok && bp_put_string(writer, wifi_status_labels[wifi.status]);
        ok = ok && bp_put_string(writer, "mac_address");
        ok = ok && bp_put_string(writer, mac_str);
        ok = ok && bp_put_string(writer, "ssid");
        ok = ok && bp_put_string(writer, wifi.ssid);
        ok = ok && bp_put_string(writer, "password");
        ok = ok && bp_put_string(writer, wifi.password);
        ok = ok && bp_put_string(writer, "diagnostics");
        ok = ok && bp_put_boolean(writer, wifi.diagnostics);
        ok = ok && bp_put_string(writer, "rssi");
        ok = ok && bp_put_integer(writer, rssi);
        ok = ok && bp_finish_container(writer);
        response = ok ? PM_205_Content : PM_500_Internal_Server_Error;
    }
    else if(method == PM_PUT) {
        if(!bp_close(reader) || !bp_next(reader) || !bp_is_map(reader) || !bp_open(reader))
            response = PM_400_Bad_Request;
        else {
            while(bp_next(reader)) {
                if(bp_match(reader, "ssid")) 
                    ok = ok && bp_get_string(reader, wifi.ssid, sizeof(wifi.ssid) / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
                else if(bp_match(reader, "password"))
                    ok = ok && bp_get_string(reader, wifi.password, sizeof(wifi.password) / sizeof(bp_type_t)) != BP_INVALID_LENGTH;
                else if(bp_match(reader, "diagnostics"))
                    wifi.diagnostics = bp_get_boolean(reader);
                else bp_next(reader);
            }
            bp_close(reader);
            ok = ok && wifi_write_to_nvs();
            wifi_stop();
            wifi_start();
            response = ok ? PM_204_Changed : PM_500_Internal_Server_Error;
        }
    }
    else
        response = PM_405_Method_Not_Allowed;

    return response;
}

void wifi_measure()
{
    int rssi;
    if(wifi.diagnostics && wifi.status >= WIFI_STATUS_CONNECTED && esp_wifi_sta_get_rssi(&rssi) == ESP_OK)
        measurements_append(wifi.mac, RESOURCE_WIFI, 0, 0, 0, 0, 0, 0, METRIC_RSSI, NOW, UNIT_dBm, rssi);
}
