/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "win64/win64.h"

#include <stdio.h>

#include "win64/input/win64_input.h"
#include "win64/gamepad/win64_gamepad.h"
#include "win64/render/win64_render.h"

#include "application/application.h"

/* Real memory-region layout (permanent/transient/etc split, if any) is intentionally undecided -
 * see the platform/engine boundary plan. One flat reserved+committed block is enough to prove the
 * app_context wiring works; this is a placeholder size, not a budget.
 */
#define WIN64_APP_MEMORY_SIZE (64ull * 1024 * 1024)

/* Small enough to keep inline for now - split into its own win64/time module if it grows beyond
 * a plain QueryPerformanceCounter delta.
 */
static s64 gWin64PerfFrequency;
static s64 gWin64PerfCounterLast;

static void
win64_time_init (void)
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency (&frequency);
    gWin64PerfFrequency = frequency.QuadPart;

    LARGE_INTEGER counter;
    QueryPerformanceCounter (&counter);
    gWin64PerfCounterLast = counter.QuadPart;
}

static f32
win64_time_tick (void)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter (&counter);

    f32 deltaTime = (f32) (counter.QuadPart - gWin64PerfCounterLast) / (f32) gWin64PerfFrequency;
    gWin64PerfCounterLast = counter.QuadPart;

    return deltaTime;
}

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

        case WM_SIZE:
            win64_render_notify_resize (LOWORD (lParam), HIWORD (lParam));
            return 0;

        default:
            return DefWindowProcW (window, message, wParam, lParam);
    }
}

/* TEMPORARY: makes the input queue's per-tick event count visible without a real debug-output
 * path yet. Remove once one exists.
 */
static void
win64_debug_update_window_title (HWND window, f32 deltaTime)
{
    input_queue *queue = win64_input_queue_get ();

    /* wsprintfW deliberately has no floating-point support, so the CRT's swprintf_s is used here
     * instead - purely a kernel debug-output concern, unrelated to the engine's CRT policy.
     */
    wchar_t title[64];
    swprintf_s (title, 64, L"Hydra - events: %u  dt: %.4f", queue->count, (double) deltaTime);

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

    if (!win64_render_init (window))
    {
        DestroyWindow (window);
        return 1;
    }

    ShowWindow (window, showCommand);
    UpdateWindow (window);

    void *appMemory = VirtualAlloc (NULL, WIN64_APP_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!appMemory)
    {
        DestroyWindow (window);
        return 1;
    }

    app_context context = {0};
    context.memory      = appMemory;
    context.memorySize  = WIN64_APP_MEMORY_SIZE;
    context.input       = win64_input_queue_get ();

    if (!application_init (&context))
    {
        VirtualFree (appMemory, 0, MEM_RELEASE);
        DestroyWindow (window);
        return 1;
    }

    win64_time_init ();

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

        win64_render_process_resize ();

        /* XInput has no message-based notification, so it's polled explicitly once per tick
         * rather than being fed from win64_window_proc like keyboard/mouse are.
         */
        win64_gamepad_poll (win64_input_queue_get ());

        f32 deltaTime = win64_time_tick ();

        application_tick (&context, deltaTime);

        win64_render_draw ();

        win64_debug_update_window_title (window, deltaTime);
    }

    application_shutdown (&context);
    win64_render_shutdown ();
    VirtualFree (appMemory, 0, MEM_RELEASE);

    return exitCode;
}
