/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    Win64Thread.h

    Raw OS thread primitives for the Windows platform. Currently just thread naming - the
    future job system's worker-thread spawner belongs here too.
*/

#include "RE/Win64/Win64.h"

/* Names a thread for debuggers/profilers - does BOTH SetThreadDescription (modern debuggers,
 * dynamically resolved since it's Win10 1607+) and the legacy RaiseException(0x406D1388, ...)
 * convention (older tooling), unconditionally - they inform different debugger generations,
 * this isn't a fallback chain.
 */
void Win64_Thread_SetThreadName (HANDLE thread, const wchar_t *name);

/* Win64_Thread_SetThreadName (GetCurrentThread (), name). */
void Win64_Thread_SetCurrentThreadName (const wchar_t *name);
