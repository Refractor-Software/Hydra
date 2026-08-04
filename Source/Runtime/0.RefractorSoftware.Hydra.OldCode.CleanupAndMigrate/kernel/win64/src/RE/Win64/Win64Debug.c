/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64.h"

#include <dbghelp.h>
#include <stdio.h>

#include <RE/Foundation/FoundationDebug.h>
#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationSpinLock.h>

/*
    Windows side of the RE_Debug_* boundary.

    DbgHelp is initialised by Win64Crash.c at startup. Symbol resolution here is best-effort: if
    it has not run, or symbols are unavailable, addresses are printed instead. A stack of bare
    addresses still narrows a leak to a call site once run through the map file, which is far
    better than refusing to report at all.
*/

/* DbgHelp is explicitly documented as not thread-safe, and the decorators call this from whatever
 * thread hit the problem.
 */
RE_GLOBAL ReSpinLock gWin64DebugSymbolLock;

ReUint32
RE_Debug_CaptureCallstack( void **frames, ReUint32 maxFrames, ReUint32 skipFrames )
{
    if ( !frames || maxFrames == 0 )
    {
        return 0;
    }

    if ( maxFrames > RE_CALLSTACK_MAX_FRAMES )
    {
        maxFrames = RE_CALLSTACK_MAX_FRAMES;
    }

    /* One more skipped than asked for, so this function never appears in its own result. */
    return (ReUint32) RtlCaptureStackBackTrace( (DWORD) ( skipFrames + 1 ), (DWORD) maxFrames, frames, NULL );
}

void
RE_Debug_FormatCallstack( void *const *frames, ReUint32 frameCount, char *buffer, ReUint64 bufferSize )
{
    if ( !buffer || bufferSize == 0 )
    {
        return;
    }

    buffer[0] = 0;

    if ( !frames || frameCount == 0 )
    {
        return;
    }

    RE_SpinLock_Acquire( &gWin64DebugSymbolLock );

    /* SYMBOL_INFO is a flexible-array-style struct: the name is written past the end of the
     * declared structure, so the storage has to be over-sized by hand.
     */
    ReUint8 symbolStorage[sizeof( SYMBOL_INFO ) + 256];
    RE_Memory_Zero( symbolStorage, sizeof( symbolStorage ) );

    SYMBOL_INFO *symbol  = (SYMBOL_INFO *) symbolStorage;
    symbol->SizeOfStruct = sizeof( SYMBOL_INFO );
    symbol->MaxNameLen   = 255;

    HANDLE   process = GetCurrentProcess();
    ReUint64 written = 0;

    for ( ReUint32 i = 0; i < frameCount && written + 1 < bufferSize; i += 1 )
    {
        DWORD64 address     = (DWORD64) frames[i];
        DWORD64 displacement = 0;

        char line[320];

        if ( SymFromAddr( process, address, &displacement, symbol ) )
        {
            IMAGEHLP_LINE64 lineInfo;
            DWORD           lineDisplacement = 0;

            lineInfo.SizeOfStruct = sizeof( lineInfo );

            if ( SymGetLineFromAddr64( process, address, &lineDisplacement, &lineInfo ) )
            {
                _snprintf_s( line, sizeof( line ), _TRUNCATE, "      %s (%s:%lu)\n",
                    symbol->Name, lineInfo.FileName, lineInfo.LineNumber );
            }
            else
            {
                _snprintf_s( line, sizeof( line ), _TRUNCATE, "      %s\n", symbol->Name );
            }
        }
        else
        {
            _snprintf_s( line, sizeof( line ), _TRUNCATE, "      0x%llX\n", (unsigned long long) address );
        }

        ReUint64 lineLength = strlen( line );

        if ( written + lineLength + 1 > bufferSize )
        {
            break;
        }

        memcpy( buffer + written, line, lineLength );
        written += lineLength;
        buffer[written] = 0;
    }

    RE_SpinLock_Release( &gWin64DebugSymbolLock );
}
