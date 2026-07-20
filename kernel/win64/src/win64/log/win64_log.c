/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "win64/log/win64_log.h"

#include <stdio.h>
#include <string.h>

#include "log/log.h"

#define WIN64_LOG_CONSOLE_COLUMNS      120
#define WIN64_LOG_CONSOLE_VISIBLE_ROWS 40
#define WIN64_LOG_CONSOLE_BUFFER_ROWS  9999

static CRITICAL_SECTION gWin64LogCriticalSection;
static HANDLE           gWin64LogConsoleHandle;
static b8               gWin64LogHasConsole;

#if !HYDRA_SHIPPING

/* SetConsoleScreenBufferSize refuses to shrink the buffer below the window's current size, so
 * the window has to be shrunk to a minimal size first, then the buffer resized, then the window
 * grown to its real final size - the standard, documented-safe ordering for this dance.
 */
static void
win64_log_size_and_place_console (HWND consoleWindow)
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
static void
win64_log_bring_console_to_front (HWND consoleWindow)
{
    if (!consoleWindow)
    {
        return;
    }

    ShowWindow (consoleWindow, SW_SHOW);
    SetForegroundWindow (consoleWindow);
    BringWindowToTop (consoleWindow);
}

/* Only our own log_write_raw uses WriteConsoleA directly - this redirect is for anything else
 * (future third-party libraries, ad hoc printf-based debugging) that goes through the CRT's
 * stdio streams instead, which otherwise target NUL for a WIN32-subsystem exe.
 */
static void
win64_log_redirect_stdio (void)
{
    FILE *stream;
    freopen_s (&stream, "CONOUT$", "w", stdout);
    freopen_s (&stream, "CONOUT$", "w", stderr);
    freopen_s (&stream, "CONIN$", "r", stdin);
}

#endif

void
win64_log_init (void)
{
#if !HYDRA_SHIPPING
    /* Always allocate a fresh console rather than reusing a launching terminal's - guarantees a
     * predictable, always-visible window regardless of how the process happened to be launched,
     * rather than depending on what (if anything) is attached to its parent.
     */
    gWin64LogHasConsole = (b8) AllocConsole ();

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
        win64_log_size_and_place_console (consoleWindow);
        win64_log_bring_console_to_front (consoleWindow);
        win64_log_redirect_stdio ();
    }
#endif

    InitializeCriticalSection (&gWin64LogCriticalSection);
}

void
win64_log_shutdown (void)
{
    DeleteCriticalSection (&gWin64LogCriticalSection);

#if !HYDRA_SHIPPING
    if (gWin64LogHasConsole)
    {
        FreeConsole ();
    }
#endif
}

static const char *
win64_log_level_prefix (log_level level)
{
    switch (level)
    {
        case LOG_LEVEL_WARN:  return "[WARN] ";
        case LOG_LEVEL_ERROR: return "[ERROR] ";
        default:               return "[INFO] ";
    }
}

/* VT100 color, console-only - a debugger's OutputDebugString sink doesn't interpret escape
 * codes, so these must never reach that path (see win64_log_write_raw below).
 */
static const char *
win64_log_level_color (log_level level)
{
    switch (level)
    {
        case LOG_LEVEL_WARN:  return "\x1b[33m";
        case LOG_LEVEL_ERROR: return "\x1b[31m";
        default:               return "\x1b[0m";
    }
}

void
log_write_raw (log_level level, const char *message)
{
    EnterCriticalSection (&gWin64LogCriticalSection);

    char plainLine[1152];
    _snprintf_s (plainLine, sizeof (plainLine), _TRUNCATE, "%s%s\n", win64_log_level_prefix (level), message);
    OutputDebugStringA (plainLine);

    if (gWin64LogHasConsole)
    {
        char coloredLine[1200];
        _snprintf_s (coloredLine, sizeof (coloredLine), _TRUNCATE, "%s%s%s\x1b[0m\n",
                     win64_log_level_color (level), win64_log_level_prefix (level), message);

        DWORD written = 0;
        WriteConsoleA (gWin64LogConsoleHandle, coloredLine, (DWORD) strlen (coloredLine), &written, NULL);
    }

    LeaveCriticalSection (&gWin64LogCriticalSection);
}
