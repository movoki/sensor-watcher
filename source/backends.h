// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef backends_h
#define backends_h

#define BACKENDS_NUM_MAX	3

#define BACKEND_SERVICE_LENGTH		32
#define BACKEND_URI_LENGTH			256
#define BACKEND_PEM_LENGTH			2048
#define BACKEND_USER_LENGTH			2048
#define BACKEND_KEY_LENGTH			2048
#define BACKEND_CLIENT_ID_LENGTH	64
#define BACKEND_TOPIC_LENGTH		256
#define BACKEND_MESSAGE_LENGTH		256
#define BACKEND_CONTENT_TYPE_LENGTH	64

#define BACKEND_TEMPLATE_HEADER_LENGTH			256
#define BACKEND_TEMPLATE_ROW_LENGTH				256
#define BACKEND_TEMPLATE_SEPARATOR_LENGTH		4
#define BACKEND_TEMPLATE_FOOTER_LENGTH			256

#define BACKEND_ERROR_TLS_STACK_BASE		0x10000000
#define BACKEND_ERROR_TRANSPORT_SOCK_BASE	0x20000000
#define BACKEND_ERROR_MQTT_RETURN_CODE_BASE	0x30000000
#define BACKEND_ERROR_HTTP_STATUS_BASE		0x40000000

#include "bigpacks.h"

typedef struct {
	uint8_t auth;
	uint8_t format;
	char service[BACKEND_SERVICE_LENGTH];
	char uri[BACKEND_URI_LENGTH];
	char user[BACKEND_USER_LENGTH];
	char key[BACKEND_KEY_LENGTH];
	char server_cert[BACKEND_PEM_LENGTH];
	char output_topic[BACKEND_TOPIC_LENGTH];
	char input_topic[BACKEND_TOPIC_LENGTH];
	char client_id[BACKEND_CLIENT_ID_LENGTH];

	char content_type[BACKEND_CONTENT_TYPE_LENGTH];
	char template_header[BACKEND_TEMPLATE_HEADER_LENGTH];
	char template_row[BACKEND_TEMPLATE_ROW_LENGTH];
	char template_row_separator[BACKEND_TEMPLATE_SEPARATOR_LENGTH];
	char template_name_separator[BACKEND_TEMPLATE_SEPARATOR_LENGTH];
	char template_footer[BACKEND_TEMPLATE_FOOTER_LENGTH];

	void *handle;
	int32_t status;
	int32_t error;
	char message[BACKEND_MESSAGE_LENGTH];
} backend_t;


extern backend_t backends[];
extern bool backends_modified;
extern bool backends_started;

void backends_init();
bool backends_read_from_nvs();
bool backends_write_to_nvs();
void backends_start();
void backends_stop();
void backends_clear_status();
bool backend_pack(bp_pack_t *writer, uint32_t index);
bool backend_unpack(bp_pack_t *reader, uint32_t index);
bool backends_schema_handler(char *resource_name, bp_pack_t *writer);
uint32_t backends_resource_handler(uint32_t method, bp_pack_t *reader, bp_pack_t *writer);

#endif

