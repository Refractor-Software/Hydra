/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

#include <RE/Foundation/FoundationMemoryAllocator.h>

/*
    FoundationMemoryArena.h

    A bump/linear allocator over a caller-supplied block of memory - never allocates from the OS
    itself. Individual allocations can't be freed; reset the whole arena (or restore to a marker)
    once its contents are no longer needed.
*/

typedef struct ReArena
{
    void  *base;
    ReUint64  size;
    ReUint64  offset;
} ReArena;

/* Cheap to pass by value - just a saved offset, not a pointer back into the arena. Restoring a
 * marker against a *different* arena than the one it was taken from is a caller bug this doesn't
 * detect; keep markers scoped to the arena they came from.
 */
typedef struct ReArenaMarker
{
    ReUint64 offset;
} ReArenaMarker;

void RE_Arena_Init( ReArena *a, void *memory, ReUint64 size );

/* Returns nullptr if the arena doesn't have enough remaining space - an arena running out is an
 * expected, checkable condition (e.g. a bounded per-frame budget), not a programmer error.
 * alignment must be a nonzero power of two; violating that is a programmer error and asserts.
 */
void * RE_Arena_Alloc( ReArena *a, ReUint64 size, ReUint64 alignment );

/* Resets the arena to empty - equivalent to RE_Arena_ResetToMarker() with a marker taken right
 * after RE_Arena_Init().
 */
void RE_Arena_Reset( ReArena *a );

ReArenaMarker RE_Arena_GetMarker( const ReArena *a );
void         RE_Arena_ResetToMarker( ReArena *a, ReArenaMarker marker );

/* Bridges into the generic ReAllocator interface (RE_MEMORY_DEFAULT_ALIGNMENT only, since the
 * interface has no alignment parameter of its own). _free is a documented no-op - bump allocators
 * don't support freeing individual allocations, only bulk reset.
 */
ReAllocator RE_Arena_AsAllocator( ReArena *a );
