/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "win64/log/win64_log.h"

#include <stdio.h>
#include <string.h>

#include "log/log.h"

static CRITICAL_SECTION gWin64LogCriticalSection;
static HANDLE           gWin64LogConsoleHandle;
static b8               gWin64LogHasConsole;

void
win64_log_init (void)
{
    gWin64LogHasConsole = (b8) (AttachConsole (ATTACH_PARENT_PROCESS) || AllocConsole ());

    if (gWin64LogHasConsole)
    {
        gWin64LogConsoleHandle = GetStdHandle (STD_OUTPUT_HANDLE);

        DWORD consoleMode = 0;
        if (GetConsoleMode (gWin64LogConsoleHandle, &consoleMode))
        {
            SetConsoleMode (gWin64LogConsoleHandle, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }

        SetConsoleOutputCP (CP_UTF8);
    }

    InitializeCriticalSection (&gWin64LogCriticalSection);
}

void
win64_log_shutdown (void)
{
    DeleteCriticalSection (&gWin64LogCriticalSection);

    if (gWin64LogHasConsole)
    {
        FreeConsole ();
    }
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
