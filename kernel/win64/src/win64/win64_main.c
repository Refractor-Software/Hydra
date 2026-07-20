/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "win64/win64.h"

#include "win64/input/win64_input.h"
#include "win64/gamepad/win64_gamepad.h"

static LRESULT CALLBACK
win64_window_proc (HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    win64_input_handle_message (message, wParam, lParam);

    switch (message)
    {
        case WM_CLOSE:
            DestroyWindow (window);
            return 0;

        case WM_DESTROY:
            PostQuitMessage (0);
            return 0;

        case WM_KILLFOCUS:
            win64_input_release_all_held_keys ();
            return 0;

        default:
            return DefWindowProcW (window, message, wParam, lParam);
    }
}

/* TEMPORARY: makes the input queue's per-tick event count visible without a real debug-output
 * path yet. Remove once one exists.
 */
static void
win64_debug_update_window_title (HWND window)
{
    input_queue *queue = win64_input_queue_get ();

    wchar_t title[64];
    wsprintfW (title, L"Hydra - events: %u", queue->count);

    SetWindowTextW (window, title);
}

static HWND
win64_window_create (HINSTANCE instance)
{
    WNDCLASSEXW windowClass = {0};

    windowClass.cbSize        = sizeof (windowClass);
    windowClass.style         = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = win64_window_proc;
    windowClass.hInstance     = instance;
    windowClass.hCursor       = LoadCursorW (NULL, IDC_ARROW);
    windowClass.lpszClassName = L"HydraWindowClass";

    if (!RegisterClassExW (&windowClass))
    {
        return NULL;
    }

    return CreateWindowExW (
        0,
        windowClass.lpszClassName,
        L"Hydra",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
        NULL, NULL,
        instance,
        NULL);
}

int WINAPI
wWinMain (HINSTANCE instance, HINSTANCE previousInstance, PWSTR commandLine, int showCommand)
{
    (void) previousInstance;
    (void) commandLine;

    HWND window = win64_window_create (instance);
    if (!window)
    {
        return 1;
    }

    ShowWindow (window, showCommand);
    UpdateWindow (window);

    b8  running  = TRUE;
    int exitCode = 0;

    while (running)
    {
        win64_input_reset ();

        MSG message;
        while (PeekMessageW (&message, NULL, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                running  = FALSE;
                exitCode = (int) message.wParam;
                break;
            }

            TranslateMessage (&message);
            DispatchMessageW (&message);
        }

        if (!running)
        {
            break;
        }

        /* XInput has no message-based notification, so it's polled explicitly once per tick
         * rather than being fed from win64_window_proc like keyboard/mouse are.
         */
        win64_gamepad_poll (win64_input_queue_get ());

        /* TODO(will) real per-frame work (simulate/render) goes here once the engine contract
         * exists. Sleep is a placeholder so this doesn't busy-spin a CPU core doing nothing.
         */
        Sleep (1);

        win64_debug_update_window_title (window);
    }

    return exitCode;
}
