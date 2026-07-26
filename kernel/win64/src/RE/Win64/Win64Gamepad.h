#pragma once

/*
    win64_gamepad.h

    Polls XInput and pushes gamepad button/axis/connection events into the shared input_queue.
    XInput has no message-based notification, so unlike keyboard/mouse this has to be driven by
    an explicit per-tick poll rather than being fed from win64_window_proc.
*/

#include "RE/Win64/Win64.h"

#include <RE/Input/Input.h>

/* Polls all INPUT_GAMEPAD_COUNT controller slots and pushes whatever changed since the last
 * call into queue - call once per tick.
 */
void win64_gamepad_poll (input_queue *queue);
