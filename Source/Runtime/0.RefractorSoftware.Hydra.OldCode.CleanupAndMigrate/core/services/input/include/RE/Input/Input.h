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
    physical-key enum (ReInputKey), not a virtual-key code or scan code - the platform kernel is
    responsible for translating whatever its native input APIs give it into these events.

    The queue itself is a fixed-capacity array, not a growable/allocated container - the platform kernel
    owns an instance of it, clears it once per tick, and fills it as events arrive.
*/

#include <RE/Foundation/FoundationPrimitiveTypes.h>

typedef enum ReInputEventKind
{
    ReInputEventKind_Key,
    ReInputEventKind_MouseMove,
    ReInputEventKind_MouseButton,
    ReInputEventKind_MouseWheel,
    ReInputEventKind_GamepadButton,
    ReInputEventKind_GamepadAxis,
    ReInputEventKind_GamepadConnection,
} ReInputEventKind;

/* Physical key identity (scan-code-based on platforms where that distinction exists), not a
 * layout-dependent virtual key - WASD should mean the same physical keys on any keyboard layout.
 */
typedef enum ReInputKey
{
    ReInputKey_Unknown = 0,

    ReInputKey_A, ReInputKey_B, ReInputKey_C, ReInputKey_D, ReInputKey_E, ReInputKey_F,
    ReInputKey_G, ReInputKey_H, ReInputKey_I, ReInputKey_J, ReInputKey_K, ReInputKey_L,
    ReInputKey_M, ReInputKey_N, ReInputKey_O, ReInputKey_P, ReInputKey_Q, ReInputKey_R,
    ReInputKey_S, ReInputKey_T, ReInputKey_U, ReInputKey_V, ReInputKey_W, ReInputKey_X,
    ReInputKey_Y, ReInputKey_Z,

    ReInputKey_0, ReInputKey_1, ReInputKey_2, ReInputKey_3, ReInputKey_4,
    ReInputKey_5, ReInputKey_6, ReInputKey_7, ReInputKey_8, ReInputKey_9,

    ReInputKey_F1, ReInputKey_F2, ReInputKey_F3, ReInputKey_F4, ReInputKey_F5, ReInputKey_F6,
    ReInputKey_F7, ReInputKey_F8, ReInputKey_F9, ReInputKey_F10, ReInputKey_F11, ReInputKey_F12,

    ReInputKey_Escape, ReInputKey_Tab, ReInputKey_CapsLock, ReInputKey_Enter, ReInputKey_Backspace,
    ReInputKey_Space,

    ReInputKey_LeftShift, ReInputKey_RightShift,
    ReInputKey_LeftControl, ReInputKey_RightControl,
    ReInputKey_LeftAlt, ReInputKey_RightAlt,
    ReInputKey_LeftSuper, ReInputKey_RightSuper,

    ReInputKey_ArrowUp, ReInputKey_ArrowDown, ReInputKey_ArrowLeft, ReInputKey_ArrowRight,

    ReInputKey_Insert, ReInputKey_Delete, ReInputKey_Home, ReInputKey_End,
    ReInputKey_PageUp, ReInputKey_PageDown,

    ReInputKey_Minus, ReInputKey_Equals, ReInputKey_LeftBracket, ReInputKey_RightBracket,
    ReInputKey_Semicolon, ReInputKey_Apostrophe, ReInputKey_Grave, ReInputKey_Backslash,
    ReInputKey_Comma, ReInputKey_Period, ReInputKey_Slash,

    ReInputKey_Count,
} ReInputKey;

typedef enum ReInputMouseButton
{
    ReInputMouseButton_Left,
    ReInputMouseButton_Right,
    ReInputMouseButton_Middle,
    ReInputMouseButton_X1,
    ReInputMouseButton_X2,

    ReInputMouseButton_Count,
} ReInputMouseButton;

typedef struct ReInputEventKey
{
    ReInputKey key;
    ReBool        isDown   : 1;
    ReBool        isRepeat : 1;
} ReInputEventKey;

typedef struct ReInputEventMouseMove
{
    /* Client-area position, not a delta - true unclamped look/aim deltas need raw input,
     * which this first pass doesn't cover yet.
     */
    ReSint32 x;
    ReSint32 y;
} ReInputEventMouseMove;

typedef struct ReInputEventMouseButton
{
    ReInputMouseButton button;
    ReBool                 isDown : 1;
} ReInputEventMouseButton;

typedef struct ReInputEventMouseWheel
{
    ReSint32 delta;
} ReInputEventMouseWheel;

/* Up to RE_INPUT_GAMEPAD_COUNT local controllers - matches typical console local-multiplayer limits,
 * not anything specific to one platform's controller API.
 */
#define RE_INPUT_GAMEPAD_COUNT 4

typedef enum ReInputGamepadButton
{
    ReInputGamepadButton_DpadUp,
    ReInputGamepadButton_DpadDown,
    ReInputGamepadButton_DpadLeft,
    ReInputGamepadButton_DpadRight,
    ReInputGamepadButton_Start,
    ReInputGamepadButton_Back,
    ReInputGamepadButton_LeftThumb,
    ReInputGamepadButton_RightThumb,
    ReInputGamepadButton_LeftShoulder,
    ReInputGamepadButton_RightShoulder,

    /* Named by physical position on the face-button diamond, not by an Xbox-specific "A/B/X/Y"
     * label - the same physical position is a different letter/symbol on a PlayStation or
     * Switch pad, so positional naming is the one that's actually platform-independent.
     */
    ReInputGamepadButton_South,
    ReInputGamepadButton_East,
    ReInputGamepadButton_West,
    ReInputGamepadButton_North,

    ReInputGamepadButton_Count,
} ReInputGamepadButton;

typedef enum ReInputGamepadAxis
{
    ReInputGamepadAxis_LeftStickX,
    ReInputGamepadAxis_LeftStickY,
    ReInputGamepadAxis_RightStickX,
    ReInputGamepadAxis_RightStickY,
    ReInputGamepadAxis_LeftTrigger,
    ReInputGamepadAxis_RightTrigger,

    ReInputGamepadAxis_Count,
} ReInputGamepadAxis;

typedef struct ReInputEventGamepadButton
{
    ReUint8                    gamepadIndex;
    ReInputGamepadButton  button;
    ReBool                    isDown : 1;
} ReInputEventGamepadButton;

typedef struct ReInputEventGamepadAxis
{
    ReUint8                  gamepadIndex;
    ReInputGamepadAxis  axis;

    /* Normalized and deadzone-filtered by the platform kernel: [-1, 1] for sticks, [0, 1] for
     * triggers. Never the platform's raw integer range - that's exactly the kind of platform
     * knowledge this service exists to hide.
     */
    ReFloat32 value;
} ReInputEventGamepadAxis;

typedef struct ReInputEventGamepadConnection
{
    ReUint8 gamepadIndex;
    ReBool isConnected : 1;
} ReInputEventGamepadConnection;

typedef struct ReInputEvent
{
    ReInputEventKind kind;

    union
    {
        ReInputEventKey                 key;
        ReInputEventMouseMove          mouseMove;
        ReInputEventMouseButton        mouseButton;
        ReInputEventMouseWheel         mouseWheel;
        ReInputEventGamepadButton      gamepadButton;
        ReInputEventGamepadAxis        gamepadAxis;
        ReInputEventGamepadConnection  gamepadConnection;
    };
} ReInputEvent;

#define RE_INPUT_QUEUE_CAPACITY 256

typedef struct ReInputQueue
{
    ReInputEvent events[RE_INPUT_QUEUE_CAPACITY];
    ReUint32         count;
} ReInputQueue;

/* No-ops if the queue is already at RE_INPUT_QUEUE_CAPACITY - dropping an event silently is a
 * safer default than crashing on an input burst under real play, but the capacity is generous
 * enough that this should never actually happen from legitimate input in a single tick.
 */
void RE_Input_PushEvent( ReInputQueue *queue, ReInputEvent event );
void RE_Input_ClearQueue( ReInputQueue *queue );
