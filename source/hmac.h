// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef hmac_h
#define hmac_h

#include "sha256.h"

typedef uint32_t hmac_sha256_key_t[16];											// 64 bytes
typedef uint32_t hmac_sha256_hash_t[SHA256_SIZE_BYTES / sizeof(uint32_t)];		// 32 bytes

void hmac_sha256_sign(uint8_t *message, size_t message_length, hmac_sha256_key_t key, hmac_sha256_hash_t hash);
size_t hmac_hex_decode(void* bytes, size_t bytes_length, const char* hex, size_t hex_length);

#endif
