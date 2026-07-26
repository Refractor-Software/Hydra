/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Application/Application.h>

#include <RE/Log/Log.h>

typedef struct app_state
{
    u64 tickCount;
} app_state;

b8
application_init (app_context *context)
{
    app_state *state = (app_state *) context->memory;
    state->tickCount = 0;

    log_info ("application_init: engine started");

    for (s32 i = 0; i < context->argCount; i += 1)
    {
        string_view arg = context->args[i];
        log_info ("arg[%d]: %.*s", i, (int) arg.length, (const char *) arg.data);
    }

    return 1;
}

void
application_tick (app_context *context, f32 deltaTime)
{
    app_state *state = (app_state *) context->memory;
    state->tickCount += 1;

    (void) deltaTime;
    (void) context->input;
}

void
application_shutdown (app_context *context)
{
    (void) context;
}
