/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryArena.h>

#include <assert.h>

#include <RE/Foundation/FoundationMemory.h>

void
arena_init (arena *a, void *memory, usize size)
{
    a->base   = memory;
    a->size   = size;
    a->offset = 0;
}

void *
arena_alloc (arena *a, usize size, usize alignment)
{
    assert (alignment != 0 && (alignment & (alignment - 1)) == 0);

    /* Align the absolute address, not the offset alone - aligning the offset alone is wrong
     * whenever a->base itself isn't aligned to `alignment`.
     */
    uptr current = (uptr) a->base + (uptr) a->offset;
    uptr aligned = (uptr) memory_align_up ((usize) current, alignment);
    usize padding = (usize) (aligned - current);

    if (padding > a->size - a->offset)
    {
        return 0;
    }

    usize remaining = a->size - a->offset - padding;
    if (size > remaining)
    {
        return 0;
    }

    a->offset += padding + size;

    return (void *) aligned;
}

void
arena_reset (arena *a)
{
    a->offset = 0;
}

arena_marker
arena_get_marker (const arena *a)
{
    arena_marker marker;
    marker.offset = a->offset;

    return marker;
}

void
arena_reset_to_marker (arena *a, arena_marker marker)
{
    a->offset = marker.offset;
}

static void *
arena_allocator_alloc (void *ctx, usize size)
{
    return arena_alloc ((arena *) ctx, size, MEMORY_DEFAULT_ALIGNMENT);
}

static void
arena_allocator_free (void *ctx, void *ptr)
{
    (void) ctx;
    (void) ptr;
}

memory_allocator
arena_as_allocator (arena *a)
{
    memory_allocator allocator;
    allocator._context = a;
    allocator._alloc   = arena_allocator_alloc;
    allocator._free    = arena_allocator_free;

    return allocator;
}
