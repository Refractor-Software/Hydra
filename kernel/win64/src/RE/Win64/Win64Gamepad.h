#pragma once

/*
    Win64Gamepad.h

    Polls XInput and pushes gamepad button/axis/connection events into the shared ReInputQueue.
    XInput has no message-based notification, so unlike keyboard/mouse this has to be driven by
    an explicit per-tick poll rather than being fed from Win64_WindowProc.
*/

#include "RE/Win64/Win64.h"

#include <RE/Input/Input.h>

/* Polls all RE_INPUT_GAMEPAD_COUNT controller slots and pushes whatever changed since the last
 * call into queue - call once per tick.
 */
void Win64_Gamepad_Poll (ReInputQueue *queue);
