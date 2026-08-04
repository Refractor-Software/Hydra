/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemorySystem.h>

#include <assert.h>

#include <RE/Foundation/FoundationContext.h>
#include <RE/Foundation/FoundationMemoryHeap.h>
#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemoryThreadCache.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

RE_GLOBAL ReBool                 gMemorySystemInitialized;
RE_GLOBAL ReMemoryDecoratorChain gMemorySystemChain;
RE_GLOBAL ReAllocator            gMemorySystemAllocator;
RE_GLOBAL ReFrameAllocator       gMemorySystemFrames;
RE_GLOBAL ReBool                 gMemorySystemHasFrames;

ReMemorySystemConfig
RE_MemorySystem_DefaultConfig( void )
{
    ReMemorySystemConfig config;
    RE_Memory_Zero( &config, sizeof( config ) );

    /* Two buffers, because nothing is pipelined yet. This has to grow to pipeline depth + 1 the
     * moment simulation and rendering are allowed to run a frame apart, or a reset will reclaim
     * memory a reader is still walking.
     */
    config.frameBufferCount = 2;

    /* Sized for a job system that does not exist yet. The arenas are reservations, so unused
     * thread slots cost address space and nothing else.
     */
    config.frameThreadCount  = 8;
    config.frameArenaReserve = 16 * 1024 * 1024;

    config.decorators = RE_Memory_DefaultDecoratorConfig();

    return config;
}

ReBool
RE_MemorySystem_Init( const ReMemorySystemConfig *config )
{
    if ( gMemorySystemInitialized )
    {
        return RE_True;
    }

    ReMemorySystemConfig defaults;

    if ( !config )
    {
        defaults = RE_MemorySystem_DefaultConfig();
        config   = &defaults;
    }

    /* The binned heap first: everything above it either wraps it or allocates from it. */
    if ( !RE_Heap_Init() )
    {
        return RE_False;
    }

    gMemorySystemAllocator = RE_Memory_BuildDecoratorChain( RE_Heap_AsAllocator(), &config->decorators,
        &gMemorySystemChain );

    if ( !RE_Frame_Init( &gMemorySystemFrames, config->frameBufferCount, config->frameThreadCount,
             config->frameArenaReserve ) )
    {
        return RE_False;
    }

    gMemorySystemHasFrames = RE_True;

    /* No overflow allocator in development, so a frame budget that has been exceeded fails at the
     * moment it happens rather than quietly spilling into the general allocator every frame -
     * which is a severe performance bug that shows up in no metric.
     */
#if RE_BUILD >= RE_BUILD_SHIPPING
    RE_Frame_SetOverflowAllocator( &gMemorySystemFrames, gMemorySystemAllocator );
#endif

    RE_Frame_BeginFrame( &gMemorySystemFrames, 0 );

    gMemorySystemInitialized = RE_True;

    /* The calling thread is a thread like any other and needs its cache and context. */
    RE_Context_ThreadInit( 0 );

    return RE_True;
}

ReUint64
RE_MemorySystem_Shutdown( void )
{
    if ( !gMemorySystemInitialized )
    {
        return 0;
    }

    ReUint64 problems = RE_Memory_ReportDecoratorFindings( &gMemorySystemChain );

    RE_Context_ThreadShutdown();

    if ( gMemorySystemHasFrames )
    {
        RE_Frame_Shutdown( &gMemorySystemFrames );
        gMemorySystemHasFrames = RE_False;
    }

    RE_Heap_Shutdown();

    gMemorySystemInitialized = RE_False;

    return problems;
}

ReBool
RE_MemorySystem_IsInitialized( void )
{
    return gMemorySystemInitialized;
}

ReAllocator
RE_MemorySystem_GlobalAllocator( void )
{
    assert( gMemorySystemInitialized && "memory system used before RE_MemorySystem_Init" );

    return gMemorySystemAllocator;
}

ReFrameAllocator *
RE_MemorySystem_FrameAllocator( void )
{
    return gMemorySystemHasFrames ? &gMemorySystemFrames : 0;
}

void
RE_MemorySystem_BeginFrame( ReUint64 frameIndex )
{
    if ( !gMemorySystemInitialized )
    {
        return;
    }

    RE_Frame_BeginFrame( &gMemorySystemFrames, frameIndex );
    RE_Memory_DecoratorTick( &gMemorySystemChain );
}

void
RE_MemorySystem_Trim( void )
{
    if ( !gMemorySystemInitialized )
    {
        return;
    }

    RE_Heap_Trim();
}

ReUint64
RE_MemorySystem_ReportFindings( void )
{
    if ( !gMemorySystemInitialized )
    {
        return 0;
    }

    return RE_Memory_ReportDecoratorFindings( &gMemorySystemChain );
}

ReMemorySystemStats
RE_MemorySystem_GetStats( void )
{
    ReMemorySystemStats stats;
    RE_Memory_Zero( &stats, sizeof( stats ) );

    if ( !gMemorySystemInitialized )
    {
        return stats;
    }

    stats.heap         = RE_Heap_GetStats();
    stats.threadCaches = RE_Memory_ThreadCacheGetStats();
    stats.metadata     = RE_MemoryMetadata_GetStats();

    if ( gMemorySystemHasFrames )
    {
        stats.frameHighWater     = RE_Frame_HighWater( &gMemorySystemFrames );
        stats.frameOverflowCount = RE_Frame_OverflowCount( &gMemorySystemFrames );
    }

    return stats;
}

void
RE_MemorySystem_ReportStats( void )
{
    ReMemorySystemStats stats = RE_MemorySystem_GetStats();

    RE_Memory_Report( "[memory] small: %llu KiB in use of %llu KiB committed across %llu span(s)",
        (unsigned long long) ( stats.heap.smallBytesInUse / 1024 ),
        (unsigned long long) ( stats.heap.smallBytesCommitted / 1024 ),
        (unsigned long long) stats.heap.smallSpansCommitted );

    /* Used over committed. A low number means memory is being held but not used, which is the
     * single most useful health signal the heap has.
     */
    if ( stats.heap.smallBytesCommitted > 0 )
    {
        ReUint64 loadFactor = ( stats.heap.smallBytesInUse * 100 ) / stats.heap.smallBytesCommitted;

        RE_Memory_Report( "[memory] small load factor: %llu%%", (unsigned long long) loadFactor );
    }

    RE_Memory_Report( "[memory] large: %llu KiB requested, %llu KiB committed across %llu allocation(s)",
        (unsigned long long) ( stats.heap.largeBytesRequested / 1024 ),
        (unsigned long long) ( stats.heap.largeBytesCommitted / 1024 ),
        (unsigned long long) stats.heap.largeCount );

    RE_Memory_Report( "[memory] thread caches: %llu holding %llu KiB",
        (unsigned long long) stats.threadCaches.cacheCount,
        (unsigned long long) ( stats.threadCaches.cachedBytes / 1024 ) );

    /* Counted separately because a design that looks lean can lose a surprising amount here, and
     * anything not reported shows up later as an unexplained gap against the OS's numbers.
     */
    RE_Memory_Report( "[memory] metadata: %llu KiB committed of %llu KiB reserved",
        (unsigned long long) ( stats.metadata.bytesCommitted / 1024 ),
        (unsigned long long) ( stats.metadata.bytesReserved / 1024 ) );

    RE_Memory_Report( "[memory] frame high water: %llu KiB",
        (unsigned long long) ( stats.frameHighWater / 1024 ) );

    /* Non-zero is a bug, not a statistic. */
    if ( stats.frameOverflowCount > 0 )
    {
        RE_Memory_Report( "[memory] WARNING: frame allocator overflowed %llu time(s)",
            (unsigned long long) stats.frameOverflowCount );
    }

    ReMemoryTagUsage tags[RE_MEMORY_MAX_TAGS];
    ReUint32         tagCount = RE_Memory_GetTagUsage( tags, RE_MEMORY_MAX_TAGS );

    for ( ReUint32 i = 0; i < tagCount; i += 1 )
    {
        if ( tags[i].tag && tags[i].peakBytes > 0 )
        {
            RE_Memory_Report( "[memory]   %s: %llu KiB in use, %llu KiB peak, %llu allocation(s)",
                tags[i].tag,
                (unsigned long long) ( tags[i].bytesInUse / 1024 ),
                (unsigned long long) ( tags[i].peakBytes / 1024 ),
                (unsigned long long) tags[i].allocationsInUse );
        }
    }
}
