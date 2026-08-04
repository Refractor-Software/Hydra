/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Log/Log.h>

#include <stdarg.h>
#include <stdio.h>

void
RE_Log_Write( ReLogLevel level, const char *format, ... )
{
    char buffer[1024];

    va_list args;
    va_start( args, format );
    vsnprintf( buffer, sizeof( buffer ), format, args );
    va_end( args );

    RE_Log_WriteRaw( level, buffer );
}
