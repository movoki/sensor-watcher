// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sha256.h"
#include "hmac.h"

void hmac_sha256_sign(uint8_t *message, size_t message_length, hmac_sha256_key_t key, hmac_sha256_hash_t hash)
{
	sha256_context ctx;
	hmac_sha256_key_t ipad;
	hmac_sha256_key_t opad;

	for (int i = 0; i < sizeof(hmac_sha256_key_t) / sizeof(uint32_t); i++) {
		ipad[i] = key[i] ^ 0x36363636;
		opad[i] = key[i] ^ 0x5c5c5c5c;
	}

    sha256_init(&ctx);
    sha256_hash(&ctx, ipad, sizeof(hmac_sha256_key_t));
    sha256_hash(&ctx, message, message_length);
    sha256_done(&ctx, (uint8_t *) hash);

    sha256_init(&ctx);
    sha256_hash(&ctx, opad, sizeof(hmac_sha256_key_t));
    sha256_hash(&ctx, hash, sizeof(hmac_sha256_hash_t));
    sha256_done(&ctx, (uint8_t *) hash);
}

size_t hmac_hex_decode(void* bytes, size_t bytes_length, const char* hex, size_t hex_length)
{
  unsigned i, j;
  for(i = 0, j = 0; (i < bytes_length) && (j+1 < hex_length); ++i, j+=2)
    ((uint8_t *) bytes)[i] = (((hex[j+0] & 0xf) + (hex[j+0] >> 6) * 9) << 4) | ((hex[j+1] & 0xf) + (hex[j+1] >> 6) * 9);
  return i;
}

