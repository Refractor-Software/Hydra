/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    log.h

    Logging service. Engine code only ever calls the functions/macros declared here - never
    anything platform-prefixed. Formatting happens here (computation, allowed in engine code);
    the actual OS-facing write is the platform kernel's job, crossing through log_write_raw.
*/

typedef enum log_level
{
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level;

void log_write (log_level level, const char *format, ...);

#define log_info(...)  log_write (LOG_LEVEL_INFO,  __VA_ARGS__)
#define log_warn(...)  log_write (LOG_LEVEL_WARN,  __VA_ARGS__)
#define log_error(...) log_write (LOG_LEVEL_ERROR, __VA_ARGS__)

/* Boundary point - defined by the platform kernel, never called directly outside log.c.
 * message is one already-formatted, plain-text (no color/escape codes) line, no trailing '\n'.
 */
extern void log_write_raw (log_level level, const char *message);
