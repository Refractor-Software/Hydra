/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

/*
    FoundationMemoryRing.h

    Memory whose lifetime is bounded but not frame-aligned. The canonical case is GPU upload: a
    constant buffer written this frame is consumed by the GPU some frames later, and the memory
    cannot be reused until the GPU is actually finished with it.

    A frame allocator cannot express that, because its reclamation is driven by the CPU frame
    boundary while this is driven by a fence completing.

    Fence completion is supplied as a callback rather than being known here. Foundation has no
    business knowing what a D3D12 fence is, and the same ring serves any async producer whose
    completion can be tested.

    @threadsafe No. Owned by whichever thread submits work against it.
*/

/* Returns whether everything up to fenceValue has completed. Called on the allocation path, so it
 * should be a cheap query - reading a fence's completed value, not waiting on one.
 */
typedef ReBool ( *ReRingFenceCompletedFn )( void *context, ReUint64 fenceValue );

/* Outstanding submissions, oldest first. One entry per submit, so the ring can reclaim in the
 * order work actually finishes.
 */
#define RE_RING_MAX_PENDING 64

typedef struct ReRingPending
{
    ReUint64 offset;     /* head position at submission - everything below this is covered */
    ReUint64 fenceValue;
} ReRingPending;

typedef struct ReRingAllocator
{
    ReUint8 *base;
    ReUint64 capacity;

    ReUint64 head; /* next write position */
    ReUint64 tail; /* oldest position still potentially in use by the consumer */

    ReRingPending pending[RE_RING_MAX_PENDING];
    ReUint32      pendingHead;
    ReUint32      pendingCount;

    ReRingFenceCompletedFn fenceCompleted;
    void                  *fenceContext;

    ReUint64 highWater;
    ReUint64 stallCount; /* allocations refused because the consumer had not caught up */

    ReVirtualRegion region;
} ReRingAllocator;

/* The whole capacity is committed up front: a ring is sized to its steady-state working set and
 * wraps through all of it continuously, so lazy commit would only defer inevitable page faults
 * into the middle of a frame.
 */
ReBool RE_Ring_Init( ReRingAllocator *ring, ReUint64 capacity, ReRingFenceCompletedFn fenceCompleted,
    void *fenceContext );

void RE_Ring_Shutdown( ReRingAllocator *ring );

/* Returns 0 when the consumer has not freed enough room yet.
 *
 * The caller decides what that means: stalling until the fence catches up is correct but shows up
 * as a hitch, while spilling to another allocator is smoother but hides the undersizing. Either
 * way it should be logged, and the ring sized from the observed high-water mark.
 *
 * @warning Allocations never straddle the end of the buffer - a request that would run off the
 *          end wraps to offset 0 instead, so the returned block is always contiguous.
 */
void *RE_Ring_Alloc( ReRingAllocator *ring, ReUint64 size, ReUint64 alignment );

/* Marks everything allocated since the last submit as belonging to fenceValue. Once the fence
 * reports completion, that space is reclaimed.
 *
 * Fence granularity is a real trade: one fence per frame is cheap and coarse, one per submit
 * reclaims sooner but costs more tracking. Per-frame is the usual starting point.
 */
ReBool RE_Ring_Submit( ReRingAllocator *ring, ReUint64 fenceValue );

/* Advances the tail past everything the consumer has finished with. Called automatically by
 * RE_Ring_Alloc; exposed for callers that want to reclaim at a known-quiet moment.
 */
void RE_Ring_Reclaim( ReRingAllocator *ring );

ReUint64 RE_Ring_Used( const ReRingAllocator *ring );

RE_ALWAYS_INLINE_HINT ReUint64
RE_Ring_HighWater( const ReRingAllocator *ring )
{
    return ring->highWater;
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Ring_StallCount( const ReRingAllocator *ring )
{
    return ring->stallCount;
}
