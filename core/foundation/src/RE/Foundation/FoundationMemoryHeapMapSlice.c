/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationMemoryHeapInternal.h"

#if RE_HEAP_MAP_STRATEGY == RE_HEAP_MAP_STRATEGY_ADDRESS_SLICE

#include <assert.h>

#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationSpinLock.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

/*
    Strategy B. One enormous reservation, split into a fixed slice per size class, so the class of
    a pointer is implied by where it sits:

        class = ( pointer - base ) / sliceSize

    There is no table and no header. The free path is a subtract and a shift on a value already in
    a register, which is as cheap as the operation gets.

    What it costs is address space - tens of gigabytes reserved, though nothing is committed until
    used - and a hard per-class ceiling. A class that fills its slice cannot borrow from another,
    so exhaustion is handled explicitly by the heap rather than pretended away.
*/

typedef struct ReSliceClassState
{
    /* Spans are handed out by bumping an index and never move, so descriptors can be a flat array
     * indexed by span index rather than anything associative.
     */
    ReHeapSpan *descriptors;
    ReUint64    descriptorsCommitted; /* entries backed by committed memory */

    ReUint64 spanSize;
    ReUint64 spanCapacity; /* spans that fit in the slice */
    ReUint64 spansUsed;    /* bump cursor */

    ReHeapSpan *freeSpans; /* released spans, ready to be handed out again */
} ReSliceClassState;

RE_GLOBAL ReSpinLock         gSliceLock;
RE_GLOBAL ReVirtualRegion    gSliceRegion;
RE_GLOBAL ReVirtualRegion    gSliceDescriptorRegion;
RE_GLOBAL ReSliceClassState *gSliceClasses;
RE_GLOBAL ReUint32           gSliceClassCount;
RE_GLOBAL ReBool             gSliceInitialized;

RE_INTERNAL ReUint8 *
Slice_BaseOfClass( ReUint32 classIndex )
{
    return (ReUint8 *) gSliceRegion.base + ( (ReUint64) classIndex * RE_HEAP_SLICE_SIZE );
}

/* Descriptor arrays live in their own reservation, committed as span indices are reached. Spans
 * are handed out in increasing order, so this only ever grows at the tail.
 */
RE_INTERNAL ReBool
Slice_EnsureDescriptors( ReSliceClassState *state, ReUint32 classIndex, ReUint64 requiredCount )
{
    if ( requiredCount <= state->descriptorsCommitted )
    {
        return RE_True;
    }

    ReUint64 perClassBytes = RE_HEAP_SLICE_SIZE / RE_HEAP_BLOCK_SIZE * sizeof( ReHeapSpan );
    ReUint64 classOffset   = (ReUint64) classIndex * perClassBytes;

    ReUint64 requiredBytes = RE_Memory_AlignUp( requiredCount * sizeof( ReHeapSpan ), 64 * 1024 );
    ReUint64 haveBytes     = state->descriptorsCommitted * sizeof( ReHeapSpan );

    if ( requiredBytes > perClassBytes )
    {
        requiredBytes = perClassBytes;
    }

    if ( !RE_VirtualMemory_Commit( &gSliceDescriptorRegion, classOffset + haveBytes,
             requiredBytes - haveBytes ) )
    {
        return RE_False;
    }

    state->descriptorsCommitted = requiredBytes / sizeof( ReHeapSpan );

    return (ReBool) ( state->descriptorsCommitted >= requiredCount );
}

ReBool
RE_HeapMap_Init( void )
{
    if ( gSliceInitialized )
    {
        return RE_True;
    }

    RE_SpinLock_Init( &gSliceLock );

    if ( !RE_HeapSizeClass_Init() )
    {
        return RE_False;
    }

    gSliceClassCount = RE_HeapSizeClass_Count();

    /* The whole point of the strategy: one contiguous range, so a subtract and a divide recover
     * the class. Reserved only - nothing here is committed until a span is actually carved.
     */
    gSliceRegion = RE_VirtualMemory_Reserve( (ReUint64) gSliceClassCount * RE_HEAP_SLICE_SIZE,
        RE_HEAP_BLOCK_SIZE );

    if ( !gSliceRegion.base )
    {
        return RE_False;
    }

    ReUint64 perClassBytes = RE_HEAP_SLICE_SIZE / RE_HEAP_BLOCK_SIZE * sizeof( ReHeapSpan );

    gSliceDescriptorRegion = RE_VirtualMemory_Reserve( (ReUint64) gSliceClassCount * perClassBytes, 0 );
    if ( !gSliceDescriptorRegion.base )
    {
        RE_VirtualMemory_Release( &gSliceRegion );

        return RE_False;
    }

    gSliceClasses = (ReSliceClassState *) RE_MemoryMetadata_Alloc(
        gSliceClassCount * sizeof( ReSliceClassState ), 64 );

    if ( !gSliceClasses )
    {
        RE_VirtualMemory_Release( &gSliceDescriptorRegion );
        RE_VirtualMemory_Release( &gSliceRegion );

        return RE_False;
    }

    for ( ReUint32 i = 0; i < gSliceClassCount; i += 1 )
    {
        ReSliceClassState *state = &gSliceClasses[i];

        state->spanSize     = RE_HeapSizeClass_SpanSize( i );
        state->spanCapacity = RE_HEAP_SLICE_SIZE / state->spanSize;
        state->descriptors  = (ReHeapSpan *) ( (ReUint8 *) gSliceDescriptorRegion.base + i * perClassBytes );
    }

    gSliceInitialized = RE_True;

    return RE_True;
}

void
RE_HeapMap_Shutdown( void )
{
    if ( !gSliceInitialized )
    {
        return;
    }

    RE_SpinLock_Acquire( &gSliceLock );

    RE_VirtualMemory_Release( &gSliceDescriptorRegion );
    RE_VirtualMemory_Release( &gSliceRegion );

    gSliceClasses     = 0;
    gSliceInitialized = RE_False;

    RE_SpinLock_Release( &gSliceLock );
}

ReHeapSpan *
RE_HeapMap_AcquireSpan( ReUint32 classIndex )
{
    assert( gSliceInitialized && classIndex < gSliceClassCount );

    ReSliceClassState *state = &gSliceClasses[classIndex];

    RE_SpinLock_Acquire( &gSliceLock );

    ReHeapSpan *span = state->freeSpans;

    if ( span )
    {
        state->freeSpans = span->next;
    }
    else
    {
        /* Slice exhausted. There is no borrowing from another class - that is the trade this
         * strategy makes - so the heap is told and handles it by promoting or falling through to
         * the large path.
         */
        if ( state->spansUsed >= state->spanCapacity )
        {
            RE_SpinLock_Release( &gSliceLock );

            return 0;
        }

        ReUint64 spanIndex = state->spansUsed;

        if ( !Slice_EnsureDescriptors( state, classIndex, spanIndex + 1 ) )
        {
            RE_SpinLock_Release( &gSliceLock );

            return 0;
        }

        ReUint8 *memory = Slice_BaseOfClass( classIndex ) + spanIndex * state->spanSize;

        if ( !RE_VirtualMemory_Commit( &gSliceRegion,
                 (ReUint64) ( memory - (ReUint8 *) gSliceRegion.base ), state->spanSize ) )
        {
            RE_SpinLock_Release( &gSliceLock );

            return 0;
        }

        span       = &state->descriptors[spanIndex];
        span->base = memory;

        state->spansUsed += 1;
    }

    span->next       = 0;
    span->prev       = 0;
    span->classIndex = classIndex;
    span->binCount   = RE_HeapSizeClass_BinsPerSpan( classIndex );
    span->binsInUse  = 0;
    span->freeRun    = 0;
    span->canary     = RE_HEAP_SPAN_CANARY;

    ReHeapFreeRun *run = (ReHeapFreeRun *) span->base;
    run->nextRun   = RE_HEAP_BIN_NONE;
    run->runLength = span->binCount;

    RE_SpinLock_Release( &gSliceLock );

    return span;
}

void
RE_HeapMap_ReleaseSpan( ReHeapSpan *span )
{
    assert( span && span->canary == RE_HEAP_SPAN_CANARY );

    ReSliceClassState *state = &gSliceClasses[span->classIndex];

    RE_SpinLock_Acquire( &gSliceLock );

    span->next       = state->freeSpans;
    state->freeSpans = span;

    RE_SpinLock_Release( &gSliceLock );
}

ReHeapSpan *
RE_HeapMap_SpanOf( const void *ptr )
{
    ReUint64 address = (ReUint64) ptr;
    ReUint64 base    = (ReUint64) gSliceRegion.base;

    /* One unsigned subtract answers both "is this ours" and "which class" - an address below the
     * base wraps to something enormous and fails the range check, so no separate test is needed.
     */
    ReUint64 offset = address - base;

    if ( offset >= gSliceRegion.size )
    {
        return 0;
    }

    ReUint32 classIndex = (ReUint32) ( offset / RE_HEAP_SLICE_SIZE );

    ReSliceClassState *state = &gSliceClasses[classIndex];

    ReUint64 offsetInSlice = offset - ( (ReUint64) classIndex * RE_HEAP_SLICE_SIZE );
    ReUint64 spanIndex     = offsetInSlice / state->spanSize;

    /* Inside the slice but past what has been carved: not a live allocation. */
    if ( spanIndex >= state->spansUsed )
    {
        return 0;
    }

    return &state->descriptors[spanIndex];
}

ReUint64
RE_HeapMap_ReservedBytes( void )
{
    return gSliceRegion.size + gSliceDescriptorRegion.size;
}

#endif /* RE_HEAP_MAP_STRATEGY == RE_HEAP_MAP_STRATEGY_ADDRESS_SLICE */
