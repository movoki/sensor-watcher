// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef pbuf_h
#define pbuf_h

#include <stdbool.h>

typedef struct {
  char *data;
  size_t size;
  size_t length;
} pbuf_t;

bool pbuf_printf(pbuf_t *buffer, const char *format, ...);
bool pbuf_putc(pbuf_t *buffer, char c);

#endif