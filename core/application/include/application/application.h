/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    application.h

    This is the entire surface the platform kernel is allowed to call into the engine through.
    Everything the engine needs for a given call arrives via app_context - the engine never asks
    the platform for anything on its own, and the kernel never reaches past this struct into
    engine-internal memory (it owns context->memory, but doesn't interpret what's inside it).
*/

#include "foundation/primitive/foundation_primitive_types.h"

#include "input/input.h"

/* Kernel-owned. The memory block's internal layout is intentionally undecided - the engine casts
 * it to whatever internal state it needs, and the kernel never interprets those bytes itself.
 */
typedef struct app_context
{
    void *memory;
    u64   memorySize;

    input_queue *input;
} app_context;

/* Called once, after context->memory has been reserved/committed and before the first
 * application_tick(). Returns 0 if the engine failed to start.
 */
b8 application_init (app_context *context);

/* Called once per kernel tick, after the platform has drained its own per-tick events into
 * context (e.g. the input queue) and computed deltaTime.
 */
void application_tick (app_context *context, f32 deltaTime);

/* Called once, after the last application_tick(), before context->memory is released. */
void application_shutdown (app_context *context);
