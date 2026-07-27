/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64CommandLine.h"

#include <shellapi.h>

#include <RE/Foundation/FoundationMemoryArena.h>

/* Windows' documented practical command-line cap is 32767 UTF-16 code units; each can expand to
 * at most 3 UTF-8 bytes, so ~98301 bytes is the true worst case - 128 KiB gives comfortable
 * margin. 256 args is similarly generous for anything realistic.
 */
#define WIN64_COMMAND_LINE_MAX_ARGS  256
#define WIN64_COMMAND_LINE_ARENA_SIZE ( 128 * 1024 )

global ReStringView gWin64CommandLineArgs[WIN64_COMMAND_LINE_MAX_ARGS];
global ReSint32          gWin64CommandLineArgCount;

global ReArena gWin64CommandLineArena;
global ReUint8    gWin64CommandLineArenaBuffer[WIN64_COMMAND_LINE_ARENA_SIZE];

void
Win64_CommandLine_Init( void )
{
    /* Fixed mode over a static buffer, not a virtual reservation: this runs before the engine's
     * memory system exists, so it cannot depend on anything that has to be initialised first.
     */
    RE_Arena_InitFixed( &gWin64CommandLineArena, gWin64CommandLineArenaBuffer,
        sizeof( gWin64CommandLineArenaBuffer ) );

    int    wideArgCount = 0;
    LPWSTR *wideArgs     = CommandLineToArgvW( GetCommandLineW(), &wideArgCount );
    if ( !wideArgs )
    {
        gWin64CommandLineArgCount = 0;
        return;
    }

    ReSint32 argCount = (wideArgCount > WIN64_COMMAND_LINE_MAX_ARGS) ? WIN64_COMMAND_LINE_MAX_ARGS : wideArgCount;

    for ( ReSint32 i = 0; i < argCount; i += 1 )
    {
        /* Size-then-convert: first call sizes the buffer (including room for the null
         * terminator, which is why the resulting ReStringView's length is byteCountWithNull - 1).
         */
        int byteCountWithNull = WideCharToMultiByte( CP_UTF8, 0, wideArgs[i], -1, NULL, 0, NULL, NULL );
        if ( byteCountWithNull <= 0 )
        {
            gWin64CommandLineArgs[i] = RE_StringView_FromBytes( 0, 0 );
            continue;
        }

        ReUint8 *buffer = (ReUint8 *) RE_Arena_Alloc( &gWin64CommandLineArena, (ReUint64) byteCountWithNull, 1 );
        if ( !buffer )
        {
            /* Arena exhausted (should never realistically happen given the sizing above) -
             * truncate the arg list here rather than fault.
             */
            argCount = i;
            break;
        }

        WideCharToMultiByte( CP_UTF8, 0, wideArgs[i], -1, (char *) buffer, byteCountWithNull, NULL, NULL );

        gWin64CommandLineArgs[i] = RE_StringView_FromBytes( buffer, (ReUint64) (byteCountWithNull - 1) );
    }

    gWin64CommandLineArgCount = argCount;

    LocalFree( wideArgs );
}

ReSint32
Win64_CommandLine_GetArgCount( void )
{
    return gWin64CommandLineArgCount;
}

ReStringView *
Win64_CommandLine_GetArgs( void )
{
    return gWin64CommandLineArgs;
}
