/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64.h"

#include <stdio.h>

#include "RE/Win64/Win64Input.h"
#include "RE/Win64/Win64Gamepad.h"
#include "RE/Win64/Win64Render.h"
#include "RE/Win64/Win64Startup.h"
#include "RE/Win64/Win64Crash.h"
#include "RE/Win64/Win64Log.h"
#include "RE/Win64/Win64Thread.h"
#include "RE/Win64/Win64CommandLine.h"

#include <RE/Application/Application.h>

/* Real memory-region layout (permanent/transient/etc split, if any) is intentionally undecided -
 * see the platform/engine boundary plan. One flat reserved+committed block is enough to prove the
 * ReAppContext wiring works; this is a placeholder size, not a budget.
 */
#define WIN64_APP_MEMORY_SIZE ( 64ull * 1024 * 1024 )

/* Small enough to keep inline for now - split into its own win64/time module if it grows beyond
 * a plain QueryPerformanceCounter delta.
 */
RE_GLOBAL ReSint64 gWin64PerfFrequency;
RE_GLOBAL ReSint64 gWin64PerfCounterLast;

RE_INTERNAL void
Win64_Time_Init( void )
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency( &frequency );
    gWin64PerfFrequency = frequency.QuadPart;

    LARGE_INTEGER counter;
    QueryPerformanceCounter( &counter );
    gWin64PerfCounterLast = counter.QuadPart;
}

RE_INTERNAL ReFloat32
Win64_Time_Tick( void )
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter( &counter );

    ReFloat32 deltaTime = (ReFloat32) (counter.QuadPart - gWin64PerfCounterLast) / (ReFloat32) gWin64PerfFrequency;
    gWin64PerfCounterLast = counter.QuadPart;

    return deltaTime;
}

RE_INTERNAL LRESULT CALLBACK
Win64_WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
{
    Win64_Input_HandleMessage( message, wParam, lParam );

    switch ( message )
    {
    case WM_CLOSE:
        DestroyWindow( window );
        return 0;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        return 0;

    case WM_KILLFOCUS:
        Win64_Input_ReleaseAllHeldKeys();
        return 0;

    case WM_SIZE:
        Win64_Render_NotifyResize( LOWORD( lParam ), HIWORD( lParam ) );
        return 0;

    default:
        return DefWindowProcW( window, message, wParam, lParam );
    }
}

/* TEMPORARY: makes the input queue's per-tick event count visible without a real debug-output
 * path yet. Remove once one exists.
 */
RE_INTERNAL void
Win64_Debug_UpdateWindowTitle( HWND window, ReFloat32 deltaTime )
{
    ReInputQueue *queue = Win64_Input_GetQueue();

    /* wsprintfW deliberately has no floating-point support, so the CRT's swprintf_s is used here
     * instead - purely a kernel debug-output concern, unrelated to the engine's CRT policy.
     */
    wchar_t title[64];
    swprintf_s( title, 64, L"Hydra - events: %u  dt: %.4f", queue->count, (double) deltaTime );

    SetWindowTextW( window, title );
}

RE_INTERNAL HWND
Win64_CreateWindow( HINSTANCE instance )
{
    WNDCLASSEXW windowClass = {0};

    windowClass.cbSize        = sizeof( windowClass );
    windowClass.style         = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = Win64_WindowProc;
    windowClass.hInstance     = instance;
    windowClass.hCursor       = LoadCursorW( NULL, IDC_ARROW );
    windowClass.lpszClassName = L"HydraWindowClass";

    if ( !RegisterClassExW( &windowClass ) )
    {
        return NULL;
    }

    return CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"Hydra",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
        NULL, NULL,
        instance,
        NULL );
}

int WINAPI
wWinMain( HINSTANCE instance, HINSTANCE previousInstance, PWSTR commandLine, int showCommand )
{
    (void) previousInstance;
    (void) commandLine;

    /* Must run before anything else - no log/crash infrastructure exists yet to report a
     * problem through, so this shows its own MessageBoxW on failure.
     */
    if ( !Win64_Startup_CheckCpuFeatures() )
    {
        return 1;
    }

    Win64_Log_Init();
    Win64_Crash_Init();
    Win64_CommandLine_Init();

    ReBool  running  = RE_True;
    int exitCode = 0;

    __try
    {
        Win64_Startup_InitCom();
        Win64_Startup_SetDpiAwareness();
        Win64_Thread_SetCurrentThreadName( L"MainThread" );

        HWND window = Win64_CreateWindow( instance );
        if ( !window )
        {
            exitCode = 1;
            __leave;
        }

        if ( !Win64_Render_Init( window ) )
        {
            DestroyWindow( window );
            exitCode = 1;
            __leave;
        }

        ShowWindow( window, showCommand );
        UpdateWindow( window );

        void *appMemory = VirtualAlloc( NULL, WIN64_APP_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
        if ( !appMemory )
        {
            Win64_Render_Shutdown();
            DestroyWindow( window );
            exitCode = 1;
            __leave;
        }

        ReAppContext context = {0};
        context.memory      = appMemory;
        context.memorySize  = WIN64_APP_MEMORY_SIZE;
        context.input       = Win64_Input_GetQueue();
        context.argCount    = Win64_CommandLine_GetArgCount();
        context.args        = Win64_CommandLine_GetArgs();

        if ( !RE_Application_Init( &context ) )
        {
            VirtualFree( appMemory, 0, MEM_RELEASE );
            Win64_Render_Shutdown();
            DestroyWindow( window );
            exitCode = 1;
            __leave;
        }

        Win64_Startup_ConfigureTiming();
        Win64_Time_Init();

        while ( running )
        {
            Win64_Input_Reset();

            MSG message;
            while ( PeekMessageW( &message, NULL, 0, 0, PM_REMOVE ) )
            {
                if ( message.message == WM_QUIT )
                {
                    running  = RE_False;
                    exitCode = (int) message.wParam;
                    break;
                }

                TranslateMessage( &message );
                DispatchMessageW( &message );
            }

            if ( !running )
            {
                break;
            }

            Win64_Render_ProcessResize();

            /* XInput has no message-based notification, so it's polled explicitly once per tick
             * rather than being fed from Win64_WindowProc like keyboard/mouse are.
             */
            Win64_Gamepad_Poll( Win64_Input_GetQueue() );

            ReFloat32 deltaTime = Win64_Time_Tick();

            RE_Application_Tick( &context, deltaTime );

            Win64_Render_Draw();

            Win64_Debug_UpdateWindowTitle( window, deltaTime );
        }

        RE_Application_Shutdown( &context );
        Win64_Render_Shutdown();
        VirtualFree( appMemory, 0, MEM_RELEASE );
        Win64_Startup_ShutdownTiming();
    }
    __except( Win64_Crash_ExceptionFilter( GetExceptionInformation() ) )
    {
        exitCode = 1;
    }

    Win64_Startup_ShutdownCom();
    Win64_Log_Shutdown();

    return exitCode;
}
