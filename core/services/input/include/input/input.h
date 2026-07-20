/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    input:

    A service exposing input as an ordered queue of events (key, mouse move/button/wheel, gamepad
    button/axis/connection), rather than a per-frame snapshot, so consumers can see every discrete
    event in the order it happened instead of only "what's true this frame."

    This header has no platform knowledge whatsoever. Keys are identified by a platform-independent
    physical-key enum (input_key), not a virtual-key code or scan code - the platform kernel is
    responsible for translating whatever its native input APIs give it into these events.

    The queue itself is a fixed-capacity array, not a growable/allocated container - the platform kernel
    owns an instance of it, clears it once per tick, and fills it as events arrive.
*/

#include "foundation/primitive/foundation_primitive_types.h"

typedef enum input_event_kind
{
    INPUT_EVENT_KEY,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_BUTTON,
    INPUT_EVENT_MOUSE_WHEEL,
    INPUT_EVENT_GAMEPAD_BUTTON,
    INPUT_EVENT_GAMEPAD_AXIS,
    INPUT_EVENT_GAMEPAD_CONNECTION,
} input_event_kind;

/* Physical key identity (scan-code-based on platforms where that distinction exists), not a
 * layout-dependent virtual key - WASD should mean the same physical keys on any keyboard layout.
 */
typedef enum input_key
{
    INPUT_KEY_UNKNOWN = 0,

    INPUT_KEY_A, INPUT_KEY_B, INPUT_KEY_C, INPUT_KEY_D, INPUT_KEY_E, INPUT_KEY_F,
    INPUT_KEY_G, INPUT_KEY_H, INPUT_KEY_I, INPUT_KEY_J, INPUT_KEY_K, INPUT_KEY_L,
    INPUT_KEY_M, INPUT_KEY_N, INPUT_KEY_O, INPUT_KEY_P, INPUT_KEY_Q, INPUT_KEY_R,
    INPUT_KEY_S, INPUT_KEY_T, INPUT_KEY_U, INPUT_KEY_V, INPUT_KEY_W, INPUT_KEY_X,
    INPUT_KEY_Y, INPUT_KEY_Z,

    INPUT_KEY_0, INPUT_KEY_1, INPUT_KEY_2, INPUT_KEY_3, INPUT_KEY_4,
    INPUT_KEY_5, INPUT_KEY_6, INPUT_KEY_7, INPUT_KEY_8, INPUT_KEY_9,

    INPUT_KEY_F1, INPUT_KEY_F2, INPUT_KEY_F3, INPUT_KEY_F4, INPUT_KEY_F5, INPUT_KEY_F6,
    INPUT_KEY_F7, INPUT_KEY_F8, INPUT_KEY_F9, INPUT_KEY_F10, INPUT_KEY_F11, INPUT_KEY_F12,

    INPUT_KEY_ESCAPE, INPUT_KEY_TAB, INPUT_KEY_CAPS_LOCK, INPUT_KEY_ENTER, INPUT_KEY_BACKSPACE,
    INPUT_KEY_SPACE,

    INPUT_KEY_LEFT_SHIFT, INPUT_KEY_RIGHT_SHIFT,
    INPUT_KEY_LEFT_CONTROL, INPUT_KEY_RIGHT_CONTROL,
    INPUT_KEY_LEFT_ALT, INPUT_KEY_RIGHT_ALT,
    INPUT_KEY_LEFT_SUPER, INPUT_KEY_RIGHT_SUPER,

    INPUT_KEY_ARROW_UP, INPUT_KEY_ARROW_DOWN, INPUT_KEY_ARROW_LEFT, INPUT_KEY_ARROW_RIGHT,

    INPUT_KEY_INSERT, INPUT_KEY_DELETE, INPUT_KEY_HOME, INPUT_KEY_END,
    INPUT_KEY_PAGE_UP, INPUT_KEY_PAGE_DOWN,

    INPUT_KEY_MINUS, INPUT_KEY_EQUALS, INPUT_KEY_LEFT_BRACKET, INPUT_KEY_RIGHT_BRACKET,
    INPUT_KEY_SEMICOLON, INPUT_KEY_APOSTROPHE, INPUT_KEY_GRAVE, INPUT_KEY_BACKSLASH,
    INPUT_KEY_COMMA, INPUT_KEY_PERIOD, INPUT_KEY_SLASH,

    INPUT_KEY_COUNT,
} input_key;

typedef enum input_mouse_button
{
    INPUT_MOUSE_BUTTON_LEFT,
    INPUT_MOUSE_BUTTON_RIGHT,
    INPUT_MOUSE_BUTTON_MIDDLE,
    INPUT_MOUSE_BUTTON_X1,
    INPUT_MOUSE_BUTTON_X2,

    INPUT_MOUSE_BUTTON_COUNT,
} input_mouse_button;

typedef struct input_event_key
{
    input_key key;
    b8        isDown   : 1;
    b8        isRepeat : 1;
} input_event_key;

typedef struct input_event_mouse_move
{
    /* Client-area position, not a delta - true unclamped look/aim deltas need raw input,
     * which this first pass doesn't cover yet.
     */
    s32 x;
    s32 y;
} input_event_mouse_move;

typedef struct input_event_mouse_button
{
    input_mouse_button button;
    b8                 isDown : 1;
} input_event_mouse_button;

typedef struct input_event_mouse_wheel
{
    s32 delta;
} input_event_mouse_wheel;

/* Up to INPUT_GAMEPAD_COUNT local controllers - matches typical console local-multiplayer limits,
 * not anything specific to one platform's controller API.
 */
#define INPUT_GAMEPAD_COUNT 4

typedef enum input_gamepad_button
{
    INPUT_GAMEPAD_BUTTON_DPAD_UP,
    INPUT_GAMEPAD_BUTTON_DPAD_DOWN,
    INPUT_GAMEPAD_BUTTON_DPAD_LEFT,
    INPUT_GAMEPAD_BUTTON_DPAD_RIGHT,
    INPUT_GAMEPAD_BUTTON_START,
    INPUT_GAMEPAD_BUTTON_BACK,
    INPUT_GAMEPAD_BUTTON_LEFT_THUMB,
    INPUT_GAMEPAD_BUTTON_RIGHT_THUMB,
    INPUT_GAMEPAD_BUTTON_LEFT_SHOULDER,
    INPUT_GAMEPAD_BUTTON_RIGHT_SHOULDER,

    /* Named by physical position on the face-button diamond, not by an Xbox-specific "A/B/X/Y"
     * label - the same physical position is a different letter/symbol on a PlayStation or
     * Switch pad, so positional naming is the one that's actually platform-independent.
     */
    INPUT_GAMEPAD_BUTTON_SOUTH,
    INPUT_GAMEPAD_BUTTON_EAST,
    INPUT_GAMEPAD_BUTTON_WEST,
    INPUT_GAMEPAD_BUTTON_NORTH,

    INPUT_GAMEPAD_BUTTON_COUNT,
} input_gamepad_button;

typedef enum input_gamepad_axis
{
    INPUT_GAMEPAD_AXIS_LEFT_STICK_X,
    INPUT_GAMEPAD_AXIS_LEFT_STICK_Y,
    INPUT_GAMEPAD_AXIS_RIGHT_STICK_X,
    INPUT_GAMEPAD_AXIS_RIGHT_STICK_Y,
    INPUT_GAMEPAD_AXIS_LEFT_TRIGGER,
    INPUT_GAMEPAD_AXIS_RIGHT_TRIGGER,

    INPUT_GAMEPAD_AXIS_COUNT,
} input_gamepad_axis;

typedef struct input_event_gamepad_button
{
    u8                    gamepadIndex;
    input_gamepad_button  button;
    b8                    isDown : 1;
} input_event_gamepad_button;

typedef struct input_event_gamepad_axis
{
    u8                  gamepadIndex;
    input_gamepad_axis  axis;

    /* Normalized and deadzone-filtered by the platform kernel: [-1, 1] for sticks, [0, 1] for
     * triggers. Never the platform's raw integer range - that's exactly the kind of platform
     * knowledge this service exists to hide.
     */
    f32 value;
} input_event_gamepad_axis;

typedef struct input_event_gamepad_connection
{
    u8 gamepadIndex;
    b8 isConnected : 1;
} input_event_gamepad_connection;

typedef struct input_event
{
    input_event_kind kind;

    union
    {
        input_event_key                 key;
        input_event_mouse_move          mouseMove;
        input_event_mouse_button        mouseButton;
        input_event_mouse_wheel         mouseWheel;
        input_event_gamepad_button      gamepadButton;
        input_event_gamepad_axis        gamepadAxis;
        input_event_gamepad_connection  gamepadConnection;
    };
} input_event;

#define INPUT_QUEUE_CAPACITY 256

typedef struct input_queue
{
    input_event events[INPUT_QUEUE_CAPACITY];
    u32         count;
} input_queue;

/* No-ops if the queue is already at INPUT_QUEUE_CAPACITY - dropping an event silently is a
 * safer default than crashing on an input burst under real play, but the capacity is generous
 * enough that this should never actually happen from legitimate input in a single tick.
 */
void input_queue_push (input_queue *queue, input_event event);
void input_queue_clear (input_queue *queue);
