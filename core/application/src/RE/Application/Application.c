/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Application/Application.h>

#include <RE/Log/Log.h>

typedef struct ReAppState
{
    ReUint64 tickCount;
} ReAppState;

ReBool
RE_Application_Init (ReAppContext *context)
{
    ReAppState *state = (ReAppState *) context->memory;
    state->tickCount = 0;

    RE_LOG_INFO ("RE_Application_Init: engine started");

    for (ReSint32 i = 0; i < context->argCount; i += 1)
    {
        ReStringView arg = context->args[i];
        RE_LOG_INFO ("arg[%d]: %.*s", i, (int) arg.length, (const char *) arg.data);
    }

    return RE_True;
}

void
RE_Application_Tick (ReAppContext *context, ReFloat32 deltaTime)
{
    ReAppState *state = (ReAppState *) context->memory;
    state->tickCount += 1;

    (void) deltaTime;
    (void) context->input;
}

void
RE_Application_Shutdown (ReAppContext *context)
{
    (void) context;
}
