// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdarg.h>

#include "pbuf.h"

bool pbuf_printf(pbuf_t *buffer, const char *format, ...)
{
    if(buffer->length >= buffer->size)
        return false;

    va_list args;
    va_start(args, format);
    int n = vsnprintf(buffer->data + buffer->length, buffer->size - buffer->length, format, args);
    va_end(args);

    if(n >= 0 && n + buffer->length < buffer->size) {
        buffer->length += n;
        return true;
    }
    else
        return false;
}

bool pbuf_putc(pbuf_t *buffer, char c)
{
    if(buffer->length >= buffer->size - 1)
        return false;

    buffer->data[buffer->length++] = c;
    buffer->data[buffer->length] = 0;
    return true;
}
