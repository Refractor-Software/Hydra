/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationMemoryThreadCache.h

    The tier that makes the heap scale. Each thread keeps a small cache of free bins per size
    class, so the overwhelming majority of allocations touch no lock and no atomic at all - the
    hot path is a pop from a thread-local list.

    Bins move between threads in whole magazines through the depot, which is what stops a
    producer/consumer pattern from stranding memory in the producer's cache forever.

    Threads do not have to call anything: the cache is created the first time a thread allocates,
    and destroyed automatically when the thread exits. The explicit calls below exist for code
    that wants to control when that happens - a job system creating a pool of workers would rather
    pay the setup cost during bootstrap than on the first allocation of the first job.
*/

/* Creates this thread's cache now rather than on first use. Idempotent. */
ReBool RE_Memory_ThreadInit( void );

/* Flushes this thread's cached bins back to the heap and destroys the cache.
 *
 * Called automatically at thread exit. Calling it explicitly is only needed to reclaim the memory
 * earlier than that.
 */
void RE_Memory_ThreadShutdown( void );

/*
    The pair below is for a thread that is about to park - waiting on a job queue, blocking on
    I/O - and exists so a trimming thread can reclaim an idle thread's cache.

    A thread normally holds its own cache lock continuously, so the fast path never pays for
    acquiring it. Releasing it around a blocking wait is what gives anyone else a chance to take
    it.

    @warning The blocking primitive itself may allocate. A thread that allocates while parked must
             not touch its own cache, or it races whoever is draining it - so between these two
             calls the cache reports itself unavailable and allocations fall through to the locked
             path. That guard is not optional; without it the result is a rare and extremely
             unpleasant heap corruption.
*/
void RE_Memory_ThreadBeginBlocking( void );
void RE_Memory_ThreadEndBlocking( void );

/* Asks every thread to flush its cache, and empties the depot.
 *
 * Costs one atomic increment. Threads notice at their next natural checkpoint and flush
 * themselves rather than being interrupted, so idle threads cost nothing and no thread is
 * disturbed mid-allocation. Caches whose owners happen to be parked are drained immediately.
 */
void RE_Memory_ThreadCacheTrim( void );

typedef struct ReThreadCacheStats
{
    ReUint64 cacheCount;    /* live per-thread caches */
    ReUint64 cachedBins;    /* bins parked across every cache */
    ReUint64 cachedBytes;
} ReThreadCacheStats;

ReThreadCacheStats RE_Memory_ThreadCacheGetStats( void );
