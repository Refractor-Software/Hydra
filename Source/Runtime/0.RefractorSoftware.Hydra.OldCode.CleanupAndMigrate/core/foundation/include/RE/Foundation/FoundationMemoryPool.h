/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationMemoryAllocator.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

/*
    FoundationMemoryPool.h

    Many objects of one size, with individually unpredictable lifetimes. The arena's complement:
    an arena handles mixed sizes with collective free, a pool handles one size with individual
    free. They are near-opposites and frequently sit side by side.

    Both allocation and deallocation are O(1) with no search, no size lookup, and no per-allocation
    metadata - strictly better than a general-purpose allocator can manage, entirely because the
    size is known in advance. The free list lives inside the free slots themselves, since free
    memory is by definition not being used for anything else.

    A pool is essentially one size class of the general-purpose heap with the hard part removed:
    the heap's complexity comes from supporting many classes at once and having to *discover*
    which one a pointer belongs to. A pool already knows.

    @threadsafe No. One instance per thread, or external synchronisation. Adding a lock here would
                cost more than the two or three instructions it guards.
*/

typedef enum RePoolKind
{
    /* Borrows a caller-supplied buffer; never grows. */
    RePoolKind_Fixed,

    /* Reserves address space for the maximum slot count and commits as it grows. Growth without
     * ever relocating, and the storage stays one contiguous array.
     */
    RePoolKind_Virtual,
} RePoolKind;

typedef struct RePool
{
    ReUint8 *base;

    /* Real distance between slots. At least large enough to hold the free-list link, and rounded
     * up to the slot alignment, so it can exceed the requested slot size.
     */
    ReUint64 slotStride;

    ReUint64 slotCapacity;    /* total slots the pool can ever hold */
    ReUint64 slotsCommitted;  /* slots backed by committed memory */
    ReUint64 slotsInitialized;/* bump cursor: slots never yet handed out */
    ReUint64 slotsInUse;
    ReUint64 highWater;

    /* Head of the intrusive free list, threaded through recycled slots only. Slots that have
     * never been used are covered by slotsInitialized instead, so initialisation is O(1) rather
     * than walking every slot to build a list up front.
     */
    void *freeHead;

    ReVirtualRegion region;
    RePoolKind      kind;
} RePool;

/* slotSize is what the caller wants per object; slotAlignment its required alignment (0 for the
 * default). The real stride may be larger - query RE_Pool_SlotStride if it matters.
 *
 * Returns RE_False if the buffer cannot hold even one slot.
 */
ReBool RE_Pool_InitFixed( RePool *pool, void *memory, ReUint64 size, ReUint64 slotSize,
    ReUint64 slotAlignment );

/* Reserves for maxSlots and commits incrementally. The reservation costs address space, not
 * memory, so maxSlots can be a generous ceiling rather than an estimate.
 */
ReBool RE_Pool_InitVirtual( RePool *pool, ReUint64 maxSlots, ReUint64 slotSize,
    ReUint64 slotAlignment );

void RE_Pool_Shutdown( RePool *pool );

/* Returns 0 when the pool is exhausted - an expected, checkable condition, not a programmer
 * error.
 */
void *RE_Pool_Alloc( RePool *pool );

/* @warning Asserts if the pointer does not belong to this pool, i.e. out of range or not on a
 *          slot boundary. That catches a foreign pointer and the misaligned shapes of a
 *          double-free cheaply, but it cannot detect freeing the same valid slot twice - use
 *          the handle pool where that matters.
 */
void RE_Pool_Free( RePool *pool, void *slot );

/* Returns every slot to the free state in O(1). Does not decommit. */
void RE_Pool_Reset( RePool *pool );

RE_ALWAYS_INLINE_HINT ReUint64
RE_Pool_SlotStride( const RePool *pool )
{
    return pool->slotStride;
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Pool_Count( const RePool *pool )
{
    return pool->slotsInUse;
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Pool_Capacity( const RePool *pool )
{
    return pool->slotCapacity;
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Pool_HighWater( const RePool *pool )
{
    return pool->highWater;
}

/* Presents the pool through the uniform interface. Requests larger than the slot size return 0,
 * since a pool cannot satisfy them; through a generic interface that is indistinguishable from
 * being out of memory, which is the correct thing for the caller to see either way.
 */
ReAllocator RE_Pool_AsAllocator( RePool *pool );

/*
    Deliberately no live-slot iteration here. Free and live slots are indistinguishable without
    extra state, and making every pool pay for an occupancy bitmap to serve the subset that
    iterates would be the wrong default. Use the handle pool, which tracks liveness because it
    needs it for handle validation anyway.
*/

/* Written over a freed slot in development builds, after the free-list link. */
#define RE_POOL_POISON_BYTE 0xDD
