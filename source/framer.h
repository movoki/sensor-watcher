// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef framer_h
#define framer_h

#include <stdint.h>
#include <stdbool.h>
#include "bigpacks.h"

#define FRAMER_RECEIVING false
#define FRAMER_SENDING true

typedef struct {
    bool state;
    bool escape;
    bool start;
    uint32_t crc;
    bp_length_t length;
    bp_length_t max_length;
    bp_length_t index;
    uint8_t * buffer;
} framer_t;

uint32_t    framer_crc32(uint32_t crc, uint8_t value);
void        framer_set_buffer(framer_t *framer, uint8_t *pack_buffer, bp_length_t pack_max_length);
bool        framer_put_received_byte(framer_t *framer, uint8_t value);
uint8_t     framer_get_byte_to_send(framer_t *framer);
bp_length_t framer_get_length(framer_t *framer);
void        framer_set_length(framer_t *framer, bp_length_t value);
bool        framer_get_state(framer_t *framer);
void        framer_set_state(framer_t *framer, bool value);

#endif
