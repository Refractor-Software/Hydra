/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "win64/gamepad/win64_gamepad.h"

#include <xinput.h>

typedef struct win64_gamepad_state
{
    b8  isConnected : 1;
    u16 buttons;
    u8  leftTrigger;
    u8  rightTrigger;
    s16 leftStickX;
    s16 leftStickY;
    s16 rightStickX;
    s16 rightStickY;
} win64_gamepad_state;

static win64_gamepad_state gWin64GamepadState[INPUT_GAMEPAD_COUNT];

typedef struct win64_gamepad_button_mapping
{
    u16                   xinputButton;
    input_gamepad_button  key;
} win64_gamepad_button_mapping;

static const win64_gamepad_button_mapping gWin64GamepadButtonMap[] =
{
    { XINPUT_GAMEPAD_DPAD_UP,        INPUT_GAMEPAD_BUTTON_DPAD_UP },
    { XINPUT_GAMEPAD_DPAD_DOWN,      INPUT_GAMEPAD_BUTTON_DPAD_DOWN },
    { XINPUT_GAMEPAD_DPAD_LEFT,      INPUT_GAMEPAD_BUTTON_DPAD_LEFT },
    { XINPUT_GAMEPAD_DPAD_RIGHT,     INPUT_GAMEPAD_BUTTON_DPAD_RIGHT },
    { XINPUT_GAMEPAD_START,          INPUT_GAMEPAD_BUTTON_START },
    { XINPUT_GAMEPAD_BACK,           INPUT_GAMEPAD_BUTTON_BACK },
    { XINPUT_GAMEPAD_LEFT_THUMB,     INPUT_GAMEPAD_BUTTON_LEFT_THUMB },
    { XINPUT_GAMEPAD_RIGHT_THUMB,    INPUT_GAMEPAD_BUTTON_RIGHT_THUMB },
    { XINPUT_GAMEPAD_LEFT_SHOULDER,  INPUT_GAMEPAD_BUTTON_LEFT_SHOULDER },
    { XINPUT_GAMEPAD_RIGHT_SHOULDER, INPUT_GAMEPAD_BUTTON_RIGHT_SHOULDER },
    { XINPUT_GAMEPAD_A,              INPUT_GAMEPAD_BUTTON_SOUTH },
    { XINPUT_GAMEPAD_B,              INPUT_GAMEPAD_BUTTON_EAST },
    { XINPUT_GAMEPAD_X,              INPUT_GAMEPAD_BUTTON_WEST },
    { XINPUT_GAMEPAD_Y,              INPUT_GAMEPAD_BUTTON_NORTH },
};

#define WIN64_GAMEPAD_BUTTON_MAP_COUNT (sizeof (gWin64GamepadButtonMap) / sizeof (gWin64GamepadButtonMap[0]))

static f32
win64_gamepad_normalize_stick_axis (s16 value, s16 deadzone)
{
    s32 magnitude = (value < 0) ? -(s32) value : (s32) value;

    if (magnitude < (s32) deadzone)
    {
        return 0.0f;
    }

    f32 normalized = (f32) value / 32768.0f;

    if (normalized > 1.0f)  { normalized = 1.0f; }
    if (normalized < -1.0f) { normalized = -1.0f; }

    return normalized;
}

static f32
win64_gamepad_normalize_trigger_axis (u8 value, u8 threshold)
{
    if (value < threshold)
    {
        return 0.0f;
    }

    return (f32) value / 255.0f;
}

static void
win64_gamepad_push_button_event (input_queue *queue, u8 gamepadIndex, input_gamepad_button button, b8 isDown)
{
    input_event event;
    event.kind                       = INPUT_EVENT_GAMEPAD_BUTTON;
    event.gamepadButton.gamepadIndex = gamepadIndex;
    event.gamepadButton.button       = button;
    event.gamepadButton.isDown       = isDown;

    input_queue_push (queue, event);
}

static void
win64_gamepad_push_axis_if_changed (input_queue *queue, u8 gamepadIndex, input_gamepad_axis axis, f32 previous, f32 current)
{
    if (previous == current)
    {
        return;
    }

    input_event event;
    event.kind                     = INPUT_EVENT_GAMEPAD_AXIS;
    event.gamepadAxis.gamepadIndex = gamepadIndex;
    event.gamepadAxis.axis         = axis;
    event.gamepadAxis.value        = current;

    input_queue_push (queue, event);
}

static void
win64_gamepad_handle_disconnect (input_queue *queue, u8 gamepadIndex, win64_gamepad_state *state)
{
    for (u32 buttonIndex = 0; buttonIndex < WIN64_GAMEPAD_BUTTON_MAP_COUNT; buttonIndex += 1)
    {
        if (state->buttons & gWin64GamepadButtonMap[buttonIndex].xinputButton)
        {
            win64_gamepad_push_button_event (queue, gamepadIndex, gWin64GamepadButtonMap[buttonIndex].key, FALSE);
        }
    }
}

void
win64_gamepad_poll (input_queue *queue)
{
    for (u8 gamepadIndex = 0; gamepadIndex < INPUT_GAMEPAD_COUNT; gamepadIndex += 1)
    {
        win64_gamepad_state *state = &gWin64GamepadState[gamepadIndex];

        XINPUT_STATE xinputState;
        b8 isConnected = (b8) (XInputGetState (gamepadIndex, &xinputState) == ERROR_SUCCESS);

        if (isConnected != state->isConnected)
        {
            input_event event;
            event.kind                          = INPUT_EVENT_GAMEPAD_CONNECTION;
            event.gamepadConnection.gamepadIndex = gamepadIndex;
            event.gamepadConnection.isConnected  = isConnected;

            input_queue_push (queue, event);

            if (!isConnected)
            {
                win64_gamepad_handle_disconnect (queue, gamepadIndex, state);
            }

            win64_gamepad_state resetState = {0};
            resetState.isConnected = isConnected;
            *state = resetState;
        }

        if (!isConnected)
        {
            continue;
        }

        u16 newButtons = xinputState.Gamepad.wButtons;

        if (newButtons != state->buttons)
        {
            for (u32 buttonIndex = 0; buttonIndex < WIN64_GAMEPAD_BUTTON_MAP_COUNT; buttonIndex += 1)
            {
                u16 mask = gWin64GamepadButtonMap[buttonIndex].xinputButton;

                b8 wasDown = (b8) ((state->buttons & mask) != 0);
                b8 isDown  = (b8) ((newButtons & mask) != 0);

                if (wasDown != isDown)
                {
                    win64_gamepad_push_button_event (queue, gamepadIndex, gWin64GamepadButtonMap[buttonIndex].key, isDown);
                }
            }

            state->buttons = newButtons;
        }

        f32 previousLeftX       = win64_gamepad_normalize_stick_axis (state->leftStickX,  XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        f32 previousLeftY       = win64_gamepad_normalize_stick_axis (state->leftStickY,  XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        f32 previousRightX      = win64_gamepad_normalize_stick_axis (state->rightStickX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        f32 previousRightY      = win64_gamepad_normalize_stick_axis (state->rightStickY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        f32 previousLeftTrigger  = win64_gamepad_normalize_trigger_axis (state->leftTrigger,  XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
        f32 previousRightTrigger = win64_gamepad_normalize_trigger_axis (state->rightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);

        f32 currentLeftX       = win64_gamepad_normalize_stick_axis (xinputState.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        f32 currentLeftY       = win64_gamepad_normalize_stick_axis (xinputState.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        f32 currentRightX      = win64_gamepad_normalize_stick_axis (xinputState.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        f32 currentRightY      = win64_gamepad_normalize_stick_axis (xinputState.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        f32 currentLeftTrigger  = win64_gamepad_normalize_trigger_axis (xinputState.Gamepad.bLeftTrigger,  XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
        f32 currentRightTrigger = win64_gamepad_normalize_trigger_axis (xinputState.Gamepad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);

        win64_gamepad_push_axis_if_changed (queue, gamepadIndex, INPUT_GAMEPAD_AXIS_LEFT_STICK_X,   previousLeftX,        currentLeftX);
        win64_gamepad_push_axis_if_changed (queue, gamepadIndex, INPUT_GAMEPAD_AXIS_LEFT_STICK_Y,   previousLeftY,        currentLeftY);
        win64_gamepad_push_axis_if_changed (queue, gamepadIndex, INPUT_GAMEPAD_AXIS_RIGHT_STICK_X,  previousRightX,       currentRightX);
        win64_gamepad_push_axis_if_changed (queue, gamepadIndex, INPUT_GAMEPAD_AXIS_RIGHT_STICK_Y,  previousRightY,       currentRightY);
        win64_gamepad_push_axis_if_changed (queue, gamepadIndex, INPUT_GAMEPAD_AXIS_LEFT_TRIGGER,   previousLeftTrigger,  currentLeftTrigger);
        win64_gamepad_push_axis_if_changed (queue, gamepadIndex, INPUT_GAMEPAD_AXIS_RIGHT_TRIGGER,  previousRightTrigger, currentRightTrigger);

        state->leftStickX   = xinputState.Gamepad.sThumbLX;
        state->leftStickY   = xinputState.Gamepad.sThumbLY;
        state->rightStickX  = xinputState.Gamepad.sThumbRX;
        state->rightStickY  = xinputState.Gamepad.sThumbRY;
        state->leftTrigger  = xinputState.Gamepad.bLeftTrigger;
        state->rightTrigger = xinputState.Gamepad.bRightTrigger;
    }
}
