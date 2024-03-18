// Copyright (c) 2022-2023 José Francisco Castro <me@fran.cc>
// SPDX short identifier: MIT

#ifndef bigpacks_h
#define bigpacks_h

#include <stdint.h>
#include <stdbool.h>

#define BP_FALSE    0x00000000
#define BP_TRUE     0x10000000
#define BP_NONE     0x20000000
#define BP_INTEGER  0x40000000
#define BP_FLOAT    0x50000000

#define BP_LIST     0x80000000
#define BP_MAP      0x90000000
#define BP_STRING   0xC0000000
#define BP_BINARY   0xD0000000

#define BP_LENGTH_MASK         0x0FFFFFFF
#define BP_TYPE_MASK           0xF0000000
#define BP_TYPE_GROUP_MASK     0xE0000000
#define BP_BOOLEAN_MASK        0x10000000

#define BP_INVALID_LENGTH      0xFFFFFFFF

#define BP_MAX_CURSOR_LEVELS   4


typedef uint32_t bp_type_t;
typedef uint32_t bp_length_t;
typedef int32_t  bp_integer_t;
typedef int64_t  bp_big_integer_t;
typedef float    bp_float_t;
typedef double   bp_double_t;
typedef struct {
    bp_type_t *  parent_start;
    bp_type_t *  element_start;
    bp_length_t parent_length;
    bp_length_t element_length;
 } bp_cursor_t;

typedef struct {
    bp_cursor_t   stack[BP_MAX_CURSOR_LEVELS];
    bp_cursor_t * cursor;
} bp_pack_t;

bp_type_t   bp_get_type(bp_pack_t *pack);
bp_length_t bp_get_element_length(bp_pack_t *pack);
bp_length_t bp_get_content_length(bp_pack_t *pack);

bool bp_is_boolean(bp_pack_t *pack);
bool bp_is_none(bp_pack_t *pack);
bool bp_is_integer(bp_pack_t *pack);
bool bp_is_float(bp_pack_t *pack);
bool bp_is_string(bp_pack_t *pack);
bool bp_is_binary(bp_pack_t *pack);
bool bp_is_list(bp_pack_t *pack);
bool bp_is_map(bp_pack_t *pack);

bool bp_is_number(bp_pack_t *pack);
bool bp_is_block(bp_pack_t *pack);
bool bp_is_container(bp_pack_t *pack);

bool bp_is_empty(bp_pack_t *pack);

void bp_set_buffer(bp_pack_t *pack, bp_type_t * buffer, bp_length_t length);
bp_length_t bp_free_space(bp_pack_t *pack);
bp_length_t bp_get_offset(bp_pack_t *pack);
bool bp_set_offset(bp_pack_t *pack, bp_length_t offset);

bool bp_has_next(bp_pack_t *pack);
bool bp_next(bp_pack_t *pack);
bool bp_open(bp_pack_t *pack);
bool bp_close(bp_pack_t *pack);
bool bp_equals(bp_pack_t *pack, const char *string);
bool bp_match(bp_pack_t *pack, const char *string);

bool             bp_get_boolean(bp_pack_t *pack);
bp_integer_t     bp_get_integer(bp_pack_t *pack);
bp_big_integer_t bp_get_big_integer(bp_pack_t *pack);
bp_float_t       bp_get_float(bp_pack_t *pack);
bp_double_t      bp_get_double(bp_pack_t *pack);
bp_length_t      bp_get_string(bp_pack_t *pack, char *buffer, bp_length_t buffer_length);
bp_length_t      bp_get_binary(bp_pack_t *pack, bp_type_t *buffer, bp_length_t buffer_length);

bool bp_put_boolean(bp_pack_t *pack, bool value);
bool bp_put_none(bp_pack_t *pack);
bool bp_put_integer(bp_pack_t *pack, bp_integer_t value);
bool bp_put_big_integer(bp_pack_t *pack, bp_big_integer_t value);
bool bp_put_float(bp_pack_t *pack, bp_float_t value);
bool bp_put_double(bp_pack_t *pack, bp_double_t value);
bool bp_put_string(bp_pack_t *pack, const char *value);
bool bp_put_binary(bp_pack_t *pack, bp_type_t *value, bp_length_t length);
bool bp_create_container(bp_pack_t *pack, bp_type_t type);
bool bp_finish_container(bp_pack_t *pack);

bool bp_save_cursor(bp_pack_t *pack);
bool bp_restore_cursor(bp_pack_t *pack);

#endif
