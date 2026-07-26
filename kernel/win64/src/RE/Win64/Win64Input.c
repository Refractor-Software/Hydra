/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64Input.h"

static input_queue gWin64InputQueue;
static b8          gWin64KeyHeld[INPUT_KEY_COUNT];

/* Set 1 scan codes, indexed directly - covers everything that isn't ambiguous between its
 * extended and non-extended form. Ambiguous codes (ctrl, alt, the nav/numpad cluster, the
 * super keys) are deliberately left INPUT_KEY_UNKNOWN here and resolved in
 * win64_input_key_from_scan_code() instead, since they need the extended bit to disambiguate.
 */
static const input_key gWin64ScanCodeToKey[256] =
{
    [0x01] = INPUT_KEY_ESCAPE,
    [0x02] = INPUT_KEY_1, [0x03] = INPUT_KEY_2, [0x04] = INPUT_KEY_3, [0x05] = INPUT_KEY_4,
    [0x06] = INPUT_KEY_5, [0x07] = INPUT_KEY_6, [0x08] = INPUT_KEY_7, [0x09] = INPUT_KEY_8,
    [0x0A] = INPUT_KEY_9, [0x0B] = INPUT_KEY_0,
    [0x0C] = INPUT_KEY_MINUS, [0x0D] = INPUT_KEY_EQUALS,
    [0x0E] = INPUT_KEY_BACKSPACE,
    [0x0F] = INPUT_KEY_TAB,
    [0x10] = INPUT_KEY_Q, [0x11] = INPUT_KEY_W, [0x12] = INPUT_KEY_E, [0x13] = INPUT_KEY_R,
    [0x14] = INPUT_KEY_T, [0x15] = INPUT_KEY_Y, [0x16] = INPUT_KEY_U, [0x17] = INPUT_KEY_I,
    [0x18] = INPUT_KEY_O, [0x19] = INPUT_KEY_P,
    [0x1A] = INPUT_KEY_LEFT_BRACKET, [0x1B] = INPUT_KEY_RIGHT_BRACKET,
    [0x1C] = INPUT_KEY_ENTER,
    /* [0x1D] left/right control - ambiguous, resolved via extended bit */
    [0x1E] = INPUT_KEY_A, [0x1F] = INPUT_KEY_S, [0x20] = INPUT_KEY_D, [0x21] = INPUT_KEY_F,
    [0x22] = INPUT_KEY_G, [0x23] = INPUT_KEY_H, [0x24] = INPUT_KEY_J, [0x25] = INPUT_KEY_K,
    [0x26] = INPUT_KEY_L,
    [0x27] = INPUT_KEY_SEMICOLON, [0x28] = INPUT_KEY_APOSTROPHE, [0x29] = INPUT_KEY_GRAVE,
    [0x2A] = INPUT_KEY_LEFT_SHIFT,
    [0x2B] = INPUT_KEY_BACKSLASH,
    [0x2C] = INPUT_KEY_Z, [0x2D] = INPUT_KEY_X, [0x2E] = INPUT_KEY_C, [0x2F] = INPUT_KEY_V,
    [0x30] = INPUT_KEY_B, [0x31] = INPUT_KEY_N, [0x32] = INPUT_KEY_M,
    [0x33] = INPUT_KEY_COMMA, [0x34] = INPUT_KEY_PERIOD, [0x35] = INPUT_KEY_SLASH,
    [0x36] = INPUT_KEY_RIGHT_SHIFT,
    /* [0x38] left/right alt - ambiguous, resolved via extended bit */
    [0x39] = INPUT_KEY_SPACE,
    [0x3A] = INPUT_KEY_CAPS_LOCK,
    [0x3B] = INPUT_KEY_F1, [0x3C] = INPUT_KEY_F2, [0x3D] = INPUT_KEY_F3, [0x3E] = INPUT_KEY_F4,
    [0x3F] = INPUT_KEY_F5, [0x40] = INPUT_KEY_F6, [0x41] = INPUT_KEY_F7, [0x42] = INPUT_KEY_F8,
    [0x43] = INPUT_KEY_F9, [0x44] = INPUT_KEY_F10,
    /* [0x47 .. 0x53] nav cluster / numpad - ambiguous, resolved via extended bit */
    [0x57] = INPUT_KEY_F11, [0x58] = INPUT_KEY_F12,
};

/* Handles the scan codes that mean something different depending on the extended-key bit
 * (lParam bit 24) - control, alt, the nav cluster (arrows/home/end/etc, which share codes with
 * the numpad), and the super keys, which are only ever sent extended.
 */
static input_key
win64_input_key_from_scan_code (u32 scanCode, b8 isExtended)
{
    if (isExtended)
    {
        switch (scanCode)
        {
            case 0x1D: return INPUT_KEY_RIGHT_CONTROL;
            case 0x38: return INPUT_KEY_RIGHT_ALT;
            case 0x47: return INPUT_KEY_HOME;
            case 0x48: return INPUT_KEY_ARROW_UP;
            case 0x49: return INPUT_KEY_PAGE_UP;
            case 0x4B: return INPUT_KEY_ARROW_LEFT;
            case 0x4D: return INPUT_KEY_ARROW_RIGHT;
            case 0x4F: return INPUT_KEY_END;
            case 0x50: return INPUT_KEY_ARROW_DOWN;
            case 0x51: return INPUT_KEY_PAGE_DOWN;
            case 0x52: return INPUT_KEY_INSERT;
            case 0x53: return INPUT_KEY_DELETE;
            case 0x5B: return INPUT_KEY_LEFT_SUPER;
            case 0x5C: return INPUT_KEY_RIGHT_SUPER;
            default: break;
        }
    }
    else
    {
        switch (scanCode)
        {
            case 0x1D: return INPUT_KEY_LEFT_CONTROL;
            case 0x38: return INPUT_KEY_LEFT_ALT;
            default: break;
        }
    }

    if (scanCode >= 256)
    {
        return INPUT_KEY_UNKNOWN;
    }

    return gWin64ScanCodeToKey[scanCode];
}

static void
win64_input_key_event (input_key key, b8 isDown, b8 isRepeat)
{
    input_event event;
    event.kind             = INPUT_EVENT_KEY;
    event.key.key           = key;
    event.key.isDown        = isDown;
    event.key.isRepeat      = isRepeat;

    input_queue_push (&gWin64InputQueue, event);

    if (key != INPUT_KEY_UNKNOWN)
    {
        gWin64KeyHeld[key] = isDown;
    }
}

static void
win64_input_mouse_button_event (input_mouse_button button, b8 isDown)
{
    input_event event;
    event.kind                  = INPUT_EVENT_MOUSE_BUTTON;
    event.mouseButton.button    = button;
    event.mouseButton.isDown    = isDown;

    input_queue_push (&gWin64InputQueue, event);
}

void
win64_input_reset (void)
{
    input_queue_clear (&gWin64InputQueue);
}

void
win64_input_handle_message (UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            u32 scanCode   = (u32) ((lParam >> 16) & 0xFF);
            b8  isExtended = (b8) ((lParam >> 24) & 0x1);
            b8  wasDown    = (b8) ((lParam >> 30) & 0x1);
            b8  isUp       = (b8) ((lParam >> 31) & 0x1);

            input_key key = win64_input_key_from_scan_code (scanCode, isExtended);

            win64_input_key_event (key, (b8) !isUp, (b8) (wasDown && !isUp));
            break;
        }

        case WM_MOUSEMOVE:
        {
            input_event event;
            event.kind         = INPUT_EVENT_MOUSE_MOVE;
            event.mouseMove.x  = (s32) (short) LOWORD (lParam);
            event.mouseMove.y  = (s32) (short) HIWORD (lParam);

            input_queue_push (&gWin64InputQueue, event);
            break;
        }

        case WM_LBUTTONDOWN: win64_input_mouse_button_event (INPUT_MOUSE_BUTTON_LEFT, TRUE);    break;
        case WM_LBUTTONUP:   win64_input_mouse_button_event (INPUT_MOUSE_BUTTON_LEFT, FALSE);   break;
        case WM_RBUTTONDOWN: win64_input_mouse_button_event (INPUT_MOUSE_BUTTON_RIGHT, TRUE);   break;
        case WM_RBUTTONUP:   win64_input_mouse_button_event (INPUT_MOUSE_BUTTON_RIGHT, FALSE);  break;
        case WM_MBUTTONDOWN: win64_input_mouse_button_event (INPUT_MOUSE_BUTTON_MIDDLE, TRUE);  break;
        case WM_MBUTTONUP:   win64_input_mouse_button_event (INPUT_MOUSE_BUTTON_MIDDLE, FALSE); break;

        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        {
            input_mouse_button button = (GET_XBUTTON_WPARAM (wParam) == XBUTTON1)
                ? INPUT_MOUSE_BUTTON_X1
                : INPUT_MOUSE_BUTTON_X2;

            win64_input_mouse_button_event (button, (b8) (message == WM_XBUTTONDOWN));
            break;
        }

        case WM_MOUSEWHEEL:
        {
            input_event event;
            event.kind             = INPUT_EVENT_MOUSE_WHEEL;
            event.mouseWheel.delta = GET_WHEEL_DELTA_WPARAM (wParam);

            input_queue_push (&gWin64InputQueue, event);
            break;
        }

        default:
            break;
    }
}

void
win64_input_release_all_held_keys (void)
{
    for (u32 i = 0; i < INPUT_KEY_COUNT; i += 1)
    {
        if (gWin64KeyHeld[i])
        {
            win64_input_key_event ((input_key) i, FALSE, FALSE);
        }
    }
}

input_queue *
win64_input_queue_get (void)
{
    return &gWin64InputQueue;
}
