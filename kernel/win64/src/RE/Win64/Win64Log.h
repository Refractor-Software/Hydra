/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    Win64Log.h

    Console/debug-output backend for the "log" service - defines RE_Log_WriteRaw (declared in
    RE/Log/Log.h). This is the only place in the whole project that knows a console or
    OutputDebugString even exist for logging purposes.
*/

#include "RE/Win64/Win64.h"

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/* Attaches to a parent console if launched from one, otherwise allocates a new one. Enables
 * VT100 color codes. Call once, before anything logs.
 */
void Win64_Log_Init (void);

/* Releases the console (if one was allocated/attached). Call once, after everything is done
 * logging.
 */
void Win64_Log_Shutdown (void);
