/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <RE/Foundation/FoundationAtomic.h>
#include <RE/Foundation/FoundationMemoryHeap.h>
#include <RE/Foundation/FoundationMemoryThreadCache.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

#include "RE/Win64/Win64.h"

#define THREAD_CACHE_TEST_CLASS_SIZE 128
#define THREAD_CACHE_TEST_HANDOFFS   20000

/*
    Producer/consumer: one thread allocates, another frees, and neither ever allocates what it
    frees. Without a depot the producer's freed memory would be stranded in the consumer's cache
    and the producer would fall to the slow path on every single allocation, forever.
*/
typedef struct ReHandoffQueue
{
    ReAtomicPtr    slots[256];
    ReAtomicUint32 produced;
    ReAtomicUint32 consumed;
    ReAtomicUint32 corruptions;
    ReAtomicUint32 producerFailures;
} ReHandoffQueue;

RE_INTERNAL DWORD WINAPI
ThreadCache_TestProducer( LPVOID parameter )
{
    ReHandoffQueue *queue = (ReHandoffQueue *) parameter;

    for ( ReUint32 i = 0; i < THREAD_CACHE_TEST_HANDOFFS; i += 1 )
    {
        ReUint8 *block = (ReUint8 *) RE_Heap_Alloc( THREAD_CACHE_TEST_CLASS_SIZE, 16 );

        if ( !block )
        {
            RE_Atomic_FetchAddUint32( &queue->producerFailures, 1 );
            continue;
        }

        RE_Memory_Set( block, 0xC3, THREAD_CACHE_TEST_CLASS_SIZE );

        /* Spin until a slot frees up rather than dropping the block, so the counts stay exact. */
        for ( ;; )
        {
            ReUint32 slot     = i % 256;
            void    *previous = RE_Atomic_CompareExchangePtr( &queue->slots[slot], 0, block );

            if ( previous == 0 )
            {
                break;
            }

            RE_Thread_Yield();
        }

        RE_Atomic_FetchAddUint32( &queue->produced, 1 );
    }

    return 0;
}

RE_INTERNAL DWORD WINAPI
ThreadCache_TestConsumer( LPVOID parameter )
{
    ReHandoffQueue *queue = (ReHandoffQueue *) parameter;

    ReUint32 taken = 0;

    while ( taken < THREAD_CACHE_TEST_HANDOFFS )
    {
        ReUint32 slot  = taken % 256;
        ReUint8 *block = (ReUint8 *) RE_Atomic_LoadPtr( &queue->slots[slot] );

        if ( !block )
        {
            RE_Thread_Yield();
            continue;
        }

        if ( RE_Atomic_CompareExchangePtr( &queue->slots[slot], block, 0 ) != block )
        {
            continue;
        }

        if ( block[0] != 0xC3 || block[THREAD_CACHE_TEST_CLASS_SIZE - 1] != 0xC3 )
        {
            RE_Atomic_FetchAddUint32( &queue->corruptions, 1 );
        }

        RE_Heap_Free( block );

        taken += 1;
        RE_Atomic_FetchAddUint32( &queue->consumed, 1 );
    }

    return 0;
}

/* Allocates, frees, and exits without ever calling RE_Memory_ThreadShutdown. */
RE_INTERNAL DWORD WINAPI
ThreadCache_TestForgetfulThread( LPVOID parameter )
{
    (void) parameter;

    for ( ReUint32 i = 0; i < 64; i += 1 )
    {
        void *block = RE_Heap_Alloc( 64, 16 );

        if ( block )
        {
            RE_Heap_Free( block );
        }
    }

    return 0;
}

void
RE_Test_MemoryThreadCache( void )
{
    RE_TEST_CHECK( RE_Heap_Init() );

    /* Start from a known state - earlier test groups will have left bins cached. */
    RE_Heap_Trim();

    /* --- freeing parks bins in this thread's cache rather than returning them --- */
    {
        enum { Count = 16 };

        void *blocks[Count];

        for ( ReUint32 i = 0; i < Count; i += 1 )
        {
            blocks[i] = RE_Heap_Alloc( 256, 16 );
            RE_TEST_CHECK_NOT_NULL( blocks[i] );
        }

        for ( ReUint32 i = 0; i < Count; i += 1 )
        {
            RE_Heap_Free( blocks[i] );
        }

        ReThreadCacheStats cached = RE_Memory_ThreadCacheGetStats();

        RE_TEST_CHECK( cached.cacheCount >= 1 );
        RE_TEST_CHECK( cached.cachedBins >= Count );
        RE_TEST_CHECK( cached.cachedBytes >= (ReUint64) Count * 256 );

        /* The most recently freed bin is the one handed back - it is the most likely to still be
         * warm in cache.
         */
        void *reused = RE_Heap_Alloc( 256, 16 );
        RE_TEST_CHECK( reused == blocks[Count - 1] );
        RE_Heap_Free( reused );
    }

    /* --- trim gives them back, including the calling thread's own cache --- */
    {
        RE_Heap_Trim();

        ReThreadCacheStats cached = RE_Memory_ThreadCacheGetStats();

        RE_TEST_CHECK_EQ_UINT( cached.cachedBins, 0 );
        RE_TEST_CHECK_EQ_UINT( cached.cachedBytes, 0 );

        ReHeapStats heap = RE_Heap_GetStats();
        RE_TEST_CHECK_EQ_UINT( heap.smallBytesInUse, 0 );
    }

    /* --- explicit init and shutdown --- */
    {
        RE_TEST_CHECK( RE_Memory_ThreadInit() );
        RE_TEST_CHECK( RE_Memory_ThreadInit() );

        void *block = RE_Heap_Alloc( 64, 16 );
        RE_TEST_CHECK_NOT_NULL( block );
        RE_Heap_Free( block );

        RE_Memory_ThreadShutdown();

        /* Shutdown must have flushed, not leaked. */
        ReThreadCacheStats cached = RE_Memory_ThreadCacheGetStats();
        RE_TEST_CHECK_EQ_UINT( cached.cachedBins, 0 );

        /* And the heap keeps working afterwards - the next allocation just makes a new cache. */
        block = RE_Heap_Alloc( 64, 16 );
        RE_TEST_CHECK_NOT_NULL( block );
        RE_Heap_Free( block );

        RE_Heap_Trim();
    }

    /* --- a thread that exits without saying so is still cleaned up --- */
    {
        ReThreadCacheStats before = RE_Memory_ThreadCacheGetStats();

        HANDLE thread = CreateThread( NULL, 0, ThreadCache_TestForgetfulThread, 0, 0, NULL );
        RE_TEST_CHECK_NOT_NULL( thread );

        if ( thread )
        {
            WaitForSingleObject( thread, INFINITE );
            CloseHandle( thread );
        }

        ReThreadCacheStats after = RE_Memory_ThreadCacheGetStats();

        /* The platform's thread-exit hook ran the teardown. Without it that thread's cache and
         * every bin in it would be unreachable for the life of the process.
         */
        RE_TEST_CHECK_EQ_UINT( after.cacheCount, before.cacheCount );
    }

    /* --- the case the depot exists for --- */
    {
        ReHandoffQueue queue;
        RE_Memory_Zero( &queue, sizeof( queue ) );

        HANDLE producer = CreateThread( NULL, 0, ThreadCache_TestProducer, &queue, 0, NULL );
        HANDLE consumer = CreateThread( NULL, 0, ThreadCache_TestConsumer, &queue, 0, NULL );

        RE_TEST_CHECK_NOT_NULL( producer );
        RE_TEST_CHECK_NOT_NULL( consumer );

        if ( producer && consumer )
        {
            HANDLE both[2] = { producer, consumer };

            WaitForMultipleObjects( 2, both, TRUE, INFINITE );

            CloseHandle( producer );
            CloseHandle( consumer );

            RE_TEST_CHECK_EQ_UINT( RE_Atomic_LoadUint32( &queue.produced ), THREAD_CACHE_TEST_HANDOFFS );
            RE_TEST_CHECK_EQ_UINT( RE_Atomic_LoadUint32( &queue.consumed ), THREAD_CACHE_TEST_HANDOFFS );

            /* Every block still held the producer's fill when the consumer got it. */
            RE_TEST_CHECK_EQ_UINT( RE_Atomic_LoadUint32( &queue.corruptions ), 0 );

            /* And the producer never ran out - memory the consumer freed made it back around,
             * which is precisely what the depot is for.
             */
            RE_TEST_CHECK_EQ_UINT( RE_Atomic_LoadUint32( &queue.producerFailures ), 0 );
        }
    }

    /* --- everything handed across threads is accounted for once trimmed --- */
    {
        RE_Heap_Trim();

        ReHeapStats heap = RE_Heap_GetStats();

        RE_TEST_CHECK_EQ_UINT( heap.smallBytesInUse, 0 );
    }

    /* --- the blocking guard: a parked cache must not serve allocations --- */
    {
        RE_TEST_CHECK( RE_Memory_ThreadInit() );

        void *warm = RE_Heap_Alloc( 128, 16 );
        RE_Heap_Free( warm );

        RE_Memory_ThreadBeginBlocking();

        /* This is what a blocking primitive allocating on its own behalf looks like. It has to
         * work, and it has to come from the locked heap path rather than the parked cache.
         */
        void *whileParked = RE_Heap_Alloc( 128, 16 );
        RE_TEST_CHECK_NOT_NULL( whileParked );

        /* The cache is parked, so the bin cannot have come out of it. */
        RE_TEST_CHECK( whileParked != warm );

        RE_Heap_Free( whileParked );

        RE_Memory_ThreadEndBlocking();

        /* Back in use, and serving from the cache again. */
        void *afterResume = RE_Heap_Alloc( 128, 16 );
        RE_TEST_CHECK_NOT_NULL( afterResume );
        RE_Heap_Free( afterResume );

        RE_Heap_Trim();
    }
}
