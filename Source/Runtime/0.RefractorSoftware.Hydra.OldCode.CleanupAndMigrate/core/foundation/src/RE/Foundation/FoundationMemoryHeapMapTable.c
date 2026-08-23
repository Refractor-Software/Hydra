/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationMemoryHeapInternal.h"

#if RE_HEAP_MAP_STRATEGY == RE_HEAP_MAP_STRATEGY_SIDE_TABLE

#include <assert.h>

#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationSpinLock.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

/*
    Strategy C. A two-level radix table maps a span-aligned address to its descriptor.

    Spans are 64 KiB-aligned and a whole number of 64 KiB blocks, so bits 16..31 of an address
    index the second level directly and bits 32..47 index the first. Every 64 KiB block of a
    multi-block span registers the same descriptor, so a pointer anywhere inside a span finds it
    with the same two loads.

    x64 user space is 47 bits today, so a 16-bit first level covers it. Anything above that range
    simply misses the table and is reported as not ours, which is the correct answer rather than
    an out-of-bounds read.
*/

#define TABLE_L1_BITS  16
#define TABLE_L2_BITS  16
#define TABLE_L1_COUNT ( 1u << TABLE_L1_BITS )
#define TABLE_L2_COUNT ( 1u << TABLE_L2_BITS )

#define TABLE_L1_INDEX( address ) ( (ReUint32) ( ( (ReUint64) ( address ) >> 32 ) & ( TABLE_L1_COUNT - 1 ) ) )
#define TABLE_L2_INDEX( address ) ( (ReUint32) ( ( (ReUint64) ( address ) >> 16 ) & ( TABLE_L2_COUNT - 1 ) ) )

/* Spans are carved from large reservations rather than one mapping each. Some kernels track every
 * distinct mapping separately and cap how many a process may hold; an allocator that maps each
 * span independently can hit that ceiling and start failing while plenty of memory is free.
 */
#define TABLE_REGION_SIZE ( 64ull * 1024ull * 1024ull )

typedef struct ReTableRegion
{
    ReVirtualRegion       region;
    ReUint64              bumpOffset; /* next unused byte */
    struct ReTableRegion *next;
} ReTableRegion;

/* Spans are recycled by block count, since a released two-block span cannot serve a one-block
 * request without splitting and we would rather not deal with splitting or coalescing.
 */
#define TABLE_MAX_SPAN_BLOCKS ( RE_HEAP_MAX_SPAN_SIZE / RE_HEAP_BLOCK_SIZE )

RE_GLOBAL ReSpinLock      gTableLock;
RE_GLOBAL ReHeapSpan    **gTableLevel1;
RE_GLOBAL ReTableRegion  *gTableRegions;
RE_GLOBAL ReHeapSpan     *gTableFreeSpans[TABLE_MAX_SPAN_BLOCKS + 1];
RE_GLOBAL ReHeapSpan     *gTableFreeDescriptors;
RE_GLOBAL ReUint64        gTableReservedBytes;
RE_GLOBAL ReBool          gTableInitialized;

RE_INTERNAL ReHeapSpan **
Table_LeafFor( ReUint64 address, ReBool create )
{
    ReUint32 l1 = TABLE_L1_INDEX( address );

    ReHeapSpan **leaf = (ReHeapSpan **) gTableLevel1[l1];

    if ( !leaf && create )
    {
        /* From the metadata allocator, not the heap - the table is the heap's own bookkeeping and
         * allocating it with the heap would recurse.
         */
        leaf = (ReHeapSpan **) RE_MemoryMetadata_Alloc( TABLE_L2_COUNT * sizeof( ReHeapSpan * ), 64 );

        if ( !leaf )
        {
            return 0;
        }

        gTableLevel1[l1] = (ReHeapSpan *) leaf;
    }

    return leaf;
}

RE_INTERNAL ReBool
Table_Register( ReHeapSpan *span, ReUint64 spanSize )
{
    for ( ReUint64 offset = 0; offset < spanSize; offset += RE_HEAP_BLOCK_SIZE )
    {
        ReUint64     address = (ReUint64) span->base + offset;
        ReHeapSpan **leaf    = Table_LeafFor( address, RE_True );

        if ( !leaf )
        {
            return RE_False;
        }

        leaf[TABLE_L2_INDEX( address )] = span;
    }

    return RE_True;
}

RE_INTERNAL void
Table_Unregister( ReHeapSpan *span, ReUint64 spanSize )
{
    for ( ReUint64 offset = 0; offset < spanSize; offset += RE_HEAP_BLOCK_SIZE )
    {
        ReUint64     address = (ReUint64) span->base + offset;
        ReHeapSpan **leaf    = Table_LeafFor( address, RE_False );

        if ( leaf )
        {
            leaf[TABLE_L2_INDEX( address )] = 0;
        }
    }
}

/* Caller holds the lock. */
RE_INTERNAL ReUint8 *
Table_CarveMemory( ReUint64 spanSize )
{
    for ( ReTableRegion *region = gTableRegions; region; region = region->next )
    {
        if ( region->region.size - region->bumpOffset >= spanSize )
        {
            ReUint8 *base = (ReUint8 *) region->region.base + region->bumpOffset;

            if ( !RE_VirtualMemory_Commit( &region->region, region->bumpOffset, spanSize ) )
            {
                return 0;
            }

            region->bumpOffset += spanSize;

            return base;
        }
    }

    ReUint64 reserveSize = ( spanSize > TABLE_REGION_SIZE ) ? spanSize : TABLE_REGION_SIZE;

    ReVirtualRegion reserved = RE_VirtualMemory_Reserve( reserveSize, RE_HEAP_BLOCK_SIZE );
    if ( !reserved.base )
    {
        return 0;
    }

    ReTableRegion *region = (ReTableRegion *) RE_MemoryMetadata_Alloc( sizeof( ReTableRegion ), 16 );
    if ( !region )
    {
        RE_VirtualMemory_Release( &reserved );

        return 0;
    }

    region->region     = reserved;
    region->bumpOffset = 0;
    region->next       = gTableRegions;
    gTableRegions      = region;

    gTableReservedBytes += reserved.size;

    if ( !RE_VirtualMemory_Commit( &region->region, 0, spanSize ) )
    {
        return 0;
    }

    region->bumpOffset = spanSize;

    return (ReUint8 *) reserved.base;
}

ReBool
RE_HeapMap_Init( void )
{
    if ( gTableInitialized )
    {
        return RE_True;
    }

    RE_SpinLock_Init( &gTableLock );

    if ( !RE_HeapSizeClass_Init() )
    {
        return RE_False;
    }

    /* Half a megabyte of pointers, zeroed. Leaves are created on demand, so the resident cost
     * tracks the address space actually touched rather than the space theoretically coverable.
     */
    gTableLevel1 = (ReHeapSpan **) RE_MemoryMetadata_Alloc( TABLE_L1_COUNT * sizeof( ReHeapSpan * ), 64 );

    if ( !gTableLevel1 )
    {
        return RE_False;
    }

    gTableInitialized = RE_True;

    return RE_True;
}

void
RE_HeapMap_Shutdown( void )
{
    if ( !gTableInitialized )
    {
        return;
    }

    RE_SpinLock_Acquire( &gTableLock );

    ReTableRegion *region = gTableRegions;
    while ( region )
    {
        ReTableRegion *next = region->next;

        RE_VirtualMemory_Release( &region->region );

        region = next;
    }

    gTableRegions         = 0;
    gTableFreeDescriptors = 0;
    gTableReservedBytes   = 0;

    for ( ReUint32 i = 0; i <= TABLE_MAX_SPAN_BLOCKS; i += 1 )
    {
        gTableFreeSpans[i] = 0;
    }

    /* The level-1 array and any leaves came from the metadata allocator, which is permanent by
     * design. Zeroing level 1 is what makes a later Init start clean.
     */
    if ( gTableLevel1 )
    {
        RE_Memory_Zero( gTableLevel1, TABLE_L1_COUNT * sizeof( ReHeapSpan * ) );
    }

    gTableInitialized = RE_False;

    RE_SpinLock_Release( &gTableLock );
}

ReHeapSpan *
RE_HeapMap_AcquireSpan( ReUint32 classIndex )
{
    assert( gTableInitialized );

    ReUint64 spanSize   = RE_HeapSizeClass_SpanSize( classIndex );
    ReUint32 blockCount = (ReUint32) ( spanSize / RE_HEAP_BLOCK_SIZE );

    assert( blockCount >= 1 && blockCount <= TABLE_MAX_SPAN_BLOCKS );

    RE_SpinLock_Acquire( &gTableLock );

    ReHeapSpan *span = gTableFreeSpans[blockCount];

    if ( span )
    {
        gTableFreeSpans[blockCount] = span->next;
    }
    else
    {
        ReUint8 *memory = Table_CarveMemory( spanSize );
        if ( !memory )
        {
            RE_SpinLock_Release( &gTableLock );

            return 0;
        }

        span = gTableFreeDescriptors;
        if ( span )
        {
            gTableFreeDescriptors = span->next;
        }
        else
        {
            span = (ReHeapSpan *) RE_MemoryMetadata_Alloc( sizeof( ReHeapSpan ), 16 );
        }

        if ( !span )
        {
            RE_SpinLock_Release( &gTableLock );

            return 0;
        }

        span->base = memory;
    }

    span->next       = 0;
    span->prev       = 0;
    span->classIndex = classIndex;
    span->binCount   = RE_HeapSizeClass_BinsPerSpan( classIndex );
    span->binsInUse  = 0;
    span->freeRun    = 0;
    span->canary     = RE_HEAP_SPAN_CANARY;

    /* One run covering every bin. Carving is O(1) rather than O(binCount) because of this. */
    ReHeapFreeRun *run = (ReHeapFreeRun *) span->base;
    run->nextRun   = RE_HEAP_BIN_NONE;
    run->runLength = span->binCount;

    if ( !Table_Register( span, spanSize ) )
    {
        span->next            = gTableFreeDescriptors;
        gTableFreeDescriptors = span;

        RE_SpinLock_Release( &gTableLock );

        return 0;
    }

    RE_SpinLock_Release( &gTableLock );

    return span;
}

void
RE_HeapMap_ReleaseSpan( ReHeapSpan *span )
{
    assert( span && span->canary == RE_HEAP_SPAN_CANARY );

    ReUint64 spanSize   = RE_HeapSizeClass_SpanSize( span->classIndex );
    ReUint32 blockCount = (ReUint32) ( spanSize / RE_HEAP_BLOCK_SIZE );

    RE_SpinLock_Acquire( &gTableLock );

    Table_Unregister( span, spanSize );

    /* The memory is kept rather than returned to the OS. Releasing it here would mean a workload
     * oscillating around a span boundary paid a reserve and commit every cycle; the span is
     * parked for reuse and only really given back on an explicit trim.
     */
    span->next                  = gTableFreeSpans[blockCount];
    gTableFreeSpans[blockCount] = span;

    RE_SpinLock_Release( &gTableLock );
}

ReHeapSpan *
RE_HeapMap_SpanOf( const void *ptr )
{
    ReUint64 address = (ReUint64) ptr;

    ReHeapSpan **leaf = (ReHeapSpan **) gTableLevel1[TABLE_L1_INDEX( address )];
    if ( !leaf )
    {
        return 0;
    }

    return leaf[TABLE_L2_INDEX( address )];
}

ReUint64
RE_HeapMap_ReservedBytes( void )
{
    return gTableReservedBytes;
}

#endif /* RE_HEAP_MAP_STRATEGY == RE_HEAP_MAP_STRATEGY_SIDE_TABLE */
