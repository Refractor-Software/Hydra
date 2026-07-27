/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryArena.h>

#include <assert.h>

#include <RE/Foundation/FoundationMemory.h>

void
RE_Arena_Init( ReArena *a, void *memory, ReUint64 size )
{
    a->base   = memory;
    a->size   = size;
    a->offset = 0;
}

void *
RE_Arena_Alloc( ReArena *a, ReUint64 size, ReUint64 alignment )
{
    assert( alignment != 0 && (alignment & (alignment - 1)) == 0 );

    /* Align the absolute address, not the offset alone - aligning the offset alone is wrong
     * whenever a->base itself isn't aligned to `alignment`.
     */
    ReUint64 current = (ReUint64) a->base + (ReUint64) a->offset;
    ReUint64 aligned = (ReUint64) RE_Memory_AlignUp( (ReUint64) current, alignment );
    ReUint64 padding = (ReUint64) (aligned - current);

    if ( padding > a->size - a->offset )
    {
        return 0;
    }

    ReUint64 remaining = a->size - a->offset - padding;
    if ( size > remaining )
    {
        return 0;
    }

    a->offset += padding + size;

    return (void *) aligned;
}

void
RE_Arena_Reset( ReArena *a )
{
    a->offset = 0;
}

ReArenaMarker
RE_Arena_GetMarker( const ReArena *a )
{
    ReArenaMarker marker;
    marker.offset = a->offset;

    return marker;
}

void
RE_Arena_ResetToMarker( ReArena *a, ReArenaMarker marker )
{
    a->offset = marker.offset;
}

internal void *
Arena_AllocatorAlloc( void *ctx, ReUint64 size )
{
    return RE_Arena_Alloc( (ReArena *) ctx, size, RE_MEMORY_DEFAULT_ALIGNMENT );
}

internal void
Arena_AllocatorFree( void *ctx, void *ptr )
{
    (void) ctx;
    (void) ptr;
}

ReAllocator
RE_Arena_AsAllocator( ReArena *a )
{
    ReAllocator allocator;
    allocator._context = a;
    allocator._alloc   = Arena_AllocatorAlloc;
    allocator._free    = Arena_AllocatorFree;

    return allocator;
}
