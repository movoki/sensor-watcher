// Copyright (c) 2022-2023 José Francisco Castro <me@fran.cc>
// SPDX short identifier: MIT

#include "bigpacks.h"

bp_type_t   bp_get_type(bp_pack_t *pack) { return pack->cursor->element_start[0] & BP_TYPE_MASK; };
bp_length_t bp_get_element_length(bp_pack_t *pack) { return pack->cursor->element_length; };
bp_length_t bp_get_content_length(bp_pack_t *pack) { return pack->cursor->element_length - 1; };

bool bp_is_none(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_MASK) == BP_NONE; };
bool bp_is_integer(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_MASK) == BP_INTEGER; };
bool bp_is_float(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_MASK) == BP_FLOAT; };
bool bp_is_string(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_MASK) == BP_STRING; };
bool bp_is_binary(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_MASK) == BP_BINARY; };
bool bp_is_list(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_MASK) == BP_LIST; };
bool bp_is_map(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_MASK) == BP_MAP; };

bool bp_is_boolean(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_GROUP_MASK) == BP_FALSE; };
bool bp_is_number(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_GROUP_MASK) == BP_INTEGER; };
bool bp_is_block(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_GROUP_MASK) == BP_STRING; };
bool bp_is_container(bp_pack_t *pack) { return (pack->cursor->element_start[0] & BP_TYPE_GROUP_MASK) == BP_LIST; };

bool bp_is_empty(bp_pack_t *pack) { return pack->cursor->element_length == 1; };

void bp_set_buffer(bp_pack_t *pack, bp_type_t * buffer, bp_length_t length)
{
    pack->cursor = &(pack->stack[0]);
    pack->cursor->element_start = buffer;
    pack->cursor->element_length = 0;
    pack->cursor->parent_start = buffer;
    pack->cursor->parent_length = length;
}

bp_length_t bp_get_offset(bp_pack_t *pack)
{
    return pack->cursor->element_start - pack->cursor->parent_start;
}

bool bp_set_offset(bp_pack_t *pack, bp_length_t offset)
{
    if(offset < pack->cursor->parent_length && offset < pack->stack[0].parent_length) {
        pack->cursor->element_start = pack->cursor->parent_start + offset;
        pack->cursor->element_length = 0;
        return true;
    }
    return false;
}

bool bp_has_next(bp_pack_t *pack) {
    return pack->cursor->element_start + pack->cursor->element_length < pack->cursor->parent_start + pack->cursor->parent_length;
}

bool bp_next(bp_pack_t *pack)
{
    if(bp_has_next(pack)) {
        pack->cursor->element_start += pack->cursor->element_length;
        pack->cursor->element_length = (pack->cursor->element_start[0] & BP_LENGTH_MASK) + 1;
        return true;
    }
    else
        return false;
}

bool bp_open(bp_pack_t *pack)
{
    if(bp_is_container(pack) && pack->cursor < &(pack->stack[BP_MAX_CURSOR_LEVELS - 1])) {
        (pack->cursor + 1)->parent_start = pack->cursor->element_start + 1;
        (pack->cursor + 1)->parent_length = pack->cursor->element_length - 1;
        (pack->cursor + 1)->element_start = pack->cursor->element_start;
        (pack->cursor + 1)->element_length = 1;
        pack->cursor += 1;
        return true;
    }
    else
        return false;
}

bool bp_close(bp_pack_t *pack)
{
    if(pack->cursor != &(pack->stack[0])) {
        pack->cursor -= 1;
        return true;
    }
    else
        return false;
}

bool bp_equals(bp_pack_t *pack, const char *string)
{
    if(!bp_is_string(pack))
        return false;
    for(bp_length_t i = 0; i < (pack->cursor->element_length - 1) * sizeof(bp_type_t); i++) {
        if(string[i] != ((char *)(pack->cursor->element_start + 1))[i])
            return false;
        if(!string[i])
            break;
    }
    return true;
}

bool bp_match(bp_pack_t *pack, const char *string)
{
    if(!bp_equals(pack, string))
        return false;
    bp_next(pack);
    return true;
}

bool bp_get_boolean(bp_pack_t *pack)
{
    return bp_is_boolean(pack) && (pack->cursor->element_start[0] & BP_BOOLEAN_MASK);
}

bp_integer_t bp_get_integer(bp_pack_t *pack)
{
    if(bp_is_integer(pack))
        return (bp_integer_t) pack->cursor->element_start[1];
    else if(bp_is_float(pack))
        return (bp_integer_t) bp_get_float(pack);
    else
        return 0;
}

bp_big_integer_t bp_get_big_integer(bp_pack_t *pack)
{
    if(bp_is_integer(pack) && pack->cursor->element_length == 1 + sizeof(bp_integer_t)/sizeof(bp_type_t))
        return (bp_integer_t) pack->cursor->element_start[1];
    else if(bp_is_integer(pack) && pack->cursor->element_length == 1 + sizeof(bp_big_integer_t)/sizeof(bp_type_t))
        return pack->cursor->element_start[1] | (bp_big_integer_t) pack->cursor->element_start[2] << sizeof(bp_type_t) * 8;
    else if(bp_is_float(pack))
        return (bp_integer_t) bp_get_double(pack);
    else
        return 0;
}

bp_float_t bp_get_float(bp_pack_t *pack)
{
    if(bp_is_float(pack))
        return *((bp_float_t *) &(pack->cursor->element_start[1]));
    else if(bp_is_integer(pack))
        return (bp_float_t) bp_get_integer(pack);
    else
        return 0;
}

bp_double_t bp_get_double(bp_pack_t *pack)
{
    if(bp_is_float(pack) && pack->cursor->element_length == 1 + sizeof(bp_double_t)/sizeof(bp_type_t)) {
        uint64_t n = (uint64_t) pack->cursor->element_start[1] | (uint64_t) pack->cursor->element_start[2] << sizeof(bp_type_t) * 8;
        return *((bp_double_t *) &n);
    }
    else if(bp_is_float(pack) && pack->cursor->element_length == 1 + sizeof(bp_float_t)/sizeof(bp_type_t))
        return *((bp_float_t *) &(pack->cursor->element_start[1]));
    else if(bp_is_integer(pack))
        return (bp_float_t) bp_get_big_integer(pack);
    else
        return 0;
}

bp_length_t bp_get_string(bp_pack_t *pack, char *buffer, bp_length_t buffer_length)
{
    if(pack->cursor->element_length - 1 <= buffer_length) {
        for(bp_length_t i = 0; i < (pack->cursor->element_length - 1) * sizeof(bp_type_t); i++)
            buffer[i] = ((uint8_t *)(pack->cursor->element_start + 1))[i];
        return pack->cursor->element_length - 1;
    }
    else
        return BP_INVALID_LENGTH;
}

bp_length_t bp_get_binary(bp_pack_t *pack, bp_type_t *buffer, bp_length_t buffer_length)
{
    if(pack->cursor->element_length - 1 <= buffer_length) {
        for(bp_length_t i = 0; i < pack->cursor->element_length - 1; i++)
            buffer[i] = pack->cursor->element_start[i + 1];
        return pack->cursor->element_length - 1;
    }
    else
        return BP_INVALID_LENGTH;
}

bp_length_t bp_free_space(bp_pack_t *pack)
{
    return pack->cursor->parent_start + pack->cursor->parent_length - pack->cursor->element_start;
}

bool bp_put_boolean(bp_pack_t *pack, bool value)
{
    if(bp_free_space(pack)) {
        pack->cursor->element_start[0] = (value ? BP_TRUE : BP_FALSE);
        pack->cursor->element_start += 1;
        pack->cursor->element_length = 0;
        return true;
    }
    else
        return false;
}

bool bp_put_none(bp_pack_t *pack)
{
    if(bp_free_space(pack)) {
        pack->cursor->element_start[0] = BP_NONE;
        pack->cursor->element_start += 1;
        pack->cursor->element_length = 0;
        return true;
    }
    else
        return false;
}

bool bp_put_integer(bp_pack_t *pack, bp_integer_t value)
{
    if(bp_free_space(pack) >= 1 + sizeof(bp_integer_t) / sizeof(bp_type_t)) {
        pack->cursor->element_start[0] = BP_INTEGER | sizeof(bp_integer_t) / sizeof(bp_type_t);
        pack->cursor->element_start[1] = (bp_type_t) value;
        pack->cursor->element_start += 1 + sizeof(bp_integer_t) / sizeof(bp_type_t);
        pack->cursor->element_length = 0;
        return true;
    }
    else
        return false;
}

bool bp_put_big_integer(bp_pack_t *pack, bp_big_integer_t value)
{
    if(bp_free_space(pack) >= 1 + sizeof(bp_big_integer_t) / sizeof(bp_type_t)) {
        pack->cursor->element_start[0] = BP_INTEGER | sizeof(bp_big_integer_t) / sizeof(bp_type_t);
        pack->cursor->element_start[1] = (bp_type_t) (value & 0xFFFFFFFF);
        pack->cursor->element_start[2] = (bp_type_t) (value >> sizeof(bp_type_t) * 8);
        pack->cursor->element_start += 1 + sizeof(bp_big_integer_t) / sizeof(bp_type_t);
        pack->cursor->element_length = 0;
        return true;
    }
    else
        return false;
}

bool bp_put_float(bp_pack_t *pack, bp_float_t value)
{
    if(bp_free_space(pack) >= 1 + sizeof(bp_float_t) / sizeof(bp_type_t)) {
        pack->cursor->element_start[0] = BP_FLOAT | sizeof(bp_float_t) / sizeof(bp_type_t);
        pack->cursor->element_start[1] = *((bp_type_t *) (&value));   /// 🤪
        pack->cursor->element_start += 1 + sizeof(bp_float_t) / sizeof(bp_type_t);
        pack->cursor->element_length = 0;
        return true;
    }
    else
        return false;
}

bool bp_put_double(bp_pack_t *pack, bp_double_t value)
{
    if(bp_free_space(pack) >= 1 + sizeof(bp_double_t) / sizeof(bp_type_t)) {
        pack->cursor->element_start[0] = BP_FLOAT | sizeof(bp_double_t) / sizeof(bp_type_t);
        pack->cursor->element_start[1] = *((bp_type_t *) &value);
        pack->cursor->element_start[2] = *((bp_type_t *) &value + 1);
        pack->cursor->element_start += 1 + sizeof(bp_double_t) / sizeof(bp_type_t);
        pack->cursor->element_length = 0;
        return true;
    }
    else
        return false;
}

bool bp_put_string(bp_pack_t *pack, const char *value)
{
    bp_length_t string_length;
    bp_length_t content_length;

    for(string_length = 0; value[string_length]; string_length++);
    content_length = (string_length + 1 + 3) / sizeof(bp_type_t);

    if(bp_free_space(pack) >= 1 + content_length) {
        pack->cursor->element_start[0] = BP_STRING | content_length;
        for(bp_length_t i = 0; i < content_length * sizeof(bp_type_t); i++)
            ((char *)(pack->cursor->element_start + 1))[i] = i < string_length ? value[i] : 0;
        pack->cursor->element_start += 1 + content_length;
        pack->cursor->element_length = 0;
        return true;
    }
    else
        return false;
}

bool bp_put_binary(bp_pack_t *pack, bp_type_t *value, bp_length_t length)
{
    if(bp_free_space(pack) >= 1 + length) {
        pack->cursor->element_start[0] = BP_BINARY | length;
        for(bp_length_t i = 0; i < length; i++)
            pack->cursor->element_start[1 + i] = value[i];
        pack->cursor->element_start += 1 + length;
        pack->cursor->element_length = 0;
        return true;
    }
    else
        return false;
}

bool bp_create_container(bp_pack_t *pack, bp_type_t type)
{
    if(bp_free_space(pack) >= 1 && pack->cursor < &(pack->stack[BP_MAX_CURSOR_LEVELS - 1])) {
        pack->cursor->element_start[0] = type;
        (pack->cursor + 1)->parent_start = pack->cursor->element_start + 1;
        (pack->cursor + 1)->parent_length = pack->cursor->parent_start + pack->cursor->parent_length - pack->cursor->element_start - 1;
        (pack->cursor + 1)->element_start = pack->cursor->element_start + 1;
        (pack->cursor + 1)->element_length = 0;
        pack->cursor += 1;
        return true;
    }
    else
        return false;
}

bool bp_finish_container(bp_pack_t *pack)
{
    if(pack->cursor > &(pack->stack[0])) {
        pack->cursor->parent_start[-1] |= pack->cursor->element_start - pack->cursor->parent_start;
        (pack->cursor - 1)->element_start = pack->cursor->element_start;
        (pack->cursor - 1)->element_length = 0;
        pack->cursor -= 1;
        return true;
    }
    else
        return false;
}

void bp_reset_cursor(bp_pack_t *pack)
{
    pack->cursor = &(pack->stack[0]);
}

bool bp_save_cursor(bp_pack_t *pack)
{
    if(pack->cursor < &(pack->stack[BP_MAX_CURSOR_LEVELS - 1])) {
        (pack->cursor + 1)->parent_start = pack->cursor->parent_start;
        (pack->cursor + 1)->parent_length = pack->cursor->parent_length;
        (pack->cursor + 1)->element_start = pack->cursor->element_start;
        (pack->cursor + 1)->element_length = pack->cursor->element_length;
        pack->cursor += 1;
        return true;
    }
    else
        return false;
}

bool bp_restore_cursor(bp_pack_t *pack)
{
    return bp_close(pack);
}
