/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    Log.h

    Logging service. Engine code only ever calls the functions/macros declared here - never
    anything platform-prefixed. Formatting happens here (computation, allowed in engine code);
    the actual OS-facing write is the platform kernel's job, crossing through RE_Log_WriteRaw.
*/

typedef enum ReLogLevel
{
    ReLogLevel_Info,
    ReLogLevel_Warn,
    ReLogLevel_Error,
} ReLogLevel;

void RE_Log_Write( ReLogLevel level, const char *format, ... );

#define RE_LOG_INFO( ... )  RE_Log_Write( ReLogLevel_Info,  __VA_ARGS__ )
#define RE_LOG_WARN( ... )  RE_Log_Write( ReLogLevel_Warn,  __VA_ARGS__ )
#define RE_LOG_ERROR( ... ) RE_Log_Write( ReLogLevel_Error, __VA_ARGS__ )

/* Boundary point - defined by the platform kernel, never called directly outside log.c.
 * message is one already-formatted, plain-text (no color/escape codes) line, no trailing '\n'.
 */
extern void RE_Log_WriteRaw( ReLogLevel level, const char *message );
