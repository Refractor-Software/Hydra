/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64Input.h"

RE_GLOBAL ReInputQueue gWin64InputQueue;
RE_GLOBAL ReBool          gWin64KeyHeld[ReInputKey_Count];

/* Set 1 scan codes, indexed directly - covers everything that isn't ambiguous between its
 * extended and non-extended form. Ambiguous codes (ctrl, alt, the nav/numpad cluster, the
 * super keys) are deliberately left ReInputKey_Unknown here and resolved in
 * Win64_Input_KeyFromScanCode() instead, since they need the extended bit to disambiguate.
 */
RE_GLOBAL const ReInputKey gWin64ScanCodeToKey[256] =
{
    [0x01] = ReInputKey_Escape,
    [0x02] = ReInputKey_1, [0x03] = ReInputKey_2, [0x04] = ReInputKey_3, [0x05] = ReInputKey_4,
    [0x06] = ReInputKey_5, [0x07] = ReInputKey_6, [0x08] = ReInputKey_7, [0x09] = ReInputKey_8,
    [0x0A] = ReInputKey_9, [0x0B] = ReInputKey_0,
    [0x0C] = ReInputKey_Minus, [0x0D] = ReInputKey_Equals,
    [0x0E] = ReInputKey_Backspace,
    [0x0F] = ReInputKey_Tab,
    [0x10] = ReInputKey_Q, [0x11] = ReInputKey_W, [0x12] = ReInputKey_E, [0x13] = ReInputKey_R,
    [0x14] = ReInputKey_T, [0x15] = ReInputKey_Y, [0x16] = ReInputKey_U, [0x17] = ReInputKey_I,
    [0x18] = ReInputKey_O, [0x19] = ReInputKey_P,
    [0x1A] = ReInputKey_LeftBracket, [0x1B] = ReInputKey_RightBracket,
    [0x1C] = ReInputKey_Enter,
    /* [0x1D] left/right control - ambiguous, resolved via extended bit */
    [0x1E] = ReInputKey_A, [0x1F] = ReInputKey_S, [0x20] = ReInputKey_D, [0x21] = ReInputKey_F,
    [0x22] = ReInputKey_G, [0x23] = ReInputKey_H, [0x24] = ReInputKey_J, [0x25] = ReInputKey_K,
    [0x26] = ReInputKey_L,
    [0x27] = ReInputKey_Semicolon, [0x28] = ReInputKey_Apostrophe, [0x29] = ReInputKey_Grave,
    [0x2A] = ReInputKey_LeftShift,
    [0x2B] = ReInputKey_Backslash,
    [0x2C] = ReInputKey_Z, [0x2D] = ReInputKey_X, [0x2E] = ReInputKey_C, [0x2F] = ReInputKey_V,
    [0x30] = ReInputKey_B, [0x31] = ReInputKey_N, [0x32] = ReInputKey_M,
    [0x33] = ReInputKey_Comma, [0x34] = ReInputKey_Period, [0x35] = ReInputKey_Slash,
    [0x36] = ReInputKey_RightShift,
    /* [0x38] left/right alt - ambiguous, resolved via extended bit */
    [0x39] = ReInputKey_Space,
    [0x3A] = ReInputKey_CapsLock,
    [0x3B] = ReInputKey_F1, [0x3C] = ReInputKey_F2, [0x3D] = ReInputKey_F3, [0x3E] = ReInputKey_F4,
    [0x3F] = ReInputKey_F5, [0x40] = ReInputKey_F6, [0x41] = ReInputKey_F7, [0x42] = ReInputKey_F8,
    [0x43] = ReInputKey_F9, [0x44] = ReInputKey_F10,
    /* [0x47 .. 0x53] nav cluster / numpad - ambiguous, resolved via extended bit */
    [0x57] = ReInputKey_F11, [0x58] = ReInputKey_F12,
};

/* Handles the scan codes that mean something different depending on the extended-key bit
 * (lParam bit 24) - control, alt, the nav cluster (arrows/home/end/etc, which share codes with
 * the numpad), and the super keys, which are only ever sent extended.
 */
RE_INTERNAL ReInputKey
Win64_Input_KeyFromScanCode( ReUint32 scanCode, ReBool isExtended )
{
    if ( isExtended )
    {
        switch ( scanCode )
        {
        case 0x1D: return ReInputKey_RightControl;
        case 0x38: return ReInputKey_RightAlt;
        case 0x47: return ReInputKey_Home;
        case 0x48: return ReInputKey_ArrowUp;
        case 0x49: return ReInputKey_PageUp;
        case 0x4B: return ReInputKey_ArrowLeft;
        case 0x4D: return ReInputKey_ArrowRight;
        case 0x4F: return ReInputKey_End;
        case 0x50: return ReInputKey_ArrowDown;
        case 0x51: return ReInputKey_PageDown;
        case 0x52: return ReInputKey_Insert;
        case 0x53: return ReInputKey_Delete;
        case 0x5B: return ReInputKey_LeftSuper;
        case 0x5C: return ReInputKey_RightSuper;
        default: break;
        }
    }
    else
    {
        switch ( scanCode )
        {
        case 0x1D: return ReInputKey_LeftControl;
        case 0x38: return ReInputKey_LeftAlt;
        default: break;
        }
    }

    if ( scanCode >= 256 )
    {
        return ReInputKey_Unknown;
    }

    return gWin64ScanCodeToKey[scanCode];
}

RE_INTERNAL void
Win64_Input_PushKeyEvent( ReInputKey key, ReBool isDown, ReBool isRepeat )
{
    ReInputEvent event;
    event.kind             = ReInputEventKind_Key;
    event.key.key           = key;
    event.key.isDown        = isDown;
    event.key.isRepeat      = isRepeat;

    RE_Input_PushEvent( &gWin64InputQueue, event );

    if ( key != ReInputKey_Unknown )
    {
        gWin64KeyHeld[key] = isDown;
    }
}

RE_INTERNAL void
Win64_Input_PushMouseButtonEvent( ReInputMouseButton button, ReBool isDown )
{
    ReInputEvent event;
    event.kind                  = ReInputEventKind_MouseButton;
    event.mouseButton.button    = button;
    event.mouseButton.isDown    = isDown;

    RE_Input_PushEvent( &gWin64InputQueue, event );
}

void
Win64_Input_Reset( void )
{
    RE_Input_ClearQueue( &gWin64InputQueue );
}

void
Win64_Input_HandleMessage( UINT message, WPARAM wParam, LPARAM lParam )
{
    switch ( message )
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        ReUint32 scanCode   = (ReUint32) ((lParam >> 16) & 0xFF);
        ReBool  isExtended = (ReBool) ((lParam >> 24) & 0x1);
        ReBool  wasDown    = (ReBool) ((lParam >> 30) & 0x1);
        ReBool  isUp       = (ReBool) ((lParam >> 31) & 0x1);

        ReInputKey key = Win64_Input_KeyFromScanCode( scanCode, isExtended );

        Win64_Input_PushKeyEvent( key, (ReBool) !isUp, (ReBool) (wasDown && !isUp) );
        break;
    }

    case WM_MOUSEMOVE:
    {
        ReInputEvent event;
        event.kind         = ReInputEventKind_MouseMove;
        event.mouseMove.x  = (ReSint32) (short) LOWORD( lParam );
        event.mouseMove.y  = (ReSint32) (short) HIWORD( lParam );

        RE_Input_PushEvent( &gWin64InputQueue, event );
        break;
    }

    case WM_LBUTTONDOWN: Win64_Input_PushMouseButtonEvent( ReInputMouseButton_Left, RE_True );    break;
    case WM_LBUTTONUP:   Win64_Input_PushMouseButtonEvent( ReInputMouseButton_Left, RE_False );   break;
    case WM_RBUTTONDOWN: Win64_Input_PushMouseButtonEvent( ReInputMouseButton_Right, RE_True );   break;
    case WM_RBUTTONUP:   Win64_Input_PushMouseButtonEvent( ReInputMouseButton_Right, RE_False );  break;
    case WM_MBUTTONDOWN: Win64_Input_PushMouseButtonEvent( ReInputMouseButton_Middle, RE_True );  break;
    case WM_MBUTTONUP:   Win64_Input_PushMouseButtonEvent( ReInputMouseButton_Middle, RE_False ); break;

    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    {
        ReInputMouseButton button = (GET_XBUTTON_WPARAM( wParam ) == XBUTTON1)
            ? ReInputMouseButton_X1
            : ReInputMouseButton_X2;

        Win64_Input_PushMouseButtonEvent( button, (ReBool) (message == WM_XBUTTONDOWN) );
        break;
    }

    case WM_MOUSEWHEEL:
    {
        ReInputEvent event;
        event.kind             = ReInputEventKind_MouseWheel;
        event.mouseWheel.delta = GET_WHEEL_DELTA_WPARAM( wParam );

        RE_Input_PushEvent( &gWin64InputQueue, event );
        break;
    }

    default:
        break;
    }
}

void
Win64_Input_ReleaseAllHeldKeys( void )
{
    for ( ReUint32 i = 0; i < ReInputKey_Count; i += 1 )
    {
        if ( gWin64KeyHeld[i] )
        {
            Win64_Input_PushKeyEvent( (ReInputKey) i, RE_False, RE_False );
        }
    }
}

ReInputQueue *
Win64_Input_GetQueue( void )
{
    return &gWin64InputQueue;
}
