/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryPool.h>

#include <assert.h>

#include <RE/Foundation/FoundationBuild.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

#define RE_POOL_COMMIT_STEP ( 64 * 1024 )

#if RE_BUILD < RE_BUILD_SHIPPING
#define RE_POOL_POISON_ENABLED 1
#else
#define RE_POOL_POISON_ENABLED 0
#endif

RE_INTERNAL ReUint64
Pool_ComputeStride( ReUint64 slotSize, ReUint64 slotAlignment )
{
    /* A free slot has to hold the free-list link, so that is the floor on stride no matter how
     * small the caller's objects are.
     */
    ReUint64 stride = ( slotSize < sizeof( void * ) ) ? sizeof( void * ) : slotSize;

    return RE_Memory_AlignUp( stride, slotAlignment );
}

/* Makes sure at least slotCount slots are backed. */
RE_INTERNAL ReBool
Pool_EnsureCommitted( RePool *pool, ReUint64 slotCount )
{
    if ( slotCount <= pool->slotsCommitted )
    {
        return RE_True;
    }

    if ( pool->kind == RePoolKind_Fixed )
    {
        return RE_False;
    }

    ReUint64 requiredBytes = RE_Memory_AlignUp( slotCount * pool->slotStride, RE_POOL_COMMIT_STEP );
    ReUint64 committedBytes = pool->slotsCommitted * pool->slotStride;

    if ( requiredBytes > pool->region.size )
    {
        requiredBytes = pool->region.size;
    }

    if ( requiredBytes <= committedBytes )
    {
        return RE_False;
    }

    if ( !RE_VirtualMemory_Commit( &pool->region, committedBytes, requiredBytes - committedBytes ) )
    {
        return RE_False;
    }

    pool->slotsCommitted = requiredBytes / pool->slotStride;

    if ( pool->slotsCommitted > pool->slotCapacity )
    {
        pool->slotsCommitted = pool->slotCapacity;
    }

    return (ReBool) ( pool->slotsCommitted >= slotCount );
}

ReBool
RE_Pool_InitFixed( RePool *pool, void *memory, ReUint64 size, ReUint64 slotSize, ReUint64 slotAlignment )
{
    if ( !pool || !memory || size == 0 || slotSize == 0 )
    {
        return RE_False;
    }

    if ( slotAlignment == 0 )
    {
        slotAlignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    assert( ( slotAlignment & ( slotAlignment - 1 ) ) == 0 && "alignment must be a power of two" );

    RE_Memory_Zero( pool, sizeof( *pool ) );

    /* Align the base up front so that every slot, being a whole number of strides from it, is
     * aligned too. Whatever that costs comes off the usable size.
     */
    ReUint64 alignedBase = RE_Memory_AlignUp( (ReUint64) memory, slotAlignment );
    ReUint64 lostBytes   = alignedBase - (ReUint64) memory;

    if ( lostBytes >= size )
    {
        return RE_False;
    }

    ReUint64 stride   = Pool_ComputeStride( slotSize, slotAlignment );
    ReUint64 capacity = ( size - lostBytes ) / stride;

    if ( capacity == 0 )
    {
        return RE_False;
    }

    pool->base           = (ReUint8 *) alignedBase;
    pool->slotStride     = stride;
    pool->slotCapacity   = capacity;
    pool->slotsCommitted = capacity;
    pool->kind           = RePoolKind_Fixed;

    return RE_True;
}

ReBool
RE_Pool_InitVirtual( RePool *pool, ReUint64 maxSlots, ReUint64 slotSize, ReUint64 slotAlignment )
{
    if ( !pool || maxSlots == 0 || slotSize == 0 )
    {
        return RE_False;
    }

    if ( slotAlignment == 0 )
    {
        slotAlignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    assert( ( slotAlignment & ( slotAlignment - 1 ) ) == 0 && "alignment must be a power of two" );

    RE_Memory_Zero( pool, sizeof( *pool ) );

    ReUint64 stride = Pool_ComputeStride( slotSize, slotAlignment );

    if ( maxSlots > ( (ReUint64) -1 ) / stride )
    {
        return RE_False;
    }

    ReVirtualRegion region = RE_VirtualMemory_Reserve( maxSlots * stride, slotAlignment );
    if ( !region.base )
    {
        return RE_False;
    }

    pool->region       = region;
    pool->base         = (ReUint8 *) region.base;
    pool->slotStride   = stride;
    pool->slotCapacity = maxSlots;
    pool->kind         = RePoolKind_Virtual;

    return RE_True;
}

void
RE_Pool_Shutdown( RePool *pool )
{
    if ( !pool )
    {
        return;
    }

    if ( pool->kind == RePoolKind_Virtual )
    {
        RE_VirtualMemory_Release( &pool->region );
    }

    RE_Memory_Zero( pool, sizeof( *pool ) );
}

void *
RE_Pool_Alloc( RePool *pool )
{
    assert( pool );

    void *slot = 0;

    if ( pool->freeHead )
    {
        slot           = pool->freeHead;
        pool->freeHead = *(void **) slot;
    }
    else if ( pool->slotsInitialized < pool->slotCapacity )
    {
        /* Never-used slots are handed out by bumping a cursor rather than being threaded onto a
         * free list at init. That keeps initialisation O(1) instead of O(capacity), which matters
         * when the capacity is a generous ceiling that will mostly go untouched.
         */
        if ( !Pool_EnsureCommitted( pool, pool->slotsInitialized + 1 ) )
        {
            return 0;
        }

        slot = pool->base + pool->slotsInitialized * pool->slotStride;
        pool->slotsInitialized += 1;
    }
    else
    {
        return 0;
    }

    pool->slotsInUse += 1;

    if ( pool->slotsInUse > pool->highWater )
    {
        pool->highWater = pool->slotsInUse;
    }

    return slot;
}

void
RE_Pool_Free( RePool *pool, void *slot )
{
    assert( pool );

    if ( !slot )
    {
        return;
    }

    ReUint64 offset = (ReUint64) ( (ReUint8 *) slot - pool->base );

    assert( offset < pool->slotCapacity * pool->slotStride && "pointer does not belong to this pool" );
    assert( offset % pool->slotStride == 0 && "pointer is not on a slot boundary" );
    assert( pool->slotsInUse > 0 && "freeing from an empty pool" );

#if RE_POOL_POISON_ENABLED
    /* Poison everything past the link, so a use-after-free reads obvious garbage. The first
     * pointer-sized bytes belong to the free list and are written immediately below.
     */
    if ( pool->slotStride > sizeof( void * ) )
    {
        RE_Memory_Set( (ReUint8 *) slot + sizeof( void * ), RE_POOL_POISON_BYTE,
            pool->slotStride - sizeof( void * ) );
    }
#endif

    *(void **) slot = pool->freeHead;
    pool->freeHead  = slot;

    pool->slotsInUse -= 1;
}

void
RE_Pool_Reset( RePool *pool )
{
    assert( pool );

    /* Dropping the free list and rewinding the bump cursor frees everything at once - there is no
     * per-slot work to do, since nothing in a pool has a destructor to run.
     */
    pool->freeHead         = 0;
    pool->slotsInitialized = 0;
    pool->slotsInUse       = 0;
}

RE_INTERNAL void *
Pool_AllocatorAlloc( void *context, ReUint64 size, ReUint64 alignment )
{
    RePool *pool = (RePool *) context;

    (void) alignment;

    if ( size > pool->slotStride )
    {
        return 0;
    }

    return RE_Pool_Alloc( pool );
}

RE_INTERNAL void
Pool_AllocatorFree( void *context, void *block, ReUint64 oldSize )
{
    (void) oldSize;

    RE_Pool_Free( (RePool *) context, block );
}

RE_INTERNAL void *
Pool_AllocatorRealloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    RePool *pool = (RePool *) context;

    (void) oldSize;
    (void) alignment;

    /* Every slot is the same size, so any request that still fits is satisfied by the block the
     * caller already has, and any request that does not fit can never be satisfied.
     */
    if ( newSize <= pool->slotStride )
    {
        return block;
    }

    return 0;
}

RE_INTERNAL ReUint64
Pool_AllocatorQuantize( void *context, ReUint64 size, ReUint64 alignment )
{
    RePool *pool = (RePool *) context;

    (void) alignment;

    /* A request that fits gets the whole slot, so a container may as well use all of it. */
    return ( size <= pool->slotStride ) ? pool->slotStride : size;
}

ReAllocator
RE_Pool_AsAllocator( RePool *pool )
{
    ReAllocator allocator;
    allocator.context                = pool;
    allocator.alloc                  = Pool_AllocatorAlloc;
    allocator.realloc                = Pool_AllocatorRealloc;
    allocator.free                   = Pool_AllocatorFree;
    allocator.quantize               = Pool_AllocatorQuantize;
    allocator.isInternallyThreadSafe = RE_False;

    return allocator;
}
