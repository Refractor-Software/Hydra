/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    Application.h

    This is the entire surface the platform kernel is allowed to call into the engine through.
    Everything the engine needs for a given call arrives via ReAppContext - the engine never asks
    the platform for anything on its own, and the kernel never reaches past this struct into
    engine-internal state.

    Memory is deliberately absent from that handoff. The kernel exposes virtual memory primitives
    (RE_VirtualMemory_*) and the engine reserves and commits what it needs through them, so the
    engine can grow, trim, and decommit on its own terms. A single flat block handed across this
    boundary could do none of those things.
*/

#include <RE/Foundation/FoundationPrimitiveTypes.h>
#include <RE/Foundation/FoundationStringView.h>

#include <RE/Input/Input.h>

typedef struct ReAppContext
{
    ReInputQueue *input;

    /* Parsed process command line, UTF-8, kernel-owned for the process's lifetime. args[0] is
     * the executable path itself (GetCommandLineW()-based parsing includes it), not the first
     * user-supplied argument.
     */
    ReSint32      argCount;
    ReStringView *args;
} ReAppContext;

/* Called once, before the first RE_Application_Tick(). Brings up the engine's memory system among
 * everything else. Returns 0 if the engine failed to start.
 */
ReBool RE_Application_Init( ReAppContext *context );

/* Called once per kernel tick, after the platform has drained its own per-tick events into
 * context (e.g. the input queue) and computed deltaTime.
 */
void RE_Application_Tick( ReAppContext *context, ReFloat32 deltaTime );

/* Called once, after the last RE_Application_Tick(). Releases everything the engine owns, and
 * reports anything the memory decorators found on the way out.
 */
void RE_Application_Shutdown( ReAppContext *context );
