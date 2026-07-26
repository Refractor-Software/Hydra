/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    win64_log.h

    Console/debug-output backend for the "log" service - defines log_write_raw (declared in
    log/log.h). This is the only place in the whole project that knows a console or
    OutputDebugString even exist for logging purposes.
*/

#include "RE/Win64/Win64.h"

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/* Attaches to a parent console if launched from one, otherwise allocates a new one. Enables
 * VT100 color codes. Call once, before anything logs.
 */
void win64_log_init (void);

/* Releases the console (if one was allocated/attached). Call once, after everything is done
 * logging.
 */
void win64_log_shutdown (void);
