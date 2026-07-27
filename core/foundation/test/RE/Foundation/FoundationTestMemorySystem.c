/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <stdio.h>
#include <string.h>

#include <RE/Foundation/FoundationContext.h>
#include <RE/Foundation/FoundationMemorySystem.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

#include "RE/Win64/Win64.h"

/*
    Mirrors what the engine actually does - init, allocate, tick frames, shut down - so that the
    end-to-end lifecycle is verified somewhere it can be asserted on rather than only by watching
    a window open and close.
*/

RE_GLOBAL ReUint32 gSystemTestProblemMessages;

RE_INTERNAL void
System_TestReport( const char *message )
{
    /* Echoed so a run shows the same summary the engine logs, and counted so the test can insist
     * a clean run reports no problems at all.
     */
    printf( "    | %s\n", message );

    if ( strstr( message, "leaked" ) || strstr( message, "outstanding" ) || strstr( message, "WARNING" ) )
    {
        gSystemTestProblemMessages += 1;
    }
}

RE_INTERNAL DWORD WINAPI
System_TestWorker( LPVOID parameter )
{
    ReUint32 threadIndex = (ReUint32) (ReUint64) parameter;

    /* Every thread that allocates sets up its own context; nothing is inherited. */
    if ( !RE_Context_ThreadInit( threadIndex ) )
    {
        return 1;
    }

    for ( ReUint32 i = 0; i < 512; i += 1 )
    {
        void *block = RE_Memory_Alloc( RE_Context_Allocator(), 64 + ( i % 512 ), 16 );

        if ( block )
        {
            RE_Memory_Free( RE_Context_Allocator(), block, 64 + ( i % 512 ) );
        }

        void *frameBlock = RE_Context_FrameAlloc( 256, 16 );

        if ( frameBlock )
        {
            RE_Memory_Set( frameBlock, (ReUint8) threadIndex, 256 );
        }
    }

    RE_Context_ThreadShutdown();

    return 0;
}

void
RE_Test_MemorySystem( void )
{
    gSystemTestProblemMessages = 0;
    RE_Memory_SetReportFn( System_TestReport );

    ReMemorySystemConfig config = RE_MemorySystem_DefaultConfig();

    RE_TEST_CHECK( config.frameBufferCount >= 2 );
    RE_TEST_CHECK( config.frameThreadCount >= 1 );

    RE_TEST_CHECK( RE_MemorySystem_Init( &config ) );
    RE_TEST_CHECK( RE_MemorySystem_IsInitialized() );

    /* Idempotent - a subsystem that is unsure whether the system is up should be able to ask for
     * it rather than having to know.
     */
    RE_TEST_CHECK( RE_MemorySystem_Init( &config ) );

    /* --- the ambient context is available on the thread that brought the system up --- */
    {
        RE_TEST_CHECK_NOT_NULL( RE_Context_Get() );
        RE_TEST_CHECK_NOT_NULL( RE_Context_Allocator() );
        RE_TEST_CHECK_NOT_NULL( RE_Context_FrameArena() );

        RE_TEST_CHECK( RE_Context_Allocator()->alloc != 0 );
    }

    /* --- general-purpose allocation through the context --- */
    {
        ReUint8 *block = (ReUint8 *) RE_Memory_Alloc( RE_Context_Allocator(), 1024, 16 );

        RE_TEST_CHECK_NOT_NULL( block );

        if ( block )
        {
            RE_Memory_Set( block, 0x9A, 1024 );
            RE_TEST_CHECK_EQ_UINT( block[1023], 0x9A );

            RE_Memory_Free( RE_Context_Allocator(), block, 1024 );
        }
    }

    /* --- frame memory is reclaimed by rotation, not by freeing --- */
    {
        void *firstFrame = RE_Context_FrameAlloc( 4096, 16 );
        RE_TEST_CHECK_NOT_NULL( firstFrame );

        ReUint64 usedInFrame = RE_Arena_Used( RE_Context_FrameArena() );
        RE_TEST_CHECK( usedInFrame >= 4096 );

        /* Enough frames to come back around to the same buffer. */
        for ( ReUint64 frame = 1; frame <= config.frameBufferCount; frame += 1 )
        {
            RE_MemorySystem_BeginFrame( frame );
        }

        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( RE_Context_FrameArena() ), 0 );
    }

    /* --- tags flow through to the report --- */
    {
        RE_MEMORY_SCOPE_BEGIN( "SystemTest" );

        void *tagged = RE_Memory_Alloc( RE_Context_Allocator(), 8192, 16 );
        RE_TEST_CHECK_NOT_NULL( tagged );

        RE_MEMORY_SCOPE_END();

        RE_Memory_Free( RE_Context_Allocator(), tagged, 8192 );
    }

    /* --- several threads, each setting up its own context --- */
    {
        enum { WorkerCount = 4 };

        HANDLE workers[WorkerCount];

        for ( ReUint32 i = 0; i < WorkerCount; i += 1 )
        {
            /* Thread index 0 belongs to this thread, so workers start at 1. */
            workers[i] = CreateThread( NULL, 0, System_TestWorker, (LPVOID) (ReUint64) ( i + 1 ), 0, NULL );
            RE_TEST_CHECK_NOT_NULL( workers[i] );
        }

        WaitForMultipleObjects( WorkerCount, workers, TRUE, INFINITE );

        for ( ReUint32 i = 0; i < WorkerCount; i += 1 )
        {
            DWORD result = 1;
            GetExitCodeThread( workers[i], &result );

            RE_TEST_CHECK_EQ_UINT( result, 0 );

            CloseHandle( workers[i] );
        }
    }

    /* --- the stats a session should be judged on --- */
    {
        ReMemorySystemStats stats = RE_MemorySystem_GetStats();

        /* A non-zero overflow count is a bug: the frame budget was exceeded and allocations
         * spilled somewhere slower, which no other metric would show.
         */
        RE_TEST_CHECK_EQ_UINT( stats.frameOverflowCount, 0 );

        /* Metadata is real memory and must be accounted for, or it shows up later as an
         * unexplained gap against what the OS reports.
         */
        RE_TEST_CHECK( stats.metadata.bytesCommitted > 0 );
        RE_TEST_CHECK( stats.metadata.bytesReserved >= stats.metadata.bytesCommitted );

        RE_TEST_CHECK( stats.heap.largeBytesCommitted >= stats.heap.largeBytesRequested );

        printf( "    --- memory summary ---\n" );
        RE_MemorySystem_ReportStats();
    }

    /* --- nothing outstanding before teardown --- */
    {
        RE_TEST_CHECK_EQ_UINT( RE_MemorySystem_ReportFindings(), 0 );
    }

    /* --- and teardown itself reports clean --- */
    {
        gSystemTestProblemMessages = 0;

        ReUint64 problems = RE_MemorySystem_Shutdown();

        RE_TEST_CHECK_EQ_UINT( problems, 0 );
        RE_TEST_CHECK_EQ_UINT( gSystemTestProblemMessages, 0 );
        RE_TEST_CHECK( !RE_MemorySystem_IsInitialized() );
    }

    /* --- a deliberate leak has to be caught, or the clean result above proves nothing --- */
    {
        RE_TEST_CHECK( RE_MemorySystem_Init( &config ) );

        void *leaked = RE_Memory_Alloc( RE_Context_Allocator(), 4096, 16 );
        RE_TEST_CHECK_NOT_NULL( leaked );

        gSystemTestProblemMessages = 0;

        ReUint64 problems = RE_MemorySystem_Shutdown();

#if RE_BUILD < RE_BUILD_SHIPPING
        /* Leak tracking is on below shipping, so this must be seen and named. */
        RE_TEST_CHECK( problems >= 1 );
        RE_TEST_CHECK( gSystemTestProblemMessages >= 1 );
#else
        /* Compiled out in shipping - there is nothing watching, by design. */
        RE_TEST_CHECK_EQ_UINT( problems, 0 );
#endif
    }

    RE_Memory_SetReportFn( 0 );
}
