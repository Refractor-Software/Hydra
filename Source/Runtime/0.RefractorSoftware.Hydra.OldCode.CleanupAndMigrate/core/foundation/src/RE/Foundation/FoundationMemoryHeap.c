/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryHeap.h>

#include <assert.h>

#include "RE/Foundation/FoundationMemoryDepot.h"
#include "RE/Foundation/FoundationMemoryHeapInternal.h"
#include "RE/Foundation/FoundationMemoryThreadCacheInternal.h"

#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemoryThreadCache.h>
#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationSpinLock.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

/* ------------------------------------------------------------------------------------------- */
/* Per-class state                                                                              */
/* ------------------------------------------------------------------------------------------- */

/*
    One lock per size class, never one for the whole heap. With around sixty classes there are
    sixty independent locks, and since different subsystems tend to allocate different sizes, real
    contention drops sharply. Padded to a cache line so two classes' locks do not ping-pong
    between cores.
*/
typedef struct ReHeapClassState
{
    ReSpinLock  lock;
    ReHeapSpan *partialSpans; /* spans with at least one free bin */
    ReUint64    binsInUse;
    ReUint64    spansCommitted;
} ReHeapClassState;

#define RE_HEAP_CLASS_SLOT_SIZE \
    ( RE_CACHE_LINE_SIZE * ( ( sizeof( ReHeapClassState ) + RE_CACHE_LINE_SIZE - 1 ) / RE_CACHE_LINE_SIZE ) )

typedef union ReHeapClassSlot
{
    ReHeapClassState state;
    ReUint8          padding[RE_HEAP_CLASS_SLOT_SIZE];
} ReHeapClassSlot;

/* ------------------------------------------------------------------------------------------- */
/* Large allocations                                                                            */
/* ------------------------------------------------------------------------------------------- */

/*
    Anything above the largest size class goes straight to the virtual memory layer, with its
    record in a chained hash keyed by the pointer.

    Both the requested and the committed size are kept. Their difference is the measured overhead
    of the large path, and the slack is what lets a grow-in-place realloc succeed without touching
    the OS.
*/

#define RE_HEAP_LARGE_BUCKETS 1024

typedef struct ReHeapLargeRecord
{
    void                     *base;
    ReUint64                  requestedBytes;
    ReUint64                  committedBytes;
    ReVirtualRegion           region;
    struct ReHeapLargeRecord *next;
} ReHeapLargeRecord;

RE_GLOBAL ReSpinLock          gHeapLargeLock;
RE_GLOBAL ReHeapLargeRecord **gHeapLargeBuckets;
RE_GLOBAL ReHeapLargeRecord  *gHeapLargeFreeRecords;
RE_GLOBAL ReUint64            gHeapLargeBytesRequested;
RE_GLOBAL ReUint64            gHeapLargeBytesCommitted;
RE_GLOBAL ReUint64            gHeapLargeCount;

RE_GLOBAL ReHeapClassSlot *gHeapClasses;
RE_GLOBAL ReUint32         gHeapClassCount;
RE_GLOBAL ReBool           gHeapInitialized;

RE_INTERNAL ReUint64
Heap_LargeBucketOf( const void *ptr )
{
    /* Pointers are page-aligned here, so the low bits carry nothing. Shifting them off first
     * keeps the buckets from clustering.
     */
    ReUint64 value = (ReUint64) ptr >> 12;

    /* Fibonacci hashing: multiply by 2^64 / golden ratio and take the high bits. Cheap, and it
     * spreads sequential page addresses far better than a mask would.
     */
    return ( value * 11400714819323198485ull ) >> ( 64 - 10 );
}

RE_INTERNAL ReHeapLargeRecord *
Heap_LargeFind( const void *ptr )
{
    ReHeapLargeRecord *record = gHeapLargeBuckets[Heap_LargeBucketOf( ptr )];

    while ( record )
    {
        if ( record->base == ptr )
        {
            return record;
        }

        record = record->next;
    }

    return 0;
}

RE_INTERNAL void *
Heap_LargeAlloc( ReUint64 size, ReUint64 alignment )
{
    ReVirtualRegion region = RE_VirtualMemory_Reserve( size, alignment );
    if ( !region.base )
    {
        return 0;
    }

    if ( !RE_VirtualMemory_Commit( &region, 0, region.size ) )
    {
        RE_VirtualMemory_Release( &region );

        return 0;
    }

    RE_SpinLock_Acquire( &gHeapLargeLock );

    ReHeapLargeRecord *record = gHeapLargeFreeRecords;
    if ( record )
    {
        gHeapLargeFreeRecords = record->next;
    }
    else
    {
        record = (ReHeapLargeRecord *) RE_MemoryMetadata_Alloc( sizeof( ReHeapLargeRecord ), 16 );
    }

    if ( !record )
    {
        RE_SpinLock_Release( &gHeapLargeLock );
        RE_VirtualMemory_Release( &region );

        return 0;
    }

    record->base           = region.base;
    record->requestedBytes = size;
    record->committedBytes = region.size;
    record->region         = region;

    ReUint64 bucket = Heap_LargeBucketOf( region.base );
    record->next            = gHeapLargeBuckets[bucket];
    gHeapLargeBuckets[bucket] = record;

    gHeapLargeBytesRequested += size;
    gHeapLargeBytesCommitted += region.size;
    gHeapLargeCount          += 1;

    RE_SpinLock_Release( &gHeapLargeLock );

    return region.base;
}

RE_INTERNAL ReBool
Heap_LargeFree( void *ptr )
{
    RE_SpinLock_Acquire( &gHeapLargeLock );

    ReUint64            bucket   = Heap_LargeBucketOf( ptr );
    ReHeapLargeRecord **link     = &gHeapLargeBuckets[bucket];
    ReHeapLargeRecord  *record   = *link;

    while ( record && record->base != ptr )
    {
        link   = &record->next;
        record = record->next;
    }

    if ( !record )
    {
        RE_SpinLock_Release( &gHeapLargeLock );

        return RE_False;
    }

    *link = record->next;

    gHeapLargeBytesRequested -= record->requestedBytes;
    gHeapLargeBytesCommitted -= record->committedBytes;
    gHeapLargeCount          -= 1;

    ReVirtualRegion region = record->region;

    record->next          = gHeapLargeFreeRecords;
    gHeapLargeFreeRecords = record;

    RE_SpinLock_Release( &gHeapLargeLock );

    RE_VirtualMemory_Release( &region );

    return RE_True;
}

/* ------------------------------------------------------------------------------------------- */
/* Small path                                                                                   */
/* ------------------------------------------------------------------------------------------- */

RE_INTERNAL ReUint8 *
Heap_BinAt( ReHeapSpan *span, ReUint32 binIndex, ReUint64 binSize )
{
    return span->base + ( (ReUint64) binIndex * binSize );
}

RE_INTERNAL void
Heap_PartialPush( ReHeapClassState *state, ReHeapSpan *span )
{
    span->prev = 0;
    span->next = state->partialSpans;

    if ( state->partialSpans )
    {
        state->partialSpans->prev = span;
    }

    state->partialSpans = span;
}

RE_INTERNAL void
Heap_PartialRemove( ReHeapClassState *state, ReHeapSpan *span )
{
    if ( span->prev )
    {
        span->prev->next = span->next;
    }
    else if ( state->partialSpans == span )
    {
        state->partialSpans = span->next;
    }

    if ( span->next )
    {
        span->next->prev = span->prev;
    }

    span->next = 0;
    span->prev = 0;
}

/* Caller holds the class lock. The span must have a free bin. */
RE_INTERNAL void *
Heap_TakeBin( ReHeapSpan *span, ReUint64 binSize )
{
    ReUint32 binIndex = span->freeRun;

    assert( binIndex != RE_HEAP_BIN_NONE );

    ReHeapFreeRun *run = (ReHeapFreeRun *) Heap_BinAt( span, binIndex, binSize );

    if ( run->runLength > 1 )
    {
        /* Split the run: the bin after this one becomes the head of what remains. This is what
         * makes a freshly carved span O(1) to hand out from - it starts as a single run covering
         * every bin, and shrinks by one each time.
         */
        ReUint32       nextIndex = binIndex + 1;
        ReHeapFreeRun *nextRun   = (ReHeapFreeRun *) Heap_BinAt( span, nextIndex, binSize );

        nextRun->runLength = run->runLength - 1;
        nextRun->nextRun   = run->nextRun;

        span->freeRun = nextIndex;
    }
    else
    {
        span->freeRun = run->nextRun;
    }

    span->binsInUse += 1;

    return Heap_BinAt( span, binIndex, binSize );
}

/* The class that serves this request, accounting for alignment.
 *
 * Over-aligned requests are promoted to a class whose *size* is a multiple of the alignment.
 * Because bins are laid out contiguously from a span base that is itself heavily aligned, a bin
 * whose size is a multiple of A always sits at an address that is a multiple of A - size
 * divisibility buys address alignment for free.
 *
 * The naive alternative, bouncing anything over-aligned to a page allocation, is catastrophic: a
 * 16-byte request wanting 128-byte alignment would consume a whole page.
 */
RE_INTERNAL ReUint32
Heap_ClassFor( ReUint64 size, ReUint64 alignment )
{
    if ( alignment > RE_HEAP_MAX_PROMOTABLE_ALIGNMENT )
    {
        return RE_HEAP_CLASS_LARGE;
    }

    /* The search runs for the default alignment too, not just for over-aligned requests. Not
     * every class size is a multiple of 16 - the smallest is 8, and the snapping step leaves a
     * few others at 8-byte multiples - so skipping the check for default-aligned requests hands
     * back an 8-aligned bin to a caller promised 16.
     */
    ReUint64 aligned = RE_Memory_AlignUp( size, alignment );

    ReUint32 classIndex = RE_HeapSizeClass_Of( aligned );
    if ( classIndex == RE_HEAP_CLASS_LARGE )
    {
        return RE_HEAP_CLASS_LARGE;
    }

    ReUint32 count = RE_HeapSizeClass_Count();

    /* Nearly always exits on the first iteration, since most classes are multiples of 16 and 16
     * is what almost everything asks for.
     */
    while ( classIndex < count )
    {
        if ( ( RE_HeapSizeClass_Size( classIndex ) % alignment ) == 0 )
        {
            return classIndex;
        }

        classIndex += 1;
    }

    return RE_HEAP_CLASS_LARGE;
}

RE_INTERNAL void *
Heap_SmallAlloc( ReUint32 classIndex )
{
    ReHeapClassState *state   = &gHeapClasses[classIndex].state;
    ReUint64          binSize = RE_HeapSizeClass_Size( classIndex );

    RE_SpinLock_Acquire( &state->lock );

    ReHeapSpan *span = state->partialSpans;

    if ( !span )
    {
        RE_SpinLock_Release( &state->lock );

        /* Acquired outside the class lock: the map takes its own lock, and holding two at once
         * in one order here and the other order elsewhere is how deadlocks are built.
         */
        span = RE_HeapMap_AcquireSpan( classIndex );

        if ( !span )
        {
            return 0;
        }

        RE_SpinLock_Acquire( &state->lock );

        Heap_PartialPush( state, span );
        state->spansCommitted += 1;
    }

    void *block = Heap_TakeBin( span, binSize );

    if ( span->freeRun == RE_HEAP_BIN_NONE )
    {
        Heap_PartialRemove( state, span );
    }

    state->binsInUse += 1;

    RE_SpinLock_Release( &state->lock );

    return block;
}

/* Caller holds the class lock. Returns the span if it emptied and should be released, else 0.
 *
 * Split out from RE_Heap_Free so a whole magazine can be returned under one lock acquisition
 * rather than one per bin.
 */
RE_INTERNAL ReHeapSpan *
Heap_SmallFreeLocked( ReHeapClassState *state, ReHeapSpan *span, void *block, ReUint64 binSize )
{
    ReUint64 offset = (ReUint64) ( (ReUint8 *) block - span->base );

    assert( offset % binSize == 0 && "pointer is not on a bin boundary" );
    assert( span->binsInUse > 0 && "freeing into a span with nothing allocated" );

    ReUint32 binIndex = (ReUint32) ( offset / binSize );

    ReBool spanWasFull = (ReBool) ( span->freeRun == RE_HEAP_BIN_NONE );

    ReHeapFreeRun *run = (ReHeapFreeRun *) block;
    run->runLength = 1;
    run->nextRun   = span->freeRun;
    span->freeRun  = binIndex;

    span->binsInUse -= 1;
    state->binsInUse -= 1;

    if ( spanWasFull )
    {
        Heap_PartialPush( state, span );
    }

    if ( span->binsInUse == 0 )
    {
        Heap_PartialRemove( state, span );
        state->spansCommitted -= 1;

        return span;
    }

    return 0;
}

RE_INTERNAL void
Heap_SmallFree( ReHeapSpan *span, void *block )
{
    ReUint32          classIndex = span->classIndex;
    ReHeapClassState *state      = &gHeapClasses[classIndex].state;
    ReUint64          binSize    = RE_HeapSizeClass_Size( classIndex );

    RE_SpinLock_Acquire( &state->lock );

    ReHeapSpan *emptied = Heap_SmallFreeLocked( state, span, block, binSize );

    RE_SpinLock_Release( &state->lock );

    /* Released outside the class lock, same ordering reason as acquisition. The map parks the
     * span for reuse rather than handing it back to the OS, so a workload oscillating around a
     * span boundary does not pay a syscall each way.
     */
    if ( emptied )
    {
        RE_HeapMap_ReleaseSpan( emptied );
    }
}

ReUint32
RE_HeapInternal_MagazineCapacity( ReUint32 classIndex )
{
    ReUint64 binSize  = RE_HeapSizeClass_Size( classIndex );
    ReUint64 capacity = RE_MAGAZINE_MAX_BYTES / binSize;

    if ( capacity > RE_MAGAZINE_MAX_COUNT )
    {
        capacity = RE_MAGAZINE_MAX_COUNT;
    }

    if ( capacity == 0 )
    {
        capacity = 1;
    }

    return (ReUint32) capacity;
}

ReUint32
RE_HeapInternal_AllocBatch( ReUint32 classIndex, void **outBlocks, ReUint32 maxCount )
{
    ReHeapClassState *state   = &gHeapClasses[classIndex].state;
    ReUint64          binSize = RE_HeapSizeClass_Size( classIndex );

    ReUint32 obtained = 0;

    while ( obtained < maxCount )
    {
        RE_SpinLock_Acquire( &state->lock );

        ReHeapSpan *span = state->partialSpans;

        if ( !span )
        {
            RE_SpinLock_Release( &state->lock );

            /* Acquired outside the class lock: the map takes its own, and holding both in one
             * order here and the other order elsewhere is how deadlocks get built.
             */
            span = RE_HeapMap_AcquireSpan( classIndex );

            if ( !span )
            {
                break;
            }

            RE_SpinLock_Acquire( &state->lock );

            Heap_PartialPush( state, span );
            state->spansCommitted += 1;
        }

        /* Drain this span for as much of the batch as it can serve, so one lock acquisition pays
         * for many bins rather than one.
         */
        while ( obtained < maxCount && span->freeRun != RE_HEAP_BIN_NONE )
        {
            outBlocks[obtained] = Heap_TakeBin( span, binSize );
            obtained += 1;
            state->binsInUse += 1;
        }

        if ( span->freeRun == RE_HEAP_BIN_NONE )
        {
            Heap_PartialRemove( state, span );
        }

        RE_SpinLock_Release( &state->lock );
    }

    return obtained;
}

void
RE_HeapInternal_FreeChain( ReUint32 classIndex, ReMagazineNode *head )
{
    ReHeapClassState *state   = &gHeapClasses[classIndex].state;
    ReUint64          binSize = RE_HeapSizeClass_Size( classIndex );

    /* Spans that empty are collected and released after the lock is dropped, for the same
     * ordering reason as everywhere else.
     */
    ReHeapSpan *emptied[RE_MAGAZINE_MAX_COUNT];
    ReUint32    emptiedCount = 0;

    RE_SpinLock_Acquire( &state->lock );

    while ( head )
    {
        /* The next link has to be read before the block is freed - freeing overwrites the first
         * bytes of the bin with the free-run node, which is the same memory the link lives in.
         */
        ReMagazineNode *next = head->next;

        ReHeapSpan *span = RE_HeapMap_SpanOf( head );

        assert( span && span->canary == RE_HEAP_SPAN_CANARY );
        assert( span->classIndex == classIndex && "magazine holds a bin from another class" );

        ReHeapSpan *justEmptied = Heap_SmallFreeLocked( state, span, head, binSize );

        if ( justEmptied && emptiedCount < RE_MAGAZINE_MAX_COUNT )
        {
            emptied[emptiedCount] = justEmptied;
            emptiedCount += 1;
        }

        head = next;
    }

    RE_SpinLock_Release( &state->lock );

    for ( ReUint32 i = 0; i < emptiedCount; i += 1 )
    {
        RE_HeapMap_ReleaseSpan( emptied[i] );
    }
}

/* ------------------------------------------------------------------------------------------- */
/* Public interface                                                                             */
/* ------------------------------------------------------------------------------------------- */

ReBool
RE_Heap_Init( void )
{
    if ( gHeapInitialized )
    {
        return RE_True;
    }

    if ( !RE_HeapSizeClass_Init() )
    {
        return RE_False;
    }

    if ( !RE_HeapMap_Init() )
    {
        return RE_False;
    }

    RE_SpinLock_Init( &gHeapLargeLock );

    gHeapClassCount = RE_HeapSizeClass_Count();

    gHeapClasses = (ReHeapClassSlot *) RE_MemoryMetadata_Alloc(
        gHeapClassCount * sizeof( ReHeapClassSlot ), RE_CACHE_LINE_SIZE );

    if ( !gHeapClasses )
    {
        return RE_False;
    }

    for ( ReUint32 i = 0; i < gHeapClassCount; i += 1 )
    {
        RE_SpinLock_Init( &gHeapClasses[i].state.lock );
    }

    gHeapLargeBuckets = (ReHeapLargeRecord **) RE_MemoryMetadata_Alloc(
        RE_HEAP_LARGE_BUCKETS * sizeof( ReHeapLargeRecord * ), 64 );

    if ( !gHeapLargeBuckets )
    {
        return RE_False;
    }

    if ( !RE_Depot_Init() )
    {
        return RE_False;
    }

    gHeapInitialized = RE_True;

    return RE_True;
}

void
RE_Heap_Shutdown( void )
{
    if ( !gHeapInitialized )
    {
        return;
    }

    /* Cached bins have to go back to their spans before anything is released, or the spans they
     * belong to would still look occupied.
     */
    RE_Memory_ThreadShutdown();
    RE_Memory_ThreadCacheTrim();

    RE_SpinLock_Acquire( &gHeapLargeLock );

    for ( ReUint64 bucket = 0; bucket < RE_HEAP_LARGE_BUCKETS; bucket += 1 )
    {
        ReHeapLargeRecord *record = gHeapLargeBuckets[bucket];

        while ( record )
        {
            ReHeapLargeRecord *next   = record->next;
            ReVirtualRegion    region = record->region;

            RE_VirtualMemory_Release( &region );

            record = next;
        }

        gHeapLargeBuckets[bucket] = 0;
    }

    gHeapLargeBytesRequested = 0;
    gHeapLargeBytesCommitted = 0;
    gHeapLargeCount          = 0;
    gHeapLargeFreeRecords    = 0;

    RE_SpinLock_Release( &gHeapLargeLock );

    RE_HeapMap_Shutdown();

    gHeapClasses     = 0;
    gHeapInitialized = RE_False;
}

void *
RE_Heap_Alloc( ReUint64 size, ReUint64 alignment )
{
    assert( gHeapInitialized && "RE_Heap_Alloc before RE_Heap_Init" );

    if ( size == 0 )
    {
        return 0;
    }

    if ( alignment == 0 )
    {
        alignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    assert( ( alignment & ( alignment - 1 ) ) == 0 && "alignment must be a power of two" );

    ReUint32 classIndex = Heap_ClassFor( size, alignment );

    if ( classIndex != RE_HEAP_CLASS_LARGE )
    {
        /* Through the thread cache first, which serves the overwhelming majority of calls with no
         * lock and no atomic. It falls back to the batched slow path internally, and returns 0
         * only when there is no usable cache at all.
         */
        void *block = RE_ThreadCache_Alloc( classIndex );

        if ( !block )
        {
            block = Heap_SmallAlloc( classIndex );
        }

        if ( block )
        {
            return block;
        }

        /* The small path could not be served. Under the address-slice strategy that means this
         * class filled its slice, which is a real and expected outcome rather than an error - so
         * fall through to the large path rather than failing.
         */
    }

    return Heap_LargeAlloc( size, alignment );
}

void
RE_Heap_Free( void *block )
{
    if ( !block )
    {
        return;
    }

    assert( gHeapInitialized );

    ReHeapSpan *span = RE_HeapMap_SpanOf( block );

    if ( span )
    {
        assert( span->canary == RE_HEAP_SPAN_CANARY && "span metadata corrupted" );

        /* Parked in this thread's cache rather than returned to the span, so the next allocation
         * of this class on this thread is a pop rather than a lock. Freeing to a thread other
         * than the one that allocated is fine - the depot is what moves it back.
         */
        if ( !RE_ThreadCache_Free( span->classIndex, block ) )
        {
            Heap_SmallFree( span, block );
        }

        return;
    }

    if ( Heap_LargeFree( block ) )
    {
        return;
    }

    /* Not a small bin and not a large record. Either it was never ours or it has already been
     * freed - both are programmer errors, and reporting loudly here beats limping on with a heap
     * that is already wrong.
     */
    assert( 0 && "RE_Heap_Free called with a pointer this heap did not hand out" );
}

ReUint64
RE_Heap_AllocationSize( const void *block )
{
    if ( !block )
    {
        return 0;
    }

    ReHeapSpan *span = RE_HeapMap_SpanOf( block );

    if ( span )
    {
        return RE_HeapSizeClass_Size( span->classIndex );
    }

    RE_SpinLock_Acquire( &gHeapLargeLock );

    ReHeapLargeRecord *record = Heap_LargeFind( block );
    ReUint64           size   = record ? record->requestedBytes : 0;

    RE_SpinLock_Release( &gHeapLargeLock );

    return size;
}

ReUint64
RE_Heap_Quantize( ReUint64 size, ReUint64 alignment )
{
    if ( size == 0 )
    {
        return 0;
    }

    if ( alignment == 0 )
    {
        alignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    ReUint32 classIndex = Heap_ClassFor( size, alignment );

    if ( classIndex != RE_HEAP_CLASS_LARGE )
    {
        return RE_HeapSizeClass_Size( classIndex );
    }

    /* The large path rounds to a page, and the caller may as well have the remainder. */
    return RE_Memory_AlignUp( size, RE_VirtualMemory_CommitGranularity() );
}

void *
RE_Heap_Realloc( void *block, ReUint64 newSize, ReUint64 alignment )
{
    if ( !block )
    {
        return RE_Heap_Alloc( newSize, alignment );
    }

    if ( newSize == 0 )
    {
        RE_Heap_Free( block );

        return 0;
    }

    if ( alignment == 0 )
    {
        alignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    ReHeapSpan *span = RE_HeapMap_SpanOf( block );

    if ( span )
    {
        ReUint32 classIndex  = span->classIndex;
        ReUint64 currentSize = RE_HeapSizeClass_Size( classIndex );

        ReBool stillFits    = (ReBool) ( newSize <= currentSize );
        ReBool stillAligned = (ReBool) ( ( (ReUint64) block & ( alignment - 1 ) ) == 0 );

        /* The clause people leave out, and the one that makes shrinking actually work. Without
         * it a buffer that drops from 4000 bytes to 100 sits in the 4096 bin forever; with it the
         * shrink migrates down a class and hands the difference back.
         */
        ReBool wouldNotFitSmaller = (ReBool) ( newSize > RE_HeapSizeClass_SizeBelow( classIndex ) );

        if ( stillFits && stillAligned && wouldNotFitSmaller )
        {
            return block;
        }

        void *moved = RE_Heap_Alloc( newSize, alignment );
        if ( !moved )
        {
            return 0;
        }

        RE_Memory_Copy( moved, block, ( newSize < currentSize ) ? newSize : currentSize );
        RE_Heap_Free( block );

        return moved;
    }

    RE_SpinLock_Acquire( &gHeapLargeLock );

    ReHeapLargeRecord *record       = Heap_LargeFind( block );
    ReUint64           committed    = record ? record->committedBytes : 0;
    ReUint64           requested    = record ? record->requestedBytes : 0;
    ReBool             resizedHere  = RE_False;

    if ( record )
    {
        ReUint64 pageSize = RE_VirtualMemory_CommitGranularity();

        /* The slack between what was asked for and what the OS actually gave absorbs a resize
         * without touching the OS - but only while the block still needs every committed page,
         * or a large shrink would sit on memory it no longer uses.
         */
        if ( newSize <= committed && RE_Memory_AlignUp( newSize, pageSize ) >= committed )
        {
            gHeapLargeBytesRequested -= record->requestedBytes;
            record->requestedBytes    = newSize;
            gHeapLargeBytesRequested += newSize;

            resizedHere = RE_True;
        }
    }

    RE_SpinLock_Release( &gHeapLargeLock );

    if ( resizedHere )
    {
        return block;
    }

    assert( record && "RE_Heap_Realloc called with a pointer this heap did not hand out" );

    if ( !record )
    {
        return 0;
    }

    void *moved = RE_Heap_Alloc( newSize, alignment );
    if ( !moved )
    {
        return 0;
    }

    RE_Memory_Copy( moved, block, ( newSize < requested ) ? newSize : requested );
    RE_Heap_Free( block );

    return moved;
}

void
RE_Heap_Trim( void )
{
    /* Asks every thread to give back its cached bins and empties the depot. Costs one atomic;
     * threads notice at their own next checkpoint rather than being interrupted.
     */
    RE_Memory_ThreadCacheTrim();

    /* Spans that empty as a result are handed back to the map, which parks them for reuse.
     * Decommitting those parked spans is the next step and wants hysteresis, or a workload
     * oscillating around a span boundary would thrash on page faults.
     */
}

ReHeapStats
RE_Heap_GetStats( void )
{
    ReHeapStats stats;
    RE_Memory_Zero( &stats, sizeof( stats ) );

    if ( !gHeapInitialized )
    {
        return stats;
    }

    for ( ReUint32 i = 0; i < gHeapClassCount; i += 1 )
    {
        ReHeapClassState *state = &gHeapClasses[i].state;

        RE_SpinLock_Acquire( &state->lock );

        stats.smallBytesInUse     += state->binsInUse * RE_HeapSizeClass_Size( i );
        stats.smallBytesCommitted += state->spansCommitted * RE_HeapSizeClass_SpanSize( i );
        stats.smallSpansCommitted += state->spansCommitted;

        RE_SpinLock_Release( &state->lock );
    }

    RE_SpinLock_Acquire( &gHeapLargeLock );

    stats.largeBytesRequested = gHeapLargeBytesRequested;
    stats.largeBytesCommitted = gHeapLargeBytesCommitted;
    stats.largeCount          = gHeapLargeCount;

    RE_SpinLock_Release( &gHeapLargeLock );

    stats.mapReservedBytes = RE_HeapMap_ReservedBytes();
    stats.metadataBytes    = RE_MemoryMetadata_GetStats().bytesCommitted;

    return stats;
}

/* ------------------------------------------------------------------------------------------- */
/* Uniform interface                                                                            */
/* ------------------------------------------------------------------------------------------- */

RE_INTERNAL void *
Heap_AllocatorAlloc( void *context, ReUint64 size, ReUint64 alignment )
{
    (void) context;

    return RE_Heap_Alloc( size, alignment );
}

RE_INTERNAL void *
Heap_AllocatorRealloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    (void) context;
    (void) oldSize; /* recoverable from the pointer; accepted so arenas can stay header-free */

    return RE_Heap_Realloc( block, newSize, alignment );
}

RE_INTERNAL void
Heap_AllocatorFree( void *context, void *block, ReUint64 oldSize )
{
    (void) context;
    (void) oldSize;

    RE_Heap_Free( block );
}

RE_INTERNAL ReUint64
Heap_AllocatorQuantize( void *context, ReUint64 size, ReUint64 alignment )
{
    (void) context;

    return RE_Heap_Quantize( size, alignment );
}

ReAllocator
RE_Heap_AsAllocator( void )
{
    ReAllocator allocator;
    allocator.context  = 0;
    allocator.alloc    = Heap_AllocatorAlloc;
    allocator.realloc  = Heap_AllocatorRealloc;
    allocator.free     = Heap_AllocatorFree;
    allocator.quantize = Heap_AllocatorQuantize;

    /* Load-bearing. This heap locks per size class internally; wrapping it in a global locking
     * proxy would erase that and serialise every allocation in the engine.
     */
    allocator.isInternallyThreadSafe = RE_True;

    return allocator;
}
