/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationMemoryArena.h>

/*
    FoundationMemoryScratch.h

    Short-lived temporary memory, acquired and released around a scope.

    The problem this exists to solve is worth stating, because it is not obvious until it has
    happened. Given one shared scratch arena, a function does this:

        buffer = scratch_alloc( 1024 );
        helper();                          // internally resets or rewinds the same scratch
        use( buffer );                     // reads memory that now belongs to someone else

    Nothing reports it. The pointer is still readable and still inside the arena; it just holds
    different data. So instead of one shared arena there is a small pool of them, and acquiring
    one means asking for an arena that is *not* one the caller already holds.

    Two conventions make this work, and both are worth applying everywhere:

      1. A function that returns allocated memory takes the destination allocator as a parameter.
         Never return memory from your own scratch - the caller cannot know its lifetime.
      2. A function that needs internal temporaries acquires a scratch, declaring any arena it was
         handed as a conflict.

    @threadsafe Each thread has its own pool, created on first use. Scratch memory must never
                cross a thread boundary; it belongs to the acquiring thread and dies with its
                scope.
*/

/* Three covers realistic nesting: a caller, a helper it calls, and a helper that one calls. Deeper
 * conflicting nesting than that is a sign the call graph wants restructuring rather than a bigger
 * pool, so exhausting it is treated as a programmer error.
 */
#define RE_SCRATCH_ARENA_COUNT 3

/* Per-thread scratch arenas reserve address space, not memory, so this can be generous. */
#define RE_SCRATCH_ARENA_RESERVE_SIZE ( 64ull * 1024ull * 1024ull )

/* A held scratch. Carries the marker taken at acquisition, so release rewinds precisely to where
 * the caller started rather than resetting the whole arena.
 */
typedef struct ReScratch
{
    ReArena      *arena;
    ReArenaMarker marker;
} ReScratch;

/* Acquires an arena that is none of the ones listed in conflicts.
 *
 * Pass every arena the caller was handed or is already holding. conflicts may be null when
 * conflictCount is 0.
 *
 * Returns a scratch whose arena is 0 if the pool is exhausted or the arenas could not be created;
 * allocating from that is a null-pointer crash by design, since silently sharing an arena with a
 * caller is exactly the corruption this is here to prevent.
 */
ReScratch RE_Scratch_Acquire( ReArena *const *conflicts, ReUint32 conflictCount );

/* Rewinds to the acquisition point, freeing everything allocated through this scratch. */
void RE_Scratch_Release( ReScratch *scratch );

/* Allocates from a held scratch. */
void *RE_Scratch_Alloc( ReScratch *scratch, ReUint64 size, ReUint64 alignment );

/* The arena behind a scratch, to pass on as a conflict when calling a helper. */
RE_ALWAYS_INLINE_HINT ReArena *
RE_Scratch_Arena( ReScratch *scratch )
{
    return scratch->arena;
}

/* Releases this thread's scratch arenas. Call at thread exit; the memory system's thread shutdown
 * does it automatically.
 */
void RE_Scratch_ThreadShutdown( void );
