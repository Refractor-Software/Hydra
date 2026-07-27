/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryHandlePool.h>

#include <assert.h>

#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

/*
    Handle indices are slot index + 1, so that index 0 never occurs in a real handle and an
    all-zero handle is therefore always invalid. That costs one value out of the index range
    rather than a whole slot of storage.
*/

/* The generation and live-bit arrays share one reservation, laid out back to back. */
RE_INTERNAL ReBool
HandlePool_AllocateMetadata( ReHandlePool *pool, ReUint64 maxSlots )
{
    ReUint64 generationBytes = RE_Memory_AlignUp( maxSlots, 8 );
    ReUint64 liveWordCount   = ( maxSlots + 63 ) / 64;
    ReUint64 liveBytes       = liveWordCount * sizeof( ReUint64 );
    ReUint64 totalBytes      = generationBytes + liveBytes;

    ReVirtualRegion region = RE_VirtualMemory_Reserve( totalBytes, 0 );
    if ( !region.base )
    {
        return RE_False;
    }

    /* Committed in full rather than incrementally: this is roughly 1.125 bytes per slot, so even
     * a large pool is a small fixed cost, and paying it once keeps the alloc path free of a
     * commit check on two separate arrays.
     */
    if ( !RE_VirtualMemory_Commit( &region, 0, totalBytes ) )
    {
        RE_VirtualMemory_Release( &region );

        return RE_False;
    }

    pool->metadataRegion = region;
    pool->generations    = (ReUint8 *) region.base;
    pool->liveBits       = (ReUint64 *) ( (ReUint8 *) region.base + generationBytes );

    return RE_True;
}

RE_INTERNAL void
HandlePool_SetLive( ReHandlePool *pool, ReUint64 slotIndex, ReBool live )
{
    ReUint64 word = slotIndex / 64;
    ReUint64 bit  = slotIndex % 64;

    if ( live )
    {
        pool->liveBits[word] |= ( 1ull << bit );
    }
    else
    {
        pool->liveBits[word] &= ~( 1ull << bit );
    }
}

RE_INTERNAL ReBool
HandlePool_IsLive( const ReHandlePool *pool, ReUint64 slotIndex )
{
    ReUint64 word = slotIndex / 64;
    ReUint64 bit  = slotIndex % 64;

    return (ReBool) ( ( pool->liveBits[word] >> bit ) & 1ull );
}

/* Resolves a handle to its slot index, or returns RE_False if it is null, out of range, dead, or
 * from an earlier generation of the same slot.
 */
RE_INTERNAL ReBool
HandlePool_SlotOf( const ReHandlePool *pool, ReHandle handle, ReUint64 *outSlotIndex )
{
    if ( RE_Handle_IsNull( handle ) )
    {
        return RE_False;
    }

    ReUint32 handleIndex = RE_Handle_Index( handle );
    if ( handleIndex == 0 || handleIndex > pool->slotCapacity )
    {
        return RE_False;
    }

    ReUint64 slotIndex = (ReUint64) handleIndex - 1;

    if ( !HandlePool_IsLive( pool, slotIndex ) )
    {
        return RE_False;
    }

    if ( pool->generations[slotIndex] != (ReUint8) RE_Handle_Generation( handle ) )
    {
        return RE_False;
    }

    *outSlotIndex = slotIndex;

    return RE_True;
}

ReBool
RE_HandlePool_Init( ReHandlePool *pool, ReUint64 maxSlots, ReUint64 slotSize, ReUint64 slotAlignment )
{
    if ( !pool || maxSlots == 0 || slotSize == 0 )
    {
        return RE_False;
    }

    /* One index value is spent on the null handle, so the usable capacity is one below what the
     * index field can express.
     */
    if ( maxSlots > RE_HANDLE_MAX_SLOTS - 1 )
    {
        return RE_False;
    }

    RE_Memory_Zero( pool, sizeof( *pool ) );

    if ( !RE_Pool_InitVirtual( &pool->storage, maxSlots, slotSize, slotAlignment ) )
    {
        return RE_False;
    }

    if ( !HandlePool_AllocateMetadata( pool, maxSlots ) )
    {
        RE_Pool_Shutdown( &pool->storage );

        return RE_False;
    }

    pool->slotCapacity = maxSlots;
    pool->slotSize     = slotSize;

    return RE_True;
}

void
RE_HandlePool_Shutdown( ReHandlePool *pool )
{
    if ( !pool )
    {
        return;
    }

    RE_VirtualMemory_Release( &pool->metadataRegion );
    RE_Pool_Shutdown( &pool->storage );
    RE_Memory_Zero( pool, sizeof( *pool ) );
}

ReHandle
RE_HandlePool_Alloc( ReHandlePool *pool )
{
    assert( pool );

    void *slot = RE_Pool_Alloc( &pool->storage );
    if ( !slot )
    {
        return RE_Handle_Null();
    }

    ReUint64 slotIndex = (ReUint64) ( (ReUint8 *) slot - pool->storage.base ) / pool->storage.slotStride;

    assert( slotIndex < pool->slotCapacity );

    HandlePool_SetLive( pool, slotIndex, RE_True );

    ReHandle handle;
    handle.value = (ReUint32) ( slotIndex + 1 )
        | ( ( (ReUint32) pool->generations[slotIndex] & RE_HANDLE_GENERATION_MASK ) << RE_HANDLE_INDEX_BITS );

    return handle;
}

void
RE_HandlePool_Free( ReHandlePool *pool, ReHandle handle )
{
    assert( pool );

    ReUint64 slotIndex;
    if ( !HandlePool_SlotOf( pool, handle, &slotIndex ) )
    {
        /* Stale, null, or already freed. Deliberately a no-op: turning a double free into a
         * harmless nothing is most of the point of handles.
         */
        return;
    }

    HandlePool_SetLive( pool, slotIndex, RE_False );

    /* Bumping the generation is what invalidates every handle anyone still holds to this slot.
     * It wraps, which is why the generation width is a deliberate choice rather than a detail.
     */
    pool->generations[slotIndex] = (ReUint8) ( ( pool->generations[slotIndex] + 1 ) & RE_HANDLE_GENERATION_MASK );

    RE_Pool_Free( &pool->storage, pool->storage.base + slotIndex * pool->storage.slotStride );
}

void *
RE_HandlePool_Resolve( const ReHandlePool *pool, ReHandle handle )
{
    assert( pool );

    ReUint64 slotIndex;
    if ( !HandlePool_SlotOf( pool, handle, &slotIndex ) )
    {
        return 0;
    }

    return pool->storage.base + slotIndex * pool->storage.slotStride;
}

ReBool
RE_HandlePool_IsValid( const ReHandlePool *pool, ReHandle handle )
{
    ReUint64 slotIndex;

    return HandlePool_SlotOf( pool, handle, &slotIndex );
}

ReBool
RE_HandlePool_Next( const ReHandlePool *pool, ReUint64 *cursor, ReHandle *outHandle, void **outItem )
{
    assert( pool && cursor );

    /* Only slots that have ever been handed out can be live, so the scan stops at the bump cursor
     * rather than at the full capacity - which for a generously sized pool is most of the work
     * avoided.
     */
    ReUint64 limit = pool->storage.slotsInitialized;

    for ( ReUint64 slotIndex = *cursor; slotIndex < limit; slotIndex += 1 )
    {
        if ( !HandlePool_IsLive( pool, slotIndex ) )
        {
            continue;
        }

        if ( outHandle )
        {
            outHandle->value = (ReUint32) ( slotIndex + 1 )
                | ( ( (ReUint32) pool->generations[slotIndex] & RE_HANDLE_GENERATION_MASK ) << RE_HANDLE_INDEX_BITS );
        }

        if ( outItem )
        {
            *outItem = pool->storage.base + slotIndex * pool->storage.slotStride;
        }

        *cursor = slotIndex + 1;

        return RE_True;
    }

    *cursor = limit;

    return RE_False;
}
