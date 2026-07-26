/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Input/Input.h>

void
RE_Input_PushEvent (ReInputQueue *queue, ReInputEvent event)
{
    if (queue->count >= RE_INPUT_QUEUE_CAPACITY)
    {
        return;
    }

    queue->events[queue->count] = event;
    queue->count += 1;
}

void
RE_Input_ClearQueue (ReInputQueue *queue)
{
    queue->count = 0;
}
