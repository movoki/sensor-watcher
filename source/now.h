// Copyright (c) 2024 José Francisco Castro <me@fran.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef now_h
#define now_h

#include <time.h>

#define NOW (time(NULL) > 1680000000 ? time(NULL) : 0)

#endif
