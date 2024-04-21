// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef wifi_h
#define wifi_h

#include <stdalign.h>

#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>

#include "bigpacks.h"

#define WIFI_SSID_LENGTH		33
#define WIFI_PASSWORD_LENGTH	64

typedef struct {
	char ssid[WIFI_SSID_LENGTH];
	char password[WIFI_PASSWORD_LENGTH];
	uint64_t mac;
	uint8_t status;
	bool diagnostics;

    esp_netif_t *netif;
	bool reconnected;
	bool disconnected;
} wifi_t;

extern wifi_t wifi;

void wifi_init();
void wifi_start();
void wifi_stop();
bool wifi_connect();
bool wifi_read_from_nvs();
bool wifi_write_to_nvs();
void wifi_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
bool wifi_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t wifi_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);
void wifi_measure();

#endif