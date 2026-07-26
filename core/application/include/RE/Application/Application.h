/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    Application.h

    This is the entire surface the platform kernel is allowed to call into the engine through.
    Everything the engine needs for a given call arrives via ReAppContext - the engine never asks
    the platform for anything on its own, and the kernel never reaches past this struct into
    engine-internal memory (it owns context->memory, but doesn't interpret what's inside it).
*/

#include <RE/Foundation/FoundationPrimitiveTypes.h>
#include <RE/Foundation/FoundationStringView.h>

#include <RE/Input/Input.h>

/* Kernel-owned. The memory block's internal layout is intentionally undecided - the engine casts
 * it to whatever internal state it needs, and the kernel never interprets those bytes itself.
 */
typedef struct ReAppContext
{
    void *memory;
    ReUint64   memorySize;

    ReInputQueue *input;

    /* Parsed process command line, UTF-8, kernel-owned for the process's lifetime. args[0] is
     * the executable path itself (GetCommandLineW()-based parsing includes it), not the first
     * user-supplied argument.
     */
    ReSint32          argCount;
    ReStringView *args;
} ReAppContext;

/* Called once, after context->memory has been reserved/committed and before the first
 * RE_Application_Tick(). Returns 0 if the engine failed to start.
 */
ReBool RE_Application_Init( ReAppContext *context );

/* Called once per kernel tick, after the platform has drained its own per-tick events into
 * context (e.g. the input queue) and computed deltaTime.
 */
void RE_Application_Tick( ReAppContext *context, ReFloat32 deltaTime );

/* Called once, after the last RE_Application_Tick(), before context->memory is released. */
void RE_Application_Shutdown( ReAppContext *context );
