/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "input/input.h"

void
input_queue_push (input_queue *queue, input_event event)
{
    if (queue->count >= INPUT_QUEUE_CAPACITY)
    {
        return;
    }

    queue->events[queue->count] = event;
    queue->count += 1;
}

void
input_queue_clear (input_queue *queue)
{
    queue->count = 0;
}
