/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <RE/Foundation/FoundationMemoryHeap.h>
#include <RE/Foundation/FoundationMemorySizeClass.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

/* Threads are spawned directly; the job system does not exist yet. */
#include "RE/Win64/Win64.h"

#define HEAP_TEST_THREADS            8
#define HEAP_TEST_ALLOCS_PER_THREAD  4000

typedef struct ReHeapTestThreadArgs
{
    ReUint32 seed;
    ReUint32 mismatches;
} ReHeapTestThreadArgs;

/* Cheap deterministic noise so each thread walks a different size mix. */
RE_INTERNAL ReUint32
Heap_TestNextRandom( ReUint32 *state )
{
    ReUint32 value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;

    *state = value;

    return value;
}

RE_INTERNAL DWORD WINAPI
Heap_TestWorker( LPVOID parameter )
{
    ReHeapTestThreadArgs *args = (ReHeapTestThreadArgs *) parameter;

    ReUint32 random = args->seed;

    void    *live[64]     = {0};
    ReUint64 liveSizes[64] = {0};

    for ( ReUint32 i = 0; i < HEAP_TEST_ALLOCS_PER_THREAD; i += 1 )
    {
        ReUint32 slot = Heap_TestNextRandom( &random ) % 64;

        if ( live[slot] )
        {
            /* Verify the block still holds this thread's own fill before releasing it. Any
             * overlap between threads or classes shows up right here.
             */
            ReUint8 *bytes    = (ReUint8 *) live[slot];
            ReUint8  expected = (ReUint8) ( args->seed & 0xFF );

            if ( bytes[0] != expected || bytes[liveSizes[slot] - 1] != expected )
            {
                args->mismatches += 1;
            }

            RE_Heap_Free( live[slot] );
            live[slot] = 0;
        }
        else
        {
            /* A spread that straddles the small/large boundary, so both paths get exercised
             * concurrently rather than one at a time.
             */
            ReUint64 size = 1 + ( Heap_TestNextRandom( &random ) % 40000 );

            void *block = RE_Heap_Alloc( size, 16 );

            if ( block )
            {
                RE_Memory_Set( block, (ReUint8) ( args->seed & 0xFF ), size );

                live[slot]      = block;
                liveSizes[slot] = size;
            }
        }
    }

    for ( ReUint32 slot = 0; slot < 64; slot += 1 )
    {
        if ( live[slot] )
        {
            RE_Heap_Free( live[slot] );
        }
    }

    return 0;
}

void
RE_Test_MemoryHeap( void )
{
    RE_TEST_CHECK( RE_Heap_Init() );
    RE_TEST_CHECK( RE_Heap_Init() );

    /* --- small allocations are distinct, aligned, and writable end to end --- */
    {
        enum { Count = 512 };

        void    *blocks[Count];
        ReUint64 sizes[Count];

        for ( ReUint32 i = 0; i < Count; i += 1 )
        {
            sizes[i]  = 1 + ( i * 7 ) % 4000;
            blocks[i] = RE_Heap_Alloc( sizes[i], 16 );

            RE_TEST_CHECK_NOT_NULL( blocks[i] );

            if ( blocks[i] )
            {
                RE_TEST_CHECK_EQ_UINT( (ReUint64) blocks[i] & 15, 0 );
                RE_Memory_Set( blocks[i], (ReUint8) ( i & 0xFF ), sizes[i] );
            }
        }

        /* Overlap between any two would have clobbered an earlier fill. */
        for ( ReUint32 i = 0; i < Count; i += 1 )
        {
            if ( blocks[i] )
            {
                ReUint8 *bytes = (ReUint8 *) blocks[i];

                RE_TEST_CHECK_EQ_UINT( bytes[0], (ReUint8) ( i & 0xFF ) );
                RE_TEST_CHECK_EQ_UINT( bytes[sizes[i] - 1], (ReUint8) ( i & 0xFF ) );

                /* Never under-delivers. */
                RE_TEST_CHECK( RE_Heap_AllocationSize( blocks[i] ) >= sizes[i] );
            }
        }

        for ( ReUint32 i = 0; i < Count; i += 1 )
        {
            RE_Heap_Free( blocks[i] );
        }
    }

    /* --- freed bins come back, which is the whole point of a free list --- */
    {
        void *first = RE_Heap_Alloc( 64, 16 );
        RE_Heap_Free( first );

        void *second = RE_Heap_Alloc( 64, 16 );
        RE_TEST_CHECK( second == first );

        RE_Heap_Free( second );
    }

    /* --- a span emptied and re-filled hands out the same memory again --- */
    {
        enum { Count = 64 };

        void *blocks[Count];

        for ( ReUint32 i = 0; i < Count; i += 1 )
        {
            blocks[i] = RE_Heap_Alloc( 128, 16 );
        }

        void *firstAddress = blocks[0];

        for ( ReUint32 i = 0; i < Count; i += 1 )
        {
            RE_Heap_Free( blocks[i] );
        }

        /* The span went empty and was released to the map, which parks rather than unmaps it -
         * so the next allocation of that class should land in the same place.
         */
        void *reused = RE_Heap_Alloc( 128, 16 );
        RE_TEST_CHECK( reused == firstAddress );

        RE_Heap_Free( reused );
    }

    /* --- large allocations --- */
    {
        ReUint64 largeSize = RE_HEAP_MAX_SMALL_SIZE * 4;

        void *large = RE_Heap_Alloc( largeSize, 16 );
        RE_TEST_CHECK_NOT_NULL( large );

        if ( large )
        {
            RE_Memory_Set( large, 0x77, largeSize );
            RE_TEST_CHECK_EQ_UINT( ( (ReUint8 *) large )[largeSize - 1], 0x77 );
            RE_TEST_CHECK_EQ_UINT( RE_Heap_AllocationSize( large ), largeSize );
        }

        ReHeapStats stats = RE_Heap_GetStats();

        RE_TEST_CHECK( stats.largeCount >= 1 );
        RE_TEST_CHECK( stats.largeBytesCommitted >= stats.largeBytesRequested );

        RE_Heap_Free( large );
    }

    /* --- over-aligned requests are promoted into a bin, not bounced to a page --- */
    {
        for ( ReUint64 alignment = 32; alignment <= RE_HEAP_MAX_PROMOTABLE_ALIGNMENT; alignment *= 2 )
        {
            void *block = RE_Heap_Alloc( 16, alignment );

            RE_TEST_CHECK_NOT_NULL( block );

            if ( block )
            {
                RE_TEST_CHECK_EQ_UINT( (ReUint64) block & ( alignment - 1 ), 0 );

                /* A 16-byte request at 128-byte alignment must not have consumed a whole page -
                 * that is a 256x blowup and the reason promotion exists.
                 */
                RE_TEST_CHECK( RE_Heap_AllocationSize( block ) <= RE_HEAP_MAX_SMALL_SIZE );

                RE_Heap_Free( block );
            }
        }
    }

    /* --- realloc, including the shrink clause most implementations omit --- */
    {
        ReUint8 *block = (ReUint8 *) RE_Heap_Alloc( 4000, 16 );
        RE_TEST_CHECK_NOT_NULL( block );
        RE_Memory_Set( block, 0x5C, 4000 );

        ReUint64 sizeAt4000 = RE_Heap_AllocationSize( block );

        /* Growing within the same class must not move the block. */
        ReUint8 *grownSlightly = (ReUint8 *) RE_Heap_Realloc( block, sizeAt4000, 16 );
        RE_TEST_CHECK( grownSlightly == block );

        /* Shrinking far enough has to migrate down a class and hand the difference back.
         * Without the "would it fit a smaller class" test this silently keeps the 4000-byte bin
         * forever.
         */
        ReUint8 *shrunk = (ReUint8 *) RE_Heap_Realloc( grownSlightly, 100, 16 );
        RE_TEST_CHECK_NOT_NULL( shrunk );
        RE_TEST_CHECK( RE_Heap_AllocationSize( shrunk ) < sizeAt4000 );
        RE_TEST_CHECK_EQ_UINT( shrunk[0], 0x5C );
        RE_TEST_CHECK_EQ_UINT( shrunk[99], 0x5C );

        /* Growing past the class moves it, and the contents come along. */
        ReUint8 *grown = (ReUint8 *) RE_Heap_Realloc( shrunk, 9000, 16 );
        RE_TEST_CHECK_NOT_NULL( grown );
        RE_TEST_CHECK_EQ_UINT( grown[0], 0x5C );
        RE_TEST_CHECK_EQ_UINT( grown[99], 0x5C );

        /* Small to large. */
        ReUint8 *huge = (ReUint8 *) RE_Heap_Realloc( grown, 512 * 1024, 16 );
        RE_TEST_CHECK_NOT_NULL( huge );
        RE_TEST_CHECK_EQ_UINT( huge[0], 0x5C );

        /* And large back down to small. */
        ReUint8 *backDown = (ReUint8 *) RE_Heap_Realloc( huge, 64, 16 );
        RE_TEST_CHECK_NOT_NULL( backDown );
        RE_TEST_CHECK_EQ_UINT( backDown[0], 0x5C );

        RE_TEST_CHECK_NULL( RE_Heap_Realloc( backDown, 0, 16 ) );

        /* From null behaves as a plain allocation. */
        void *fromNull = RE_Heap_Realloc( 0, 128, 16 );
        RE_TEST_CHECK_NOT_NULL( fromNull );
        RE_Heap_Free( fromNull );
    }

    /* --- quantize agrees with what an allocation actually gets --- */
    {
        for ( ReUint64 size = 1; size <= RE_HEAP_MAX_SMALL_SIZE; size += 37 )
        {
            ReUint64 promised = RE_Heap_Quantize( size, 16 );

            void *block = RE_Heap_Alloc( size, 16 );
            RE_TEST_CHECK_NOT_NULL( block );

            if ( block )
            {
                RE_TEST_CHECK_EQ_UINT( RE_Heap_AllocationSize( block ), promised );
                RE_Heap_Free( block );
            }
        }
    }

    /* --- the uniform interface --- */
    {
        ReAllocator allocator = RE_Heap_AsAllocator();

        /* Must be advertised as internally synchronised, or a decorator chain would wrap it in a
         * global lock and undo the per-class locking entirely.
         */
        RE_TEST_CHECK( allocator.isInternallyThreadSafe );

        void *block = RE_Memory_Alloc( &allocator, 200, 16 );
        RE_TEST_CHECK_NOT_NULL( block );
        RE_Memory_Free( &allocator, block, 200 );
    }

    /* --- degenerate inputs --- */
    {
        RE_TEST_CHECK_NULL( RE_Heap_Alloc( 0, 16 ) );
        RE_Heap_Free( 0 );
        RE_TEST_CHECK_EQ_UINT( RE_Heap_AllocationSize( 0 ), 0 );
    }

    /* --- concurrent churn across every class, which is what the per-class locks are for --- */
    {
        HANDLE               threads[HEAP_TEST_THREADS];
        ReHeapTestThreadArgs args[HEAP_TEST_THREADS];
        ReUint32             started = 0;

        for ( ReUint32 i = 0; i < HEAP_TEST_THREADS; i += 1 )
        {
            args[i].seed       = 0x1234567u + i * 2654435761u;
            args[i].mismatches = 0;

            threads[i] = CreateThread( NULL, 0, Heap_TestWorker, &args[i], 0, NULL );

            if ( threads[i] )
            {
                started += 1;
            }
        }

        RE_TEST_CHECK_EQ_UINT( started, HEAP_TEST_THREADS );

        WaitForMultipleObjects( started, threads, TRUE, INFINITE );

        for ( ReUint32 i = 0; i < started; i += 1 )
        {
            CloseHandle( threads[i] );

            /* A non-zero count means two threads were handed overlapping memory. */
            RE_TEST_CHECK_EQ_UINT( args[i].mismatches, 0 );
        }
    }

    /* --- after everything is freed, nothing should still be counted as in use --- */
    {
        ReHeapStats stats = RE_Heap_GetStats();

        RE_TEST_CHECK_EQ_UINT( stats.largeCount, 0 );
        RE_TEST_CHECK_EQ_UINT( stats.largeBytesRequested, 0 );
        RE_TEST_CHECK_EQ_UINT( stats.smallBytesInUse, 0 );

        /* Metadata is real memory and has to be reported, or it shows up later as an unexplained
         * gap between our numbers and the OS's.
         */
        RE_TEST_CHECK( stats.metadataBytes > 0 );
    }
}
