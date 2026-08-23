/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationMemoryPool.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationMemoryHandlePool.h

    A pool that hands out handles instead of pointers.

    The bug this exists to remove: something frees a slot, the slot is reused for a different
    object, and a stale pointer elsewhere now reads a valid-looking but entirely wrong object. No
    crash, no corruption, just wrong behaviour - and it is miserable to track down.

    A per-slot generation counter, bumped on free, invalidates every outstanding handle to that
    slot. A stale access then resolves to null at the point of use, which turns an invisible
    correctness bug into an immediate, local, debuggable one.

    Handles are also relocatable and serialisable in a way pointers are not: outside code holds an
    index, so the storage is free to move, and a 32-bit handle is a stable value that can be saved
    or sent over a network.

    @threadsafe No. Same contract as RePool.
*/

/*
    Handle layout: index in the low bits, generation in the high bits.

    The generation width is a wrap-around trade, and worth setting deliberately rather than
    inheriting. 8 bits means a slot must be recycled 256 times before a stale handle can collide
    with a live one; for most engine objects that is plenty, but a high-churn pool (particles, say)
    can burn through it in seconds. Wrap-around is made improbable here, not eliminated.
*/
#define RE_HANDLE_INDEX_BITS      24
#define RE_HANDLE_GENERATION_BITS 8

#define RE_HANDLE_INDEX_MASK      ( ( 1u << RE_HANDLE_INDEX_BITS ) - 1u )
#define RE_HANDLE_GENERATION_MASK ( ( 1u << RE_HANDLE_GENERATION_BITS ) - 1u )
#define RE_HANDLE_MAX_SLOTS       ( 1ull << RE_HANDLE_INDEX_BITS )

typedef struct ReHandle
{
    ReUint32 value;
} ReHandle;

/* Slot 0 is never handed out, so an all-zero handle - which is what a zeroed struct or a
 * default-initialised field holds - is always invalid rather than accidentally naming slot 0.
 */
#define RE_HANDLE_NULL_VALUE 0u

RE_ALWAYS_INLINE_HINT ReHandle
RE_Handle_Null( void )
{
    ReHandle handle;
    handle.value = RE_HANDLE_NULL_VALUE;

    return handle;
}

RE_ALWAYS_INLINE_HINT ReBool
RE_Handle_IsNull( ReHandle handle )
{
    return (ReBool) ( handle.value == RE_HANDLE_NULL_VALUE );
}

RE_ALWAYS_INLINE_HINT ReUint32
RE_Handle_Index( ReHandle handle )
{
    return handle.value & RE_HANDLE_INDEX_MASK;
}

RE_ALWAYS_INLINE_HINT ReUint32
RE_Handle_Generation( ReHandle handle )
{
    return ( handle.value >> RE_HANDLE_INDEX_BITS ) & RE_HANDLE_GENERATION_MASK;
}

typedef struct ReHandlePool
{
    RePool storage;

    /* One counter per slot, bumped on free. Kept beside the slots rather than inside them so that
     * resolving a handle never has to touch the object's own memory to decide whether it is live.
     */
    ReUint8 *generations;

    /* One bit per slot. Liveness cannot be recovered from the free list without walking it, and
     * both handle validation and iteration need it.
     */
    ReUint64 *liveBits;

    /* Backs both arrays above. Owned by the pool rather than taken from the process-lifetime
     * metadata allocator, because a handle pool has a lifecycle - a level's entity pool is
     * created and destroyed - and its bookkeeping has to go away with it.
     */
    ReVirtualRegion metadataRegion;

    ReUint64 slotCapacity;
    ReUint64 slotSize;
} ReHandlePool;

/* maxSlots is capped by RE_HANDLE_MAX_SLOTS, since the index has to fit the handle.
 *
 * Metadata (generations, live bits) comes from the metadata allocator, not the heap, so a handle
 * pool can be created before the general allocator exists.
 */
ReBool RE_HandlePool_Init( ReHandlePool *pool, ReUint64 maxSlots, ReUint64 slotSize,
    ReUint64 slotAlignment );

void RE_HandlePool_Shutdown( ReHandlePool *pool );

/* Returns the null handle when the pool is exhausted. */
ReHandle RE_HandlePool_Alloc( ReHandlePool *pool );

/* A stale or null handle is a no-op rather than corruption, which is what makes a double free
 * harmless here instead of catastrophic.
 */
void RE_HandlePool_Free( ReHandlePool *pool, ReHandle handle );

/* Returns 0 if the handle is null, out of range, or stale. Callers are expected to check - that
 * check is the entire benefit of using handles.
 */
void *RE_HandlePool_Resolve( const ReHandlePool *pool, ReHandle handle );

ReBool RE_HandlePool_IsValid( const ReHandlePool *pool, ReHandle handle );

RE_ALWAYS_INLINE_HINT ReUint64
RE_HandlePool_Count( const ReHandlePool *pool )
{
    return pool->storage.slotsInUse;
}

/* Iterates live slots. Start with *cursor == 0 and keep calling until it returns RE_False:
 *
 *     ReUint64 cursor = 0;
 *     ReHandle handle;
 *     void    *item;
 *     while ( RE_HandlePool_Next( pool, &cursor, &handle, &item ) ) { ... }
 *
 * Allocating or freeing during iteration is not supported; the cursor is a slot index and either
 * would move what it refers to.
 */
ReBool RE_HandlePool_Next( const ReHandlePool *pool, ReUint64 *cursor, ReHandle *outHandle,
    void **outItem );
