/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <RE/Foundation/FoundationMemoryFrame.h>
#include <RE/Foundation/FoundationMemoryRing.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

void
RE_Test_MemoryFrame( void )
{
    /* --- the cache-line invariant the per-thread split depends on --- */
    {
        RE_TEST_CHECK_EQ_UINT( sizeof( ReFrameThreadArena ) % RE_CACHE_LINE_SIZE, 0 );
    }

    /* --- N-buffering: resetting a frame must not touch the buffer a reader is still on --- */
    {
        ReFrameAllocator frame;
        RE_TEST_CHECK( RE_Frame_Init( &frame, 3, 4, 1024 * 1024 ) );

        RE_Frame_BeginFrame( &frame, 0 );

        ReUint8 *frameZero = (ReUint8 *) RE_Frame_Alloc( &frame, 0, 256, 16 );
        RE_TEST_CHECK_NOT_NULL( frameZero );
        RE_Memory_Set( frameZero, 0x11, 256 );

        RE_Frame_BeginFrame( &frame, 1 );

        ReUint8 *frameOne = (ReUint8 *) RE_Frame_Alloc( &frame, 0, 256, 16 );
        RE_TEST_CHECK_NOT_NULL( frameOne );
        RE_Memory_Set( frameOne, 0x22, 256 );

        /* Different buffer, so frame 0's data is untouched - this is exactly what a pipelined
         * render thread reading one frame behind depends on.
         */
        RE_TEST_CHECK( frameOne != frameZero );
        RE_TEST_CHECK_EQ_UINT( frameZero[0], 0x11 );

        RE_Frame_BeginFrame( &frame, 2 );
        ReUint8 *frameTwo = (ReUint8 *) RE_Frame_Alloc( &frame, 0, 256, 16 );
        RE_TEST_CHECK( frameTwo != frameZero && frameTwo != frameOne );
        RE_TEST_CHECK_EQ_UINT( frameZero[0], 0x11 );

        /* Frame 3 rotates back onto frame 0's buffer, which is now three frames stale. */
        RE_Frame_BeginFrame( &frame, 3 );
        ReUint8 *frameThree = (ReUint8 *) RE_Frame_Alloc( &frame, 0, 256, 16 );
        RE_TEST_CHECK( frameThree == frameZero );

        RE_Frame_Shutdown( &frame );
    }

    /* --- threads get disjoint arenas --- */
    {
        ReFrameAllocator frame;
        RE_TEST_CHECK( RE_Frame_Init( &frame, 2, 8, 1024 * 1024 ) );
        RE_Frame_BeginFrame( &frame, 0 );

        ReUint8 *perThread[8];

        for ( ReUint32 thread = 0; thread < 8; thread += 1 )
        {
            perThread[thread] = (ReUint8 *) RE_Frame_Alloc( &frame, thread, 128, 16 );
            RE_TEST_CHECK_NOT_NULL( perThread[thread] );

            if ( perThread[thread] )
            {
                RE_Memory_Set( perThread[thread], (ReUint8) thread, 128 );
            }
        }

        /* Overlapping arenas would show up as an earlier thread's fill being clobbered. */
        for ( ReUint32 thread = 0; thread < 8; thread += 1 )
        {
            if ( perThread[thread] )
            {
                RE_TEST_CHECK_EQ_UINT( perThread[thread][0], thread );
                RE_TEST_CHECK_EQ_UINT( perThread[thread][127], thread );
            }
        }

        /* Each thread's arena really is on its own cache line. */
        for ( ReUint32 thread = 1; thread < 8; thread += 1 )
        {
            ReUint64 a = (ReUint64) RE_Frame_Arena( &frame, thread - 1 );
            ReUint64 b = (ReUint64) RE_Frame_Arena( &frame, thread );

            RE_TEST_CHECK( ( b - a ) >= RE_CACHE_LINE_SIZE );
        }

        RE_Frame_Shutdown( &frame );
    }

    /* --- overflow without a fallback fails rather than spilling silently --- */
    {
        ReFrameAllocator frame;
        RE_TEST_CHECK( RE_Frame_Init( &frame, 2, 1, 64 * 1024 ) );
        RE_Frame_BeginFrame( &frame, 0 );

        RE_TEST_CHECK_NULL( RE_Frame_Alloc( &frame, 0, 1024 * 1024, 16 ) );
        RE_TEST_CHECK_EQ_UINT( RE_Frame_OverflowCount( &frame ), 0 );

        RE_Frame_Shutdown( &frame );
    }

    /* --- with a fallback it degrades instead of failing, but the spill is counted --- */
    {
        ReArena backing;
        RE_TEST_CHECK( RE_Arena_InitVirtual( &backing, 16 * 1024 * 1024 ) );

        ReFrameAllocator frame;
        RE_TEST_CHECK( RE_Frame_Init( &frame, 2, 1, 64 * 1024 ) );
        RE_Frame_SetOverflowAllocator( &frame, RE_Arena_AsAllocator( &backing ) );
        RE_Frame_BeginFrame( &frame, 0 );

        void *spilled = RE_Frame_Alloc( &frame, 0, 1024 * 1024, 16 );

        RE_TEST_CHECK_NOT_NULL( spilled );
        RE_TEST_CHECK_EQ_UINT( RE_Frame_OverflowCount( &frame ), 1 );
        RE_TEST_CHECK_EQ_UINT( RE_Frame_OverflowBytes( &frame ), 1024 * 1024 );

        /* A non-zero count is a bug, not a success - the value of the counter is that it makes an
         * otherwise invisible performance problem visible.
         */
        RE_Frame_Shutdown( &frame );
        RE_Arena_Shutdown( &backing );
    }

    /* --- high-water is the sizing number, so it must survive frame resets --- */
    {
        ReFrameAllocator frame;
        RE_TEST_CHECK( RE_Frame_Init( &frame, 2, 2, 1024 * 1024 ) );

        RE_Frame_BeginFrame( &frame, 0 );
        RE_TEST_CHECK_NOT_NULL( RE_Frame_Alloc( &frame, 0, 4096, 16 ) );

        RE_Frame_BeginFrame( &frame, 1 );
        RE_Frame_BeginFrame( &frame, 2 );

        RE_TEST_CHECK( RE_Frame_HighWater( &frame ) >= 4096 );

        RE_Frame_Shutdown( &frame );
    }

    /* --- degenerate configuration --- */
    {
        ReFrameAllocator frame;

        RE_TEST_CHECK( !RE_Frame_Init( &frame, 0, 4, 1024 ) );
        RE_TEST_CHECK( !RE_Frame_Init( &frame, 2, 0, 1024 ) );
        RE_TEST_CHECK( !RE_Frame_Init( &frame, RE_FRAME_MAX_BUFFERS + 1, 4, 1024 ) );
        RE_TEST_CHECK( !RE_Frame_Init( &frame, 2, RE_FRAME_MAX_THREADS + 1, 1024 ) );
    }
}

/* A stand-in for a GPU fence: completion is whatever the test says it is. */
typedef struct ReRingTestFence
{
    ReUint64 completedValue;
} ReRingTestFence;

RE_INTERNAL ReBool
Ring_TestFenceCompleted( void *context, ReUint64 fenceValue )
{
    ReRingTestFence *fence = (ReRingTestFence *) context;

    return (ReBool) ( fenceValue <= fence->completedValue );
}

void
RE_Test_MemoryRing( void )
{
    ReRingTestFence fence;
    fence.completedValue = 0;

    ReRingAllocator ring;
    RE_TEST_CHECK( RE_Ring_Init( &ring, 64 * 1024, Ring_TestFenceCompleted, &fence ) );

    ReUint64 capacity = ring.capacity;

    /* --- allocations are contiguous and aligned --- */
    {
        void *a = RE_Ring_Alloc( &ring, 256, 256 );
        void *b = RE_Ring_Alloc( &ring, 256, 256 );

        RE_TEST_CHECK_NOT_NULL( a );
        RE_TEST_CHECK_NOT_NULL( b );
        RE_TEST_CHECK( a != b );
        RE_TEST_CHECK_EQ_UINT( (ReUint64) a & 255, 0 );
        RE_TEST_CHECK_EQ_UINT( (ReUint64) b & 255, 0 );
    }

    /* --- memory is not reusable until the fence says the consumer is done --- */
    {
        RE_TEST_CHECK( RE_Ring_Submit( &ring, 1 ) );

        ReUint64 usedBeforeReclaim = RE_Ring_Used( &ring );
        RE_TEST_CHECK( usedBeforeReclaim > 0 );

        /* Fence has not completed, so nothing is reclaimed. */
        RE_Ring_Reclaim( &ring );
        RE_TEST_CHECK_EQ_UINT( RE_Ring_Used( &ring ), usedBeforeReclaim );

        fence.completedValue = 1;
        RE_Ring_Reclaim( &ring );
        RE_TEST_CHECK_EQ_UINT( RE_Ring_Used( &ring ), 0 );
    }

    /* --- overrunning the consumer is refused and counted, not silently overwritten --- */
    {
        fence.completedValue = 1;

        void *large = RE_Ring_Alloc( &ring, capacity - 4096, 16 );
        RE_TEST_CHECK_NOT_NULL( large );
        RE_TEST_CHECK( RE_Ring_Submit( &ring, 2 ) );

        ReUint64 stallsBefore = RE_Ring_StallCount( &ring );

        /* Fence 2 is outstanding, so this cannot fit and must fail rather than clobber memory
         * the GPU is still reading.
         */
        RE_TEST_CHECK_NULL( RE_Ring_Alloc( &ring, 8192, 16 ) );
        RE_TEST_CHECK_EQ_UINT( RE_Ring_StallCount( &ring ), stallsBefore + 1 );

        /* Once the consumer catches up the same request succeeds. */
        fence.completedValue = 2;
        RE_TEST_CHECK_NOT_NULL( RE_Ring_Alloc( &ring, 8192, 16 ) );
    }

    /* --- a request that would straddle the end wraps whole rather than being split --- */
    {
        RE_Ring_Shutdown( &ring );
        RE_TEST_CHECK( RE_Ring_Init( &ring, 64 * 1024, Ring_TestFenceCompleted, &fence ) );
        fence.completedValue = 0;

        ReUint64 chunk = 4096;
        ReUint64 fenceValue = 1;

        /* Walk the head around the buffer several times, retiring as we go, checking every block
         * stays inside the buffer and never straddles the end.
         */
        for ( ReUint32 iteration = 0; iteration < 100; iteration += 1 )
        {
            ReUint8 *block = (ReUint8 *) RE_Ring_Alloc( &ring, chunk, 256 );

            if ( !block )
            {
                fence.completedValue = fenceValue;
                RE_Ring_Reclaim( &ring );
                block = (ReUint8 *) RE_Ring_Alloc( &ring, chunk, 256 );
            }

            RE_TEST_CHECK_NOT_NULL( block );

            if ( block )
            {
                ReUint64 offset = (ReUint64) ( block - ring.base );

                RE_TEST_CHECK( offset + chunk <= ring.capacity );

                /* Writing the whole block proves it is really contiguous and inside the mapping. */
                RE_Memory_Set( block, 0x5A, chunk );
            }

            fenceValue += 1;
            RE_Ring_Submit( &ring, fenceValue );
        }
    }

    /* --- degenerate inputs --- */
    {
        RE_TEST_CHECK_NULL( RE_Ring_Alloc( &ring, 0, 16 ) );
        RE_TEST_CHECK_NULL( RE_Ring_Alloc( &ring, ring.capacity + 1, 16 ) );

        ReRingAllocator bad;
        RE_TEST_CHECK( !RE_Ring_Init( &bad, 0, Ring_TestFenceCompleted, &fence ) );
        RE_TEST_CHECK( !RE_Ring_Init( &bad, 4096, 0, &fence ) );
    }

    RE_Ring_Shutdown( &ring );
}
