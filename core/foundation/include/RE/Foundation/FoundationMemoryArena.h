/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

#include <RE/Foundation/FoundationMemoryAllocator.h>

/*
    foundation_memory_arena.h

    A bump/linear allocator over a caller-supplied block of memory - never allocates from the OS
    itself. Individual allocations can't be freed; reset the whole arena (or restore to a marker)
    once its contents are no longer needed.
*/

typedef struct arena
{
    void  *base;
    usize  size;
    usize  offset;
} arena;

/* Cheap to pass by value - just a saved offset, not a pointer back into the arena. Restoring a
 * marker against a *different* arena than the one it was taken from is a caller bug this doesn't
 * detect; keep markers scoped to the arena they came from.
 */
typedef struct arena_marker
{
    usize offset;
} arena_marker;

void arena_init (arena *a, void *memory, usize size);

/* Returns nullptr if the arena doesn't have enough remaining space - an arena running out is an
 * expected, checkable condition (e.g. a bounded per-frame budget), not a programmer error.
 * alignment must be a nonzero power of two; violating that is a programmer error and asserts.
 */
void * arena_alloc (arena *a, usize size, usize alignment);

/* Resets the arena to empty - equivalent to arena_reset_to_marker() with a marker taken right
 * after arena_init().
 */
void arena_reset (arena *a);

arena_marker arena_get_marker (const arena *a);
void         arena_reset_to_marker (arena *a, arena_marker marker);

/* Bridges into the generic memory_allocator interface (MEMORY_DEFAULT_ALIGNMENT only, since the
 * interface has no alignment parameter of its own). _free is a documented no-op - bump allocators
 * don't support freeing individual allocations, only bulk reset.
 */
memory_allocator arena_as_allocator (arena *a);
