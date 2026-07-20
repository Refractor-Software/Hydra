/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "log/log.h"

#include <stdarg.h>
#include <stdio.h>

void
log_write (log_level level, const char *format, ...)
{
    char buffer[1024];

    va_list args;
    va_start (args, format);
    vsnprintf (buffer, sizeof (buffer), format, args);
    va_end (args);

    log_write_raw (level, buffer);
}
