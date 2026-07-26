/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    win64_thread.h

    Raw OS thread primitives for the Windows platform. Currently just thread naming - the
    future job system's worker-thread spawner belongs here too.
*/

#include "RE/Win64/Win64.h"

/* Names a thread for debuggers/profilers - does BOTH SetThreadDescription (modern debuggers,
 * dynamically resolved since it's Win10 1607+) and the legacy RaiseException(0x406D1388, ...)
 * convention (older tooling), unconditionally - they inform different debugger generations,
 * this isn't a fallback chain.
 */
void win64_thread_set_thread_name (HANDLE thread, const wchar_t *name);

/* win64_thread_set_thread_name (GetCurrentThread (), name). */
void win64_thread_set_current_thread_name (const wchar_t *name);
