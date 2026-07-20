/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#define UNICODE
#include <windows.h>

static LRESULT CALLBACK
win64_window_proc (HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CLOSE:
            DestroyWindow (window);
            return 0;

        case WM_DESTROY:
            PostQuitMessage (0);
            return 0;

        default:
            return DefWindowProcW (window, message, wParam, lParam);
    }
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

    MSG message;
    while (GetMessageW (&message, NULL, 0, 0) > 0)
    {
        TranslateMessage (&message);
        DispatchMessageW (&message);
    }

    return (int) message.wParam;
}
