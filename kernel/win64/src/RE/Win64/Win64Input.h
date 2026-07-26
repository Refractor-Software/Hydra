/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    win64_input.h

    Translates Win32 keyboard/mouse messages into the platform-independent input_event queue
    defined by the "input" service. This is the only place in the whole project that knows
    scan codes, virtual-key codes, or WM_* message values even exist.
*/

#include "RE/Win64/Win64.h"

#include <RE/Input/Input.h>

/* Clears the queue - call once per tick, before draining the Win32 message queue. */
void win64_input_reset (void);

/* Feeds a single Win32 message through input translation. Ignores anything it doesn't care about. */
void win64_input_handle_message (UINT message, WPARAM wParam, LPARAM lParam);

/* Releases (as synthetic key-up events) any keys still marked held - call on focus loss so a key
 * held during an alt-tab doesn't read as stuck down forever.
 */
void win64_input_release_all_held_keys (void);

input_queue * win64_input_queue_get (void);
