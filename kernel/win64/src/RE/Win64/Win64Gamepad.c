/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64Gamepad.h"

#include <xinput.h>

typedef struct ReWin64GamepadState
{
    ReBool  isConnected : 1;
    ReUint16 buttons;
    ReUint8  leftTrigger;
    ReUint8  rightTrigger;
    ReSint16 leftStickX;
    ReSint16 leftStickY;
    ReSint16 rightStickX;
    ReSint16 rightStickY;
} ReWin64GamepadState;

global ReWin64GamepadState gWin64GamepadState[RE_INPUT_GAMEPAD_COUNT];

typedef struct ReWin64GamepadButtonMapping
{
    ReUint16                   xinputButton;
    ReInputGamepadButton  key;
} ReWin64GamepadButtonMapping;

global const ReWin64GamepadButtonMapping gWin64GamepadButtonMap[] =
{
    { XINPUT_GAMEPAD_DPAD_UP,        ReInputGamepadButton_DpadUp },
    { XINPUT_GAMEPAD_DPAD_DOWN,      ReInputGamepadButton_DpadDown },
    { XINPUT_GAMEPAD_DPAD_LEFT,      ReInputGamepadButton_DpadLeft },
    { XINPUT_GAMEPAD_DPAD_RIGHT,     ReInputGamepadButton_DpadRight },
    { XINPUT_GAMEPAD_START,          ReInputGamepadButton_Start },
    { XINPUT_GAMEPAD_BACK,           ReInputGamepadButton_Back },
    { XINPUT_GAMEPAD_LEFT_THUMB,     ReInputGamepadButton_LeftThumb },
    { XINPUT_GAMEPAD_RIGHT_THUMB,    ReInputGamepadButton_RightThumb },
    { XINPUT_GAMEPAD_LEFT_SHOULDER,  ReInputGamepadButton_LeftShoulder },
    { XINPUT_GAMEPAD_RIGHT_SHOULDER, ReInputGamepadButton_RightShoulder },
    { XINPUT_GAMEPAD_A,              ReInputGamepadButton_South },
    { XINPUT_GAMEPAD_B,              ReInputGamepadButton_East },
    { XINPUT_GAMEPAD_X,              ReInputGamepadButton_West },
    { XINPUT_GAMEPAD_Y,              ReInputGamepadButton_North },
};

#define WIN64_GAMEPAD_BUTTON_MAP_COUNT ( sizeof( gWin64GamepadButtonMap ) / sizeof( gWin64GamepadButtonMap[0] ) )

internal ReFloat32
Win64_Gamepad_NormalizeStickAxis( ReSint16 value, ReSint16 deadzone )
{
    ReSint32 magnitude = (value < 0) ? -(ReSint32) value : (ReSint32) value;

    if ( magnitude < (ReSint32) deadzone )
    {
        return 0.0f;
    }

    ReFloat32 normalized = (ReFloat32) value / 32768.0f;

    if ( normalized > 1.0f )  { normalized = 1.0f; }
    if ( normalized < -1.0f ) { normalized = -1.0f; }

    return normalized;
}

internal ReFloat32
Win64_Gamepad_NormalizeTriggerAxis( ReUint8 value, ReUint8 threshold )
{
    if ( value < threshold )
    {
        return 0.0f;
    }

    return( ReFloat32 ) value / 255.0f;
}

internal void
Win64_Gamepad_PushButtonEvent( ReInputQueue *queue, ReUint8 gamepadIndex, ReInputGamepadButton button, ReBool isDown )
{
    ReInputEvent event;
    event.kind                       = ReInputEventKind_GamepadButton;
    event.gamepadButton.gamepadIndex = gamepadIndex;
    event.gamepadButton.button       = button;
    event.gamepadButton.isDown       = isDown;

    RE_Input_PushEvent( queue, event );
}

internal void
Win64_Gamepad_PushAxisIfChanged( ReInputQueue *queue, ReUint8 gamepadIndex, ReInputGamepadAxis axis,
    ReFloat32 previous, ReFloat32 current )
{
    if ( previous == current )
    {
        return;
    }

    ReInputEvent event;
    event.kind                     = ReInputEventKind_GamepadAxis;
    event.gamepadAxis.gamepadIndex = gamepadIndex;
    event.gamepadAxis.axis         = axis;
    event.gamepadAxis.value        = current;

    RE_Input_PushEvent( queue, event );
}

internal void
Win64_Gamepad_HandleDisconnect( ReInputQueue *queue, ReUint8 gamepadIndex, ReWin64GamepadState *state )
{
    for ( ReUint32 buttonIndex = 0; buttonIndex < WIN64_GAMEPAD_BUTTON_MAP_COUNT; buttonIndex += 1 )
    {
        if ( state->buttons & gWin64GamepadButtonMap[buttonIndex].xinputButton )
        {
            Win64_Gamepad_PushButtonEvent( queue, gamepadIndex,
                gWin64GamepadButtonMap[buttonIndex].key, RE_False );
        }
    }
}

void
Win64_Gamepad_Poll( ReInputQueue *queue )
{
    for ( ReUint8 gamepadIndex = 0; gamepadIndex < RE_INPUT_GAMEPAD_COUNT; gamepadIndex += 1 )
    {
        ReWin64GamepadState *state = &gWin64GamepadState[gamepadIndex];

        XINPUT_STATE xinputState;
        ReBool isConnected = (ReBool) (XInputGetState( gamepadIndex, &xinputState ) == ERROR_SUCCESS);

        if ( isConnected != state->isConnected )
        {
            ReInputEvent event;
            event.kind                          = ReInputEventKind_GamepadConnection;
            event.gamepadConnection.gamepadIndex = gamepadIndex;
            event.gamepadConnection.isConnected  = isConnected;

            RE_Input_PushEvent( queue, event );

            if ( !isConnected )
            {
                Win64_Gamepad_HandleDisconnect( queue, gamepadIndex, state );
            }

            ReWin64GamepadState resetState = {0};
            resetState.isConnected = isConnected;
            *state = resetState;
        }

        if ( !isConnected )
        {
            continue;
        }

        ReUint16 newButtons = xinputState.Gamepad.wButtons;

        if ( newButtons != state->buttons )
        {
            for ( ReUint32 buttonIndex = 0; buttonIndex < WIN64_GAMEPAD_BUTTON_MAP_COUNT; buttonIndex += 1 )
            {
                ReUint16 mask = gWin64GamepadButtonMap[buttonIndex].xinputButton;

                ReBool wasDown = (ReBool) ((state->buttons & mask) != 0);
                ReBool isDown  = (ReBool) ((newButtons & mask) != 0);

                if ( wasDown != isDown )
                {
                    Win64_Gamepad_PushButtonEvent( queue, gamepadIndex,
                        gWin64GamepadButtonMap[buttonIndex].key, isDown );
                }
            }

            state->buttons = newButtons;
        }

        ReFloat32 previousLeftX = Win64_Gamepad_NormalizeStickAxis(
            state->leftStickX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
        ReFloat32 previousLeftY = Win64_Gamepad_NormalizeStickAxis(
            state->leftStickY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
        ReFloat32 previousRightX = Win64_Gamepad_NormalizeStickAxis(
            state->rightStickX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
        ReFloat32 previousRightY = Win64_Gamepad_NormalizeStickAxis(
            state->rightStickY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
        ReFloat32 previousLeftTrigger = Win64_Gamepad_NormalizeTriggerAxis(
            state->leftTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD );
        ReFloat32 previousRightTrigger = Win64_Gamepad_NormalizeTriggerAxis(
            state->rightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD );

        ReFloat32 currentLeftX = Win64_Gamepad_NormalizeStickAxis(
            xinputState.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
        ReFloat32 currentLeftY = Win64_Gamepad_NormalizeStickAxis(
            xinputState.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
        ReFloat32 currentRightX = Win64_Gamepad_NormalizeStickAxis(
            xinputState.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
        ReFloat32 currentRightY = Win64_Gamepad_NormalizeStickAxis(
            xinputState.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
        ReFloat32 currentLeftTrigger = Win64_Gamepad_NormalizeTriggerAxis(
            xinputState.Gamepad.bLeftTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD );
        ReFloat32 currentRightTrigger = Win64_Gamepad_NormalizeTriggerAxis(
            xinputState.Gamepad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD );

        Win64_Gamepad_PushAxisIfChanged( queue, gamepadIndex, ReInputGamepadAxis_LeftStickX,
            previousLeftX, currentLeftX );
        Win64_Gamepad_PushAxisIfChanged( queue, gamepadIndex, ReInputGamepadAxis_LeftStickY,
            previousLeftY, currentLeftY );
        Win64_Gamepad_PushAxisIfChanged( queue, gamepadIndex, ReInputGamepadAxis_RightStickX,
            previousRightX, currentRightX );
        Win64_Gamepad_PushAxisIfChanged( queue, gamepadIndex, ReInputGamepadAxis_RightStickY,
            previousRightY, currentRightY );
        Win64_Gamepad_PushAxisIfChanged( queue, gamepadIndex, ReInputGamepadAxis_LeftTrigger,
            previousLeftTrigger, currentLeftTrigger );
        Win64_Gamepad_PushAxisIfChanged( queue, gamepadIndex, ReInputGamepadAxis_RightTrigger,
            previousRightTrigger, currentRightTrigger );

        state->leftStickX   = xinputState.Gamepad.sThumbLX;
        state->leftStickY   = xinputState.Gamepad.sThumbLY;
        state->rightStickX  = xinputState.Gamepad.sThumbRX;
        state->rightStickY  = xinputState.Gamepad.sThumbRY;
        state->leftTrigger  = xinputState.Gamepad.bLeftTrigger;
        state->rightTrigger = xinputState.Gamepad.bRightTrigger;
    }
}
