/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryThreadCache.h>

#include <assert.h>

#include "RE/Foundation/FoundationMemoryDepot.h"
#include "RE/Foundation/FoundationMemoryHeapInternal.h"
#include "RE/Foundation/FoundationMemoryThreadCacheInternal.h"

#include <RE/Foundation/FoundationAtomic.h>
#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemorySizeClass.h>
#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationSpinLock.h>
#include <RE/Foundation/FoundationThread.h>

/*
    Per class, a thread holds at most two magazines:

      partial - the one being filled or drained right now
      full    - a completed magazine, ready to hand to the depot

    Allocation drains partial; when it runs dry the full one is swapped in, and only when both are
    empty does the thread go to the depot or the heap. Freeing fills partial; when it completes,
    the previous full magazine goes to the depot. That two-magazine hysteresis is what stops a
    thread that allocates and frees around a single boundary from hitting the depot every time.
*/

typedef struct ReThreadCacheClass
{
    ReMagazineNode *partial;
    ReMagazineNode *full;
    ReUint32        partialCount;
} ReThreadCacheClass;

struct ReThreadCache
{
    ReThreadCacheClass classes[RE_HEAP_MAX_CLASSES];

    /* Held continuously by the owning thread, so the fast path never pays to acquire it. Released
     * only around a blocking wait, which is the window a trimmer gets.
     */
    ReSpinLock lock;

    /* The load-bearing guard. Cleared whenever the lock is not held by the owner, so that an
     * allocation made from inside a blocking primitive falls through to the locked heap path
     * instead of racing whoever is draining this cache.
     */
    ReBool inUse;

    ReUint32 localEpoch;

    struct ReThreadCache *next;
};

RE_GLOBAL RE_THREAD_LOCAL ReThreadCache *gThreadCache;

RE_GLOBAL ReSpinLock     gThreadCacheRegistryLock;
RE_GLOBAL ReThreadCache *gThreadCacheRegistry;
RE_GLOBAL ReThreadCache *gThreadCacheFreeList;
RE_GLOBAL ReUint32       gThreadCacheCount;
RE_GLOBAL ReAtomicUint32 gThreadCacheTrimEpoch;

RE_INTERNAL void ThreadCache_FlushLocked( ReThreadCache *cache );

/* Runs on the owning thread as it exits, through the platform's thread-exit hook. */
RE_INTERNAL void
ThreadCache_OnThreadExit( void *userData )
{
    (void) userData;

    RE_Memory_ThreadShutdown();
}

RE_INTERNAL ReThreadCache *
ThreadCache_Create( void )
{
    RE_SpinLock_Acquire( &gThreadCacheRegistryLock );

    ReThreadCache *cache = gThreadCacheFreeList;

    if ( cache )
    {
        gThreadCacheFreeList = cache->next;

        RE_Memory_Zero( cache->classes, sizeof( cache->classes ) );
    }
    else
    {
        /* From the metadata allocator: a thread cache is the heap's own bookkeeping, and
         * allocating it with the heap would recurse on the very first allocation a thread makes.
         */
        cache = (ReThreadCache *) RE_MemoryMetadata_Alloc( sizeof( ReThreadCache ), RE_CACHE_LINE_SIZE );

        if ( !cache )
        {
            RE_SpinLock_Release( &gThreadCacheRegistryLock );

            return 0;
        }
    }

    RE_SpinLock_Init( &cache->lock );

    cache->localEpoch = RE_Atomic_LoadUint32( &gThreadCacheTrimEpoch );
    cache->next       = gThreadCacheRegistry;

    gThreadCacheRegistry = cache;
    gThreadCacheCount   += 1;

    RE_SpinLock_Release( &gThreadCacheRegistryLock );

    /* Taken and held for as long as this thread owns the cache. */
    RE_SpinLock_Acquire( &cache->lock );
    cache->inUse = RE_True;

    gThreadCache = cache;

    RE_Thread_RegisterExitCallback( ThreadCache_OnThreadExit, cache );

    return cache;
}

/* The accessor every fast path goes through. Returns 0 when there is no usable cache, which sends
 * the caller to the locked heap path.
 */
RE_INTERNAL ReThreadCache *
ThreadCache_Get( void )
{
    ReThreadCache *cache = gThreadCache;

    if ( cache && cache->inUse )
    {
        return cache;
    }

    /* A cache exists but is parked: the owner is inside a blocking wait that has itself
     * allocated. Falling through to the locked path is the only safe answer.
     */
    if ( cache )
    {
        return 0;
    }

    return ThreadCache_Create();
}

/* Returns a whole magazine to the heap. */
RE_INTERNAL void
ThreadCache_ReleaseMagazine( ReUint32 classIndex, ReMagazineNode *magazine )
{
    if ( magazine )
    {
        RE_HeapInternal_FreeChain( classIndex, magazine );
    }
}

RE_INTERNAL void
ThreadCache_FlushLocked( ReThreadCache *cache )
{
    ReUint32 classCount = RE_HeapSizeClass_Count();

    for ( ReUint32 classIndex = 0; classIndex < classCount; classIndex += 1 )
    {
        ReThreadCacheClass *slot = &cache->classes[classIndex];

        ThreadCache_ReleaseMagazine( classIndex, slot->partial );
        ThreadCache_ReleaseMagazine( classIndex, slot->full );

        slot->partial      = 0;
        slot->full         = 0;
        slot->partialCount = 0;
    }
}

/* Checked whenever a thread is already going slowly, so a trim costs idle threads nothing and
 * busy threads only notice at a point where they were paying for a lock anyway.
 */
RE_INTERNAL void
ThreadCache_CheckEpoch( ReThreadCache *cache )
{
    ReUint32 epoch = RE_Atomic_LoadUint32( &gThreadCacheTrimEpoch );

    if ( cache->localEpoch != epoch )
    {
        ThreadCache_FlushLocked( cache );
        cache->localEpoch = epoch;
    }
}

void *
RE_ThreadCache_Alloc( ReUint32 classIndex )
{
    ReThreadCache *cache = ThreadCache_Get();

    if ( !cache )
    {
        return 0;
    }

    ReThreadCacheClass *slot = &cache->classes[classIndex];

    /* The common case: no atomics, no lock, just a pop. */
    if ( !slot->partial && slot->full )
    {
        slot->partial      = slot->full;
        slot->full         = 0;
        slot->partialCount = RE_HeapInternal_MagazineCapacity( classIndex );
    }

    if ( slot->partial )
    {
        ReMagazineNode *node = slot->partial;

        slot->partial       = node->next;
        slot->partialCount -= 1;

        return node;
    }

    ThreadCache_CheckEpoch( cache );

    /* Empty. Try the depot before the heap - another thread may have freed exactly what this one
     * needs, and claiming it costs a single compare-exchange.
     */
    ReMagazineNode *magazine = RE_Depot_Pop( classIndex );

    if ( magazine )
    {
        slot->partial      = magazine->next;
        slot->partialCount = RE_HeapInternal_MagazineCapacity( classIndex ) - 1;

        return magazine;
    }

    /* Slow path. Take a batch rather than one bin, so this lock acquisition pays for many
     * allocations to come.
     */
    void    *batch[RE_HEAP_SLOW_PATH_BATCH];
    ReUint32 obtained = RE_HeapInternal_AllocBatch( classIndex, batch, RE_HEAP_SLOW_PATH_BATCH );

    if ( obtained == 0 )
    {
        return 0;
    }

    for ( ReUint32 i = 1; i < obtained; i += 1 )
    {
        ReMagazineNode *node = (ReMagazineNode *) batch[i];

        node->next    = slot->partial;
        slot->partial = node;
    }

    slot->partialCount = obtained - 1;

    return batch[0];
}

ReBool
RE_ThreadCache_Free( ReUint32 classIndex, void *block )
{
    ReThreadCache *cache = ThreadCache_Get();

    if ( !cache )
    {
        return RE_False;
    }

    ReThreadCacheClass *slot     = &cache->classes[classIndex];
    ReUint32            capacity = RE_HeapInternal_MagazineCapacity( classIndex );

    if ( slot->partialCount >= capacity )
    {
        /* partial is complete. Promote it, and hand whatever was already promoted to the depot so
         * another thread can claim it.
         */
        if ( slot->full )
        {
            if ( !RE_Depot_Push( classIndex, slot->full ) )
            {
                /* Depot is full, so this really does go back to the heap. */
                ThreadCache_ReleaseMagazine( classIndex, slot->full );
            }
        }

        slot->full         = slot->partial;
        slot->partial      = 0;
        slot->partialCount = 0;
    }

    ReMagazineNode *node = (ReMagazineNode *) block;

    node->next          = slot->partial;
    slot->partial       = node;
    slot->partialCount += 1;

    return RE_True;
}

ReBool
RE_Memory_ThreadInit( void )
{
    return (ReBool) ( ThreadCache_Get() != 0 );
}

void
RE_Memory_ThreadShutdown( void )
{
    ReThreadCache *cache = gThreadCache;

    if ( !cache )
    {
        return;
    }

    ThreadCache_FlushLocked( cache );

    cache->inUse = RE_False;
    RE_SpinLock_Release( &cache->lock );

    RE_SpinLock_Acquire( &gThreadCacheRegistryLock );

    ReThreadCache **link = &gThreadCacheRegistry;
    while ( *link && *link != cache )
    {
        link = &( *link )->next;
    }

    if ( *link == cache )
    {
        *link = cache->next;
        gThreadCacheCount -= 1;
    }

    /* Recycled rather than released - metadata allocations are permanent by design, and a process
     * that churns threads would otherwise grow its metadata without bound.
     */
    cache->next          = gThreadCacheFreeList;
    gThreadCacheFreeList = cache;

    RE_SpinLock_Release( &gThreadCacheRegistryLock );

    gThreadCache = 0;
}

void
RE_Memory_ThreadBeginBlocking( void )
{
    ReThreadCache *cache = gThreadCache;

    if ( !cache || !cache->inUse )
    {
        return;
    }

    ThreadCache_CheckEpoch( cache );

    cache->inUse = RE_False;
    RE_SpinLock_Release( &cache->lock );
}

void
RE_Memory_ThreadEndBlocking( void )
{
    ReThreadCache *cache = gThreadCache;

    if ( !cache || cache->inUse )
    {
        return;
    }

    RE_SpinLock_Acquire( &cache->lock );
    cache->inUse = RE_True;
}

void
RE_Memory_ThreadCacheTrim( void )
{
    /* The whole cost of asking every thread to flush: one increment. Threads notice at their next
     * checkpoint rather than being interrupted, so idle threads cost nothing.
     */
    RE_Atomic_FetchAddUint32( &gThreadCacheTrimEpoch, 1 );

    /* The calling thread's own cache is flushed here and now. It cannot be picked up by the
     * registry walk below - this thread holds that lock, so the try-acquire is guaranteed to
     * fail - and waiting for its own next checkpoint would mean a trim on a single-threaded
     * program did nothing at all.
     */
    ReThreadCache *own = gThreadCache;

    if ( own && own->inUse )
    {
        ThreadCache_FlushLocked( own );
        own->localEpoch = RE_Atomic_LoadUint32( &gThreadCacheTrimEpoch );
    }

    RE_Depot_Flush();

    /* Caches whose owners happen to be parked can be drained right now. Anything busy is skipped
     * rather than waited on - it will flush itself at its next checkpoint.
     */
    RE_SpinLock_Acquire( &gThreadCacheRegistryLock );

    for ( ReThreadCache *cache = gThreadCacheRegistry; cache; cache = cache->next )
    {
        if ( RE_SpinLock_TryAcquire( &cache->lock ) )
        {
            ThreadCache_FlushLocked( cache );
            cache->localEpoch = RE_Atomic_LoadUint32( &gThreadCacheTrimEpoch );

            RE_SpinLock_Release( &cache->lock );
        }
    }

    RE_SpinLock_Release( &gThreadCacheRegistryLock );
}

ReThreadCacheStats
RE_Memory_ThreadCacheGetStats( void )
{
    ReThreadCacheStats stats;
    RE_Memory_Zero( &stats, sizeof( stats ) );

    ReUint32 classCount = RE_HeapSizeClass_Count();

    RE_SpinLock_Acquire( &gThreadCacheRegistryLock );

    stats.cacheCount = gThreadCacheCount;

    for ( ReThreadCache *cache = gThreadCacheRegistry; cache; cache = cache->next )
    {
        for ( ReUint32 classIndex = 0; classIndex < classCount; classIndex += 1 )
        {
            ReThreadCacheClass *slot    = &cache->classes[classIndex];
            ReUint64            binSize = RE_HeapSizeClass_Size( classIndex );

            ReUint64 bins = slot->partialCount;

            if ( slot->full )
            {
                bins += RE_HeapInternal_MagazineCapacity( classIndex );
            }

            stats.cachedBins  += bins;
            stats.cachedBytes += bins * binSize;
        }
    }

    RE_SpinLock_Release( &gThreadCacheRegistryLock );

    return stats;
}
