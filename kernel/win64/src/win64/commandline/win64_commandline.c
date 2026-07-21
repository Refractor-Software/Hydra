/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "win64/commandline/win64_commandline.h"

#include <shellapi.h>

#include "foundation/memory/foundation_memory_arena.h"

/* Windows' documented practical command-line cap is 32767 UTF-16 code units; each can expand to
 * at most 3 UTF-8 bytes, so ~98301 bytes is the true worst case - 128 KiB gives comfortable
 * margin. 256 args is similarly generous for anything realistic.
 */
#define WIN64_COMMAND_LINE_MAX_ARGS  256
#define WIN64_COMMAND_LINE_ARENA_SIZE (128 * 1024)

static string_view gWin64CommandLineArgs[WIN64_COMMAND_LINE_MAX_ARGS];
static s32          gWin64CommandLineArgCount;

static arena gWin64CommandLineArena;
static u8    gWin64CommandLineArenaBuffer[WIN64_COMMAND_LINE_ARENA_SIZE];

void
win64_command_line_init (void)
{
    arena_init (&gWin64CommandLineArena, gWin64CommandLineArenaBuffer, sizeof (gWin64CommandLineArenaBuffer));

    int    wideArgCount = 0;
    LPWSTR *wideArgs     = CommandLineToArgvW (GetCommandLineW (), &wideArgCount);
    if (!wideArgs)
    {
        gWin64CommandLineArgCount = 0;
        return;
    }

    s32 argCount = (wideArgCount > WIN64_COMMAND_LINE_MAX_ARGS) ? WIN64_COMMAND_LINE_MAX_ARGS : wideArgCount;

    for (s32 i = 0; i < argCount; i += 1)
    {
        /* Size-then-convert: first call sizes the buffer (including room for the null
         * terminator, which is why the resulting string_view's length is byteCountWithNull - 1).
         */
        int byteCountWithNull = WideCharToMultiByte (CP_UTF8, 0, wideArgs[i], -1, NULL, 0, NULL, NULL);
        if (byteCountWithNull <= 0)
        {
            gWin64CommandLineArgs[i] = string_view_from_bytes (0, 0);
            continue;
        }

        u8 *buffer = (u8 *) arena_alloc (&gWin64CommandLineArena, (usize) byteCountWithNull, 1);
        if (!buffer)
        {
            /* Arena exhausted (should never realistically happen given the sizing above) -
             * truncate the arg list here rather than fault.
             */
            argCount = i;
            break;
        }

        WideCharToMultiByte (CP_UTF8, 0, wideArgs[i], -1, (char *) buffer, byteCountWithNull, NULL, NULL);

        gWin64CommandLineArgs[i] = string_view_from_bytes (buffer, (usize) (byteCountWithNull - 1));
    }

    gWin64CommandLineArgCount = argCount;

    LocalFree (wideArgs);
}

s32
win64_command_line_get_arg_count (void)
{
    return gWin64CommandLineArgCount;
}

string_view *
win64_command_line_get_args (void)
{
    return gWin64CommandLineArgs;
}
