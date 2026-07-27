/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationMemoryAllocator.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

/*
    FoundationMemoryArena.h

    A linear (bump) allocator: allocation advances a cursor, and everything is freed at once.

    Allocation is an align, a bounds check, and an add - a couple of instructions with no atomics,
    no metadata lookup, and no per-allocation overhead beyond alignment padding. Objects allocated
    together end up adjacent, so anything that iterates over them is a linear scan.

    The price is that individual allocations cannot be freed. That is the premise, not a
    limitation to work around: wanting individual frees from an arena means a pool is the right
    allocator instead.

    @threadsafe No, deliberately. The speed here comes from doing almost nothing per allocation,
                and a lock would cost more than the work it guards. Give each thread its own arena
                rather than sharing one - see the frame allocator, which does exactly that.
*/

typedef enum ReArenaKind
{
    /* Borrows a caller-supplied buffer. For budgeted subsystems, and for anything that has to run
     * before the virtual memory layer is available.
     */
    ReArenaKind_Fixed,

    /* Owns a reservation and commits pages as the cursor advances. The default choice: contiguous
     * so locality never degrades, pointers never move, physical memory tracks actual use, and it
     * still fails loudly at a stated ceiling instead of growing without bound.
     */
    ReArenaKind_Virtual,
} ReArenaKind;

typedef struct ReArena
{
    ReUint8 *base;
    ReUint64 capacity;  /* usable bytes; the reservation size in Virtual mode */
    ReUint64 cursor;    /* offset of the next free byte */
    ReUint64 committed; /* offset one past the last committed byte; == capacity when Fixed */
    ReUint64 highWater; /* peak cursor, for sizing */

    ReVirtualRegion region; /* Virtual mode only */
    ReArenaKind     kind;

    /* Bumped on every rewind so out-of-order rewinds can be caught in development. */
    ReUint32 markerSequence;
} ReArena;

/* A saved cursor position. An offset rather than a pointer, so it stays valid even if the arena's
 * backing were ever to move. Copying one is free, and nesting them is free.
 */
typedef struct ReArenaMarker
{
    ReUint64 offset;
    ReUint32 sequence;
} ReArenaMarker;

/* Fixed mode over caller memory. Returns RE_False only if the arguments are unusable. */
ReBool RE_Arena_InitFixed( ReArena *arena, void *memory, ReUint64 size );

/* Virtual mode. maxSize is a *reservation*, so it can be extravagant - reserved-but-uncommitted
 * address space costs no physical memory on 64-bit. Size it to the worst case you are willing to
 * tolerate, not to the expected case.
 *
 * Returns RE_False if the reservation itself failed.
 */
ReBool RE_Arena_InitVirtual( ReArena *arena, ReUint64 maxSize );

/* Releases a Virtual arena's reservation. A no-op for Fixed arenas, whose memory is the caller's.
 */
void RE_Arena_Shutdown( ReArena *arena );

/* Returns 0 when the arena is exhausted. Running out is an expected, checkable condition - a
 * bounded per-frame budget doing its job - not a programmer error, so this does not assert.
 *
 * alignment must be a power of two, or 0 for the default.
 */
void *RE_Arena_Alloc( ReArena *arena, ReUint64 size, ReUint64 alignment );

/* Reclaims the allocation only if it was the most recent one, and is otherwise a no-op.
 *
 * That single case is worth handling: a growing container's realloc almost always targets the
 * newest allocation, so this is what makes grow-in-place work against an arena.
 */
void RE_Arena_Free( ReArena *arena, void *block, ReUint64 oldSize );

/* Grows in place when block is the most recent allocation and there is room; otherwise allocates
 * and copies. Returns 0 on exhaustion, leaving the original block untouched.
 */
void *RE_Arena_Realloc( ReArena *arena, void *block, ReUint64 oldSize, ReUint64 newSize,
    ReUint64 alignment );

/* Frees everything at once. One store, regardless of how many allocations were outstanding.
 *
 * Does not decommit: a frame arena that decommitted every reset would pay page faults and
 * first-touch zeroing every single frame. Committed pages are kept for reuse; RE_Arena_Trim gives
 * them back when that is actually wanted.
 */
void RE_Arena_Reset( ReArena *arena );

/* Decommits everything above the current cursor, returning physical pages while keeping the
 * reservation. Call at a quiet point - a level transition, a suspend - never mid-frame.
 */
void RE_Arena_Trim( ReArena *arena );

ReArenaMarker RE_Arena_Mark( const ReArena *arena );

/* Rewinds to a marker, freeing everything allocated since.
 *
 * @warning Strictly LIFO. Rewinding to an old marker after a newer one was taken silently
 *          invalidates the newer allocations while live pointers to them remain. Development
 *          builds poison the abandoned range so the misuse shows up immediately as garbage.
 */
void RE_Arena_Rewind( ReArena *arena, ReArenaMarker marker );

ReUint64 RE_Arena_Used( const ReArena *arena );
ReUint64 RE_Arena_Remaining( const ReArena *arena );
ReUint64 RE_Arena_HighWater( const ReArena *arena );

/* Presents the arena through the uniform interface, so generic code can target it.
 *
 * The returned allocator points at the arena, so the arena must outlive it.
 */
ReAllocator RE_Arena_AsAllocator( ReArena *arena );

/* Written over abandoned memory in development builds. Chosen to be conspicuous both ways: as a
 * float it reads as a huge NaN-ish value, and as a pointer it is far outside any mapping, so
 * using rewound memory crashes or produces visible nonsense rather than plausible data.
 */
#define RE_ARENA_POISON_BYTE 0xFD
