// Copyright (c) 2022-2023 José Francisco Castro <me@fran.cc>
// SPDX short identifier: MIT

#include <time.h>

#include "postman.h"

void postman_init(postman_t *postman)
{
    postman->registered_resources = 0;
}

bool postman_register_resource(postman_t *postman, const char *path, handler_t handler)
{
    if(postman->registered_resources < PM_MAX_RESOURCES) {
        postman->resources[postman->registered_resources].path = path;
        postman->resources[postman->registered_resources].handler = handler;
        postman->registered_resources += 1;
        return true;
    }
    else
        return false;
}

// 👻

bp_length_t postman_handle_pack(postman_t *pm, bp_type_t *buffer, bp_length_t length, bp_length_t max_length, time_t now, char *id, hmac_sha256_key_t key)
{
    time_t timestamp;
    uint32_t method_token = 0;
    uint32_t response_code = 0;

    bool signature_verified = false;
    bp_length_t signed_data_length;
    bp_length_t signature_length;
    hmac_sha256_hash_t hash;
    hmac_sha256_hash_t signature;

    bp_set_buffer(&pm->reader, buffer, length);
    bp_set_buffer(&pm->writer, buffer, max_length - 1);  // reserve one uint32_t for crc32

    if(!(bp_next(&pm->reader) && bp_is_integer(&pm->reader) && (method_token = bp_get_integer(&pm->reader)) &&      // method_token
         bp_next(&pm->reader) && bp_is_list(&pm->reader) &&                                                         // path
         bp_set_offset(&pm->writer, bp_get_offset(&pm->reader)) && bp_next(&pm->writer) && bp_next(&pm->writer)))   // point pm->writer to body
            response_code = PM_400_Bad_Request;
    else {
        if(key) {
            bp_save_cursor(&pm->reader);
            if(!(bp_next(&pm->reader) &&                                                                                  // body
                 bp_next(&pm->reader) && bp_is_integer(&pm->reader) && (timestamp = bp_get_big_integer(&pm->reader)) &&   // timestamp
                 bp_next(&pm->reader) && bp_is_string(&pm->reader)  &&                                                    // id
                 bp_next(&pm->reader) && bp_is_binary(&pm->reader)  &&                                                    // signature
                 (signed_data_length = bp_get_offset(&pm->reader) * sizeof(bp_type_t)) &&
                 (signature_length = bp_get_binary(&pm->reader, (bp_type_t *) &signature, sizeof(signature) / sizeof(bp_type_t))) != BP_INVALID_LENGTH &&
                 (signature_length * sizeof(bp_type_t) == sizeof(signature))))
                    response_code = PM_400_Bad_Request;
            else {
                hmac_sha256_sign((uint8_t *) buffer, signed_data_length, key, hash);
                if(timestamp + 600 < now)                                   // only allows timestamps not older than 10 minutes
                    response_code = PM_408_Timeout;
                else if(memcmp(signature, hash, sizeof(hash)))
                    response_code = PM_403_Forbidden;
                else
                    signature_verified = true;
            }
            bp_restore_cursor(&pm->reader);
        }

        if(!key || signature_verified) {
            if(bp_is_empty(&pm->reader)) {                                  // if path is empty reply with index of resources
                if(method_token >> 24 == PM_GET) {
                    bp_create_container(&pm->writer, BP_LIST);
                    for(int i = 0; i != pm->registered_resources; i++)
                        bp_put_string(&pm->writer, pm->resources[i].path);
                    bp_finish_container(&pm->writer);
                    response_code = PM_205_Content;
                }
                else
                    response_code = PM_405_Method_Not_Allowed;
            }
            else if(!bp_open(&pm->reader))                                  // check for cursor stack overflow
                response_code = PM_500_Internal_Server_Error;
            else if(!bp_next(&pm->reader) || !bp_is_string(&pm->reader))
                response_code = PM_404_Not_Found;                           // Root part of the path must be string
            else {
                response_code = PM_404_Not_Found;
                for(int i = 0; i != pm->registered_resources; i++) {
                    if(bp_equals(&pm->reader, pm->resources[i].path)) {
                        response_code = (*pm->resources[i].handler)(method_token >> 24, &pm->reader, &pm->writer);
                        break;
                    }
                }
            }
        }
    }

    if(response_code < PM_400_Bad_Request) {
        bp_save_cursor(&pm->writer);
        bp_set_offset(&pm->writer, 0);
        bp_put_integer(&pm->writer, (response_code << 24) | (method_token & 0x00FFFFFF));
        bp_restore_cursor(&pm->writer);

        if(key && signature_verified && !postman_put_signature(pm, now, id, key))
            response_code = PM_500_Internal_Server_Error;
    }

    if(response_code >= PM_400_Bad_Request) {       // if error, return only response_code and token
        bp_reset_cursor(&pm->writer);
        bp_set_offset(&pm->writer, 0);
        bp_put_integer(&pm->writer, (response_code << 24) | (method_token & 0x00FFFFFF));
    }

    return bp_get_offset(&pm->writer);
}

bool postman_put_signature(postman_t *pm, time_t now, char *id, hmac_sha256_key_t key)
{
    bool ok = true;
    hmac_sha256_hash_t hash;

    ok = ok && bp_put_big_integer(&pm->writer, now);
    ok = ok && bp_put_string(&pm->writer, id);
    hmac_sha256_sign((uint8_t *) pm->writer.cursor->parent_start, bp_get_offset(&pm->writer) * sizeof(bp_type_t), key, hash);
    ok = ok && bp_put_binary(&pm->writer, (bp_type_t *) hash, sizeof(hash) / sizeof(bp_type_t));
    return ok;
}
