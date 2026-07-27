/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationMemoryDecorator.h>
#include <RE/Foundation/FoundationMemoryFrame.h>
#include <RE/Foundation/FoundationMemoryHeap.h>
#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemoryThreadCache.h>

/*
    FoundationMemorySystem.h

    Brings the memory system up as a whole and owns the pieces that are process-wide: the binned
    heap, the decorator chain assembled over it, and the frame allocator.

    One place to call at startup rather than half a dozen, and one place that knows the order the
    layers have to come up in.

    Everything here is process scope. Per-thread ambient state - which allocator a given worker
    should reach for without being handed one - lives in FoundationContext.h.
*/

typedef struct ReMemorySystemConfig
{
    /* Frame allocator geometry. bufferCount must be at least pipeline depth + 1, or a reset will
     * pull memory out from under a thread still reading last frame's data.
     */
    ReUint32 frameBufferCount;
    ReUint32 frameThreadCount;
    ReUint64 frameArenaReserve;

    ReMemoryDecoratorConfig decorators;
} ReMemorySystemConfig;

/* Defaults for the current build level: decorator defaults, no pipelining assumed, and a frame
 * arena reservation generous enough that hitting it means something is wrong rather than tight.
 */
ReMemorySystemConfig RE_MemorySystem_DefaultConfig( void );

/* Idempotent. Order matters and is handled here: heap, then decorators over it, then the frame
 * allocator, then this thread's cache and context.
 */
ReBool RE_MemorySystem_Init( const ReMemorySystemConfig *config );

/* Reports anything the decorators found, tears everything down, and returns the number of
 * problems - so a caller can fail a shutdown check on a non-zero result.
 *
 * @warning Bookkeeping taken from the metadata allocator is permanent by design and is not
 *          reclaimed here, so an init/shutdown cycle costs a few megabytes that never come back.
 *          Fine for a process that starts once; something that cycles repeatedly should stay up
 *          instead.
 */
ReUint64 RE_MemorySystem_Shutdown( void );

ReBool RE_MemorySystem_IsInitialized( void );

/* The general-purpose allocator, with whatever decorators this build installed. This is what
 * unpredictable lifetimes should use; anything with a known lifetime pattern should be reaching
 * for an arena, a pool, or the frame allocator instead.
 */
ReAllocator RE_MemorySystem_GlobalAllocator( void );

ReFrameAllocator *RE_MemorySystem_FrameAllocator( void );

/* Rotates the frame allocator onto the next buffer and does the decorators' per-frame upkeep.
 *
 * Call once per frame, from one thread, before any worker allocates.
 */
void RE_MemorySystem_BeginFrame( ReUint64 frameIndex );

/* Hands cached memory back to the OS. Call at a quiet point - a level transition, a suspend -
 * never mid-frame.
 */
void RE_MemorySystem_Trim( void );

/* Reports outstanding leaks, double frees, and quarantine violations without tearing down.
 * Returns how many problems were found.
 */
ReUint64 RE_MemorySystem_ReportFindings( void );

typedef struct ReMemorySystemStats
{
    ReHeapStats           heap;
    ReThreadCacheStats    threadCaches;
    ReMemoryMetadataStats metadata;

    ReUint64 frameHighWater;
    ReUint64 frameOverflowCount;
} ReMemorySystemStats;

ReMemorySystemStats RE_MemorySystem_GetStats( void );

/* Writes a human-readable summary through the report function. The numbers worth looking at
 * before calling a session healthy: load factor, metadata overhead, and whether the frame
 * allocator ever had to spill.
 */
void RE_MemorySystem_ReportStats( void );
