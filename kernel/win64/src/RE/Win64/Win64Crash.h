/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    win64_crash.h

    SEH-based crash handling: writes a minidump and shows a message box when an unhandled
    exception reaches wWinMain's __try/__except.
*/

#include "RE/Win64/Win64.h"

/* Prepares DbgHelp ahead of time, so as little work as possible happens for the first time
 * while the process is already faulted. Call once, early.
 */
void win64_crash_init (void);

/* Called directly as the filter expression in wWinMain's __except(...) - the only place
 * GetExceptionInformation() is valid to call from. Writes a .dmp next to the exe, shows a
 * MessageBoxW naming it, and returns EXCEPTION_EXECUTE_HANDLER.
 */
LONG WINAPI win64_crash_exception_filter (EXCEPTION_POINTERS *exceptionPointers);
