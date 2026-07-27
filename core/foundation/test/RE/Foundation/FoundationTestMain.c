/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <stdarg.h>
#include <stdio.h>

RE_GLOBAL ReUint32     gTestsRun;
RE_GLOBAL ReUint32     gTestsFailed;
RE_GLOBAL ReUint32     gCurrentTestFailures;
RE_GLOBAL const char  *gCurrentTestName;

void
RE_Test_ReportFailure( const char *file, ReUint32 line, const char *format, ... )
{
    gCurrentTestFailures += 1;

    printf( "  FAIL %s:%u\n    ", file, line );

    va_list args;
    va_start( args, format );
    vprintf( format, args );
    va_end( args );

    printf( "\n" );
}

void
RE_Test_Run( const char *name, ReTestFn fn )
{
    gCurrentTestName     = name;
    gCurrentTestFailures = 0;
    gTestsRun           += 1;

    printf( "[ run  ] %s\n", name );

    fn();

    if ( gCurrentTestFailures == 0 )
    {
        printf( "[  ok  ] %s\n", name );
    }
    else
    {
        gTestsFailed += 1;
        printf( "[ FAIL ] %s (%u failed checks)\n", name, gCurrentTestFailures );
    }
}

ReSint32
RE_Test_Summary( void )
{
    printf( "\n%u test(s) run, %u failed.\n", gTestsRun, gTestsFailed );

    return ( gTestsFailed == 0 ) ? 0 : 1;
}

int
main( void )
{
    RE_Test_Run( "VirtualMemory", RE_Test_VirtualMemory );
    RE_Test_Run( "MemoryMetadata", RE_Test_MemoryMetadata );
    RE_Test_Run( "SpinLock", RE_Test_SpinLock );
    RE_Test_Run( "MemoryArena", RE_Test_MemoryArena );
    RE_Test_Run( "MemoryScratch", RE_Test_MemoryScratch );
    RE_Test_Run( "MemoryPool", RE_Test_MemoryPool );
    RE_Test_Run( "MemoryHandlePool", RE_Test_MemoryHandlePool );
    RE_Test_Run( "MemoryFrame", RE_Test_MemoryFrame );
    RE_Test_Run( "MemoryRing", RE_Test_MemoryRing );
    RE_Test_Run( "MemorySizeClass", RE_Test_MemorySizeClass );
    RE_Test_Run( "MemoryHeap", RE_Test_MemoryHeap );
    RE_Test_Run( "MemoryThreadCache", RE_Test_MemoryThreadCache );

    return (int) RE_Test_Summary();
}
