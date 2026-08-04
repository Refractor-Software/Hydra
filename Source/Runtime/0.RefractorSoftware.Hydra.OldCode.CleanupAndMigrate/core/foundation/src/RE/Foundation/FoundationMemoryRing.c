/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryRing.h>

#include <assert.h>

#include <RE/Foundation/FoundationMemoryAllocator.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

/*
    head and tail are monotonically increasing absolute counters, mapped into the buffer with a
    modulo. That removes the classic ring ambiguity where head == tail means either empty or full:
    used bytes are simply head - tail, and the two only coincide when the ring is genuinely empty.

    At 64 bits these counters cannot realistically wrap - a ring uploading a gigabyte per frame at
    60 Hz would take about ten thousand years to get there.
*/

ReBool
RE_Ring_Init( ReRingAllocator *ring, ReUint64 capacity, ReRingFenceCompletedFn fenceCompleted,
    void *fenceContext )
{
    if ( !ring || capacity == 0 || !fenceCompleted )
    {
        return RE_False;
    }

    RE_Memory_Zero( ring, sizeof( *ring ) );

    ReVirtualRegion region = RE_VirtualMemory_Reserve( capacity, 0 );
    if ( !region.base )
    {
        return RE_False;
    }

    if ( !RE_VirtualMemory_Commit( &region, 0, region.size ) )
    {
        RE_VirtualMemory_Release( &region );

        return RE_False;
    }

    ring->region         = region;
    ring->base           = (ReUint8 *) region.base;
    ring->capacity       = region.size;
    ring->fenceCompleted = fenceCompleted;
    ring->fenceContext   = fenceContext;

    return RE_True;
}

void
RE_Ring_Shutdown( ReRingAllocator *ring )
{
    if ( !ring )
    {
        return;
    }

    RE_VirtualMemory_Release( &ring->region );
    RE_Memory_Zero( ring, sizeof( *ring ) );
}

void
RE_Ring_Reclaim( ReRingAllocator *ring )
{
    assert( ring );

    /* Oldest first, and stop at the first incomplete one - fences complete in submission order,
     * so a later completion cannot make an earlier submission safe to reclaim.
     */
    while ( ring->pendingCount > 0 )
    {
        ReRingPending *oldest = &ring->pending[ring->pendingHead];

        if ( !ring->fenceCompleted( ring->fenceContext, oldest->fenceValue ) )
        {
            break;
        }

        ring->tail        = oldest->offset;
        ring->pendingHead = ( ring->pendingHead + 1 ) % RE_RING_MAX_PENDING;
        ring->pendingCount -= 1;
    }
}

void *
RE_Ring_Alloc( ReRingAllocator *ring, ReUint64 size, ReUint64 alignment )
{
    assert( ring && ring->base );

    if ( alignment == 0 )
    {
        alignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    assert( ( alignment & ( alignment - 1 ) ) == 0 && "alignment must be a power of two" );

    if ( size == 0 || size > ring->capacity )
    {
        return 0;
    }

    RE_Ring_Reclaim( ring );

    ReUint64 offset  = ring->head % ring->capacity;
    ReUint64 aligned = RE_Memory_AlignUp( offset, alignment );
    ReUint64 padding = aligned - offset;

    /* A block that would run off the end is pushed to the start of the next lap instead of being
     * split. Callers hand these pointers to a GPU, which needs one contiguous range.
     */
    if ( aligned + size > ring->capacity )
    {
        padding = ring->capacity - offset;
    }

    ReUint64 needed = padding + size;

    if ( ( ring->head + needed ) - ring->tail > ring->capacity )
    {
        /* The consumer has not caught up. Refusing is the honest answer - the caller decides
         * whether to stall on the fence or spill elsewhere, and either way this counter is what
         * says the ring is undersized.
         */
        ring->stallCount += 1;

        return 0;
    }

    void *block = ring->base + ( ( ring->head + padding ) % ring->capacity );

    ring->head += needed;

    ReUint64 used = ring->head - ring->tail;
    if ( used > ring->highWater )
    {
        ring->highWater = used;
    }

    return block;
}

ReBool
RE_Ring_Submit( ReRingAllocator *ring, ReUint64 fenceValue )
{
    assert( ring );

    if ( ring->pendingCount == RE_RING_MAX_PENDING )
    {
        /* Too many submissions outstanding. Reclaiming first is the caller's cheapest fix; if
         * that frees nothing then the consumer is genuinely that far behind.
         */
        RE_Ring_Reclaim( ring );

        if ( ring->pendingCount == RE_RING_MAX_PENDING )
        {
            return RE_False;
        }
    }

    ReUint32 slot = ( ring->pendingHead + ring->pendingCount ) % RE_RING_MAX_PENDING;

    ring->pending[slot].offset     = ring->head;
    ring->pending[slot].fenceValue = fenceValue;
    ring->pendingCount            += 1;

    return RE_True;
}

ReUint64
RE_Ring_Used( const ReRingAllocator *ring )
{
    return ring->head - ring->tail;
}
