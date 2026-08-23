/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationAtomic.h>
#include <RE/Foundation/FoundationMemoryArena.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationMemoryFrame.h

    Per-frame scratch memory: an arena that is reset once a frame. Two things separate this from
    a toy version, and both come from how the engine threads.

    N-buffered. Resetting at the frame boundary breaks the moment the engine pipelines - if
    simulation runs ahead of rendering, data written during frame N is still being read during
    N+1, and resetting at the end of N pulls memory out from under a live reader. Rotating between
    N buffers means the arena being reset was last written N frames ago, by which time every
    consumer has finished. N must be at least pipeline depth + 1.

    Per-thread. A single shared arena has a shared cursor, which is either a data race or an
    atomic every job in the engine contends on. Neither is acceptable, so each worker gets its
    own and allocation stays an uncontended pointer bump.

    Memory cost is N x threads x per-thread peak. That is the price of pipelining, and it is
    normally worth it - but size per-thread arenas modestly, because most workers use far less
    than the main thread.

    @threadsafe Allocation is, provided each thread uses its own index. BeginFrame is not, and
                must be called once, from one thread, while no worker is allocating.
*/

#define RE_FRAME_MAX_BUFFERS 4
#define RE_FRAME_MAX_THREADS 64

/* Rounded up to a whole number of cache lines so two threads' arenas never share one. Without
 * this the per-thread split still contends in hardware even though it does not race in the
 * language, and the cost shows up only as scaling that never arrives.
 *
 * A union rather than an aligned struct: C's _Alignas applies to declarations, not to type
 * definitions, so there is no portable way to over-align a struct type itself. Sizing the union
 * to a cache-line multiple fixes the stride, and the array's base comes from a page-aligned
 * reservation, which fixes the starting offset.
 */
#define RE_FRAME_ARENA_SLOT_SIZE \
    ( RE_CACHE_LINE_SIZE * ( ( sizeof( ReArena ) + RE_CACHE_LINE_SIZE - 1 ) / RE_CACHE_LINE_SIZE ) )

typedef union ReFrameThreadArena
{
    ReArena arena;
    ReUint8 padding[RE_FRAME_ARENA_SLOT_SIZE];
} ReFrameThreadArena;

typedef struct ReFrameAllocator
{
    ReFrameThreadArena *arenas; /* [bufferCount][threadCount], buffer-major */

    ReUint32 bufferCount;
    ReUint32 threadCount;
    ReUint32 currentBuffer;
    ReUint64 frameIndex;

    /* Where allocations go when a per-thread arena is exhausted. May be null, in which case
     * overflow simply fails.
     */
    ReAllocator overflowAllocator;
    ReBool      hasOverflowAllocator;

    /* Counted, never silent. A frame allocator quietly spilling thousands of allocations into the
     * general allocator every frame is a severe performance bug that shows up in no metric unless
     * something is counting - so this is checked and reported, and a non-zero value is a bug
     * rather than a success.
     */
    ReAtomicUint64 overflowCount;
    ReAtomicUint64 overflowBytes;
} ReFrameAllocator;

/* bufferCount is the pipeline depth plus one; threadCount is how many workers will allocate.
 * arenaReserveSize is per thread per buffer, and is a reservation, so it can be generous.
 */
ReBool RE_Frame_Init( ReFrameAllocator *allocator, ReUint32 bufferCount, ReUint32 threadCount,
    ReUint64 arenaReserveSize );

void RE_Frame_Shutdown( ReFrameAllocator *allocator );

/* Where exhausted allocations go. Without one, overflow returns 0.
 *
 * The intended configuration is no overflow allocator in development, so a regression fails at
 * the moment it appears, and a real one in shipping, so players get a slower frame rather than a
 * crash.
 */
void RE_Frame_SetOverflowAllocator( ReFrameAllocator *allocator, ReAllocator overflow );

/* Rotates to the next buffer and resets it. Call once per frame, before any worker allocates. */
void RE_Frame_BeginFrame( ReFrameAllocator *allocator, ReUint64 frameIndex );

/* Uncontended pointer bump, provided threadIndex is this thread's own.
 *
 * Returns 0 only when the arena is exhausted and no overflow allocator is set.
 */
void *RE_Frame_Alloc( ReFrameAllocator *allocator, ReUint32 threadIndex, ReUint64 size,
    ReUint64 alignment );

/* The current buffer's arena for a thread, to pass to code that takes an arena directly. */
ReArena *RE_Frame_Arena( ReFrameAllocator *allocator, ReUint32 threadIndex );

/* Peak bytes used by any one thread in any one buffer. The single most useful sizing number:
 * it says both whether the arenas are oversized and how close they are to overflowing.
 */
ReUint64 RE_Frame_HighWater( const ReFrameAllocator *allocator );

RE_ALWAYS_INLINE_HINT ReUint64
RE_Frame_OverflowCount( const ReFrameAllocator *allocator )
{
    return RE_Atomic_LoadUint64( &allocator->overflowCount );
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Frame_OverflowBytes( const ReFrameAllocator *allocator )
{
    return RE_Atomic_LoadUint64( &allocator->overflowBytes );
}
