/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64Log.h"

#include <stdio.h>
#include <string.h>

#include <RE/Foundation/FoundationBuild.h>
#include <RE/Log/Log.h>

#define WIN64_LOG_CONSOLE_COLUMNS      120
#define WIN64_LOG_CONSOLE_VISIBLE_ROWS 40
#define WIN64_LOG_CONSOLE_BUFFER_ROWS  9999

global CRITICAL_SECTION gWin64LogCriticalSection;
global HANDLE           gWin64LogConsoleHandle;
global ReBool               gWin64LogHasConsole;

#if RE_BUILD < RE_BUILD_SHIPPING

/* SetConsoleScreenBufferSize refuses to shrink the buffer below the window's current size, so
 * the window has to be shrunk to a minimal size first, then the buffer resized, then the window
 * grown to its real final size - the standard, documented-safe ordering for this dance.
 */
internal void
Win64_Log_SizeAndPlaceConsole (HWND consoleWindow)
{
    SMALL_RECT minimalRect = {0, 0, 1, 1};
    SetConsoleWindowInfo (gWin64LogConsoleHandle, TRUE, &minimalRect);

    COORD bufferSize = {WIN64_LOG_CONSOLE_COLUMNS, WIN64_LOG_CONSOLE_BUFFER_ROWS};
    SetConsoleScreenBufferSize (gWin64LogConsoleHandle, bufferSize);

    SMALL_RECT windowRect = {0, 0, WIN64_LOG_CONSOLE_COLUMNS - 1, WIN64_LOG_CONSOLE_VISIBLE_ROWS - 1};
    SetConsoleWindowInfo (gWin64LogConsoleHandle, TRUE, &windowRect);

    if (consoleWindow)
    {
        SetWindowPos (consoleWindow, NULL, 60, 60, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

/* A freshly allocated console isn't guaranteed to actually have focus/be in front - this is
 * what makes it reliably "on screen" rather than just technically existing somewhere.
 */
internal void
Win64_Log_BringConsoleToFront (HWND consoleWindow)
{
    if (!consoleWindow)
    {
        return;
    }

    ShowWindow (consoleWindow, SW_SHOW);
    SetForegroundWindow (consoleWindow);
    BringWindowToTop (consoleWindow);
}

/* Only our own RE_Log_WriteRaw uses WriteConsoleA directly - this redirect is for anything else
 * (future third-party libraries, ad hoc printf-based debugging) that goes through the CRT's
 * stdio streams instead, which otherwise target NUL for a WIN32-subsystem exe.
 */
internal void
Win64_Log_RedirectStdio (void)
{
    FILE *stream;
    freopen_s (&stream, "CONOUT$", "w", stdout);
    freopen_s (&stream, "CONOUT$", "w", stderr);
    freopen_s (&stream, "CONIN$", "r", stdin);
}

#endif

void
Win64_Log_Init (void)
{
#if RE_BUILD < RE_BUILD_SHIPPING
    /* Always allocate a fresh console rather than reusing a launching terminal's - guarantees a
     * predictable, always-visible window regardless of how the process happened to be launched,
     * rather than depending on what (if anything) is attached to its parent.
     */
    gWin64LogHasConsole = (ReBool) AllocConsole ();

    if (gWin64LogHasConsole)
    {
        gWin64LogConsoleHandle = GetStdHandle (STD_OUTPUT_HANDLE);

        DWORD consoleMode = 0;
        if (GetConsoleMode (gWin64LogConsoleHandle, &consoleMode))
        {
            SetConsoleMode (gWin64LogConsoleHandle, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }

        SetConsoleOutputCP (CP_UTF8);
        SetConsoleTitleW (L"Hydra Console");

        HWND consoleWindow = GetConsoleWindow ();
        Win64_Log_SizeAndPlaceConsole (consoleWindow);
        Win64_Log_BringConsoleToFront (consoleWindow);
        Win64_Log_RedirectStdio ();
    }
#endif

    InitializeCriticalSection (&gWin64LogCriticalSection);
}

void
Win64_Log_Shutdown (void)
{
    DeleteCriticalSection (&gWin64LogCriticalSection);

#if RE_BUILD < RE_BUILD_SHIPPING
    if (gWin64LogHasConsole)
    {
        FreeConsole ();
    }
#endif
}

internal const char *
Win64_Log_LevelPrefix (ReLogLevel level)
{
    switch (level)
    {
        case ReLogLevel_Warn:  return "[WARN] ";
        case ReLogLevel_Error: return "[ERROR] ";
        default:               return "[INFO] ";
    }
}

/* VT100 color, console-only - a debugger's OutputDebugString sink doesn't interpret escape
 * codes, so these must never reach that path (see RE_Log_WriteRaw below).
 */
internal const char *
Win64_Log_LevelColor (ReLogLevel level)
{
    switch (level)
    {
        case ReLogLevel_Warn:  return "\x1b[33m";
        case ReLogLevel_Error: return "\x1b[31m";
        default:               return "\x1b[0m";
    }
}

void
RE_Log_WriteRaw (ReLogLevel level, const char *message)
{
    EnterCriticalSection (&gWin64LogCriticalSection);

    char plainLine[1152];
    _snprintf_s (plainLine, sizeof (plainLine), _TRUNCATE, "%s%s\n", Win64_Log_LevelPrefix (level), message);
    OutputDebugStringA (plainLine);

    if (gWin64LogHasConsole)
    {
        char coloredLine[1200];
        _snprintf_s (coloredLine, sizeof (coloredLine), _TRUNCATE, "%s%s%s\x1b[0m\n",
                     Win64_Log_LevelColor (level), Win64_Log_LevelPrefix (level), message);

        DWORD written = 0;
        WriteConsoleA (gWin64LogConsoleHandle, coloredLine, (DWORD) strlen (coloredLine), &written, NULL);
    }

    LeaveCriticalSection (&gWin64LogCriticalSection);
}
