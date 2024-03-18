// Copyright (c) 2022-2023 José Francisco Castro <me@fran.cc>
// SPDX short identifier: MIT

#ifndef postman_h
#define postman_h

#include <string.h>
#include <time.h>

#include "bigpacks.h"
#include "hmac.h"

#define PM_MAX_RESOURCES 16
#define PM_MAX_PATH_LENGTH 16

#define PM_GET    0x01
#define PM_POST   0x02
#define PM_PUT    0x03
#define PM_DELETE 0x04

#define PM_200_OK                 0x20
#define PM_201_Created            0x21
#define PM_202_Deleted            0x22
#define PM_204_Changed            0x24
#define PM_205_Content            0x25
#define PM_400_Bad_Request        0x40
#define PM_401_Unauthorized       0x41
#define PM_403_Forbidden          0x43
#define PM_404_Not_Found          0x44
#define PM_405_Method_Not_Allowed 0x45
#define PM_408_Timeout            0x48
#define PM_413_Request_Entity_Too_Large     0x4D
#define PM_500_Internal_Server_Error        0x50

typedef uint8_t pm_response_code_t;

typedef uint32_t (*handler_t)(uint32_t, bp_pack_t *, bp_pack_t *);

typedef struct {
    bp_pack_t reader;
    bp_pack_t writer;
    uint8_t registered_resources;
    struct {
        const char *path;
        handler_t handler;
    } resources[PM_MAX_RESOURCES];
} postman_t;

void postman_init(postman_t *postman);
bool postman_register_resource(postman_t *postman, const char *path, handler_t handler);
bp_length_t postman_handle_pack(postman_t *pm, bp_type_t *buffer, bp_length_t length, bp_length_t max_length, time_t now, char *id, hmac_sha256_key_t key);
bool postman_put_signature(postman_t *pm, time_t now, char *id, hmac_sha256_key_t key);

#endif
