/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryArena.h>

#include <assert.h>

#include <RE/Foundation/FoundationBuild.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

/* Commit in chunks rather than a page at a time. Page-at-a-time commits turn a large allocation
 * into a syscall storm; this trades a bounded amount of over-commit for far fewer of them.
 */
#define RE_ARENA_COMMIT_STEP ( 64 * 1024 )

#if RE_BUILD < RE_BUILD_SHIPPING
#define RE_ARENA_POISON_ENABLED 1
#else
#define RE_ARENA_POISON_ENABLED 0
#endif

internal void
Arena_Poison( ReArena *arena, ReUint64 from, ReUint64 to )
{
#if RE_ARENA_POISON_ENABLED
    /* Only ever poison memory that is actually backed - writing into a reserved-but-uncommitted
     * range would fault, and the point is to make misuse loud, not to crash the poisoner.
     */
    ReUint64 limit = ( to < arena->committed ) ? to : arena->committed;

    if ( from < limit )
    {
        RE_Memory_Set( arena->base + from, RE_ARENA_POISON_BYTE, limit - from );
    }
#else
    (void) arena;
    (void) from;
    (void) to;
#endif
}

/* Makes sure [0, requiredEnd) is backed. Fixed arenas are fully backed by definition. */
internal ReBool
Arena_EnsureCommitted( ReArena *arena, ReUint64 requiredEnd )
{
    if ( requiredEnd <= arena->committed )
    {
        return RE_True;
    }

    if ( arena->kind == ReArenaKind_Fixed )
    {
        return RE_False;
    }

    ReUint64 target = RE_Memory_AlignUp( requiredEnd, RE_ARENA_COMMIT_STEP );
    if ( target > arena->capacity )
    {
        target = arena->capacity;
    }

    if ( !RE_VirtualMemory_Commit( &arena->region, arena->committed, target - arena->committed ) )
    {
        return RE_False;
    }

    arena->committed = target;

    return RE_True;
}

ReBool
RE_Arena_InitFixed( ReArena *arena, void *memory, ReUint64 size )
{
    if ( !arena || !memory || size == 0 )
    {
        return RE_False;
    }

    RE_Memory_Zero( arena, sizeof( *arena ) );

    arena->base      = (ReUint8 *) memory;
    arena->capacity  = size;
    arena->committed = size;
    arena->kind      = ReArenaKind_Fixed;

    return RE_True;
}

ReBool
RE_Arena_InitVirtual( ReArena *arena, ReUint64 maxSize )
{
    if ( !arena || maxSize == 0 )
    {
        return RE_False;
    }

    RE_Memory_Zero( arena, sizeof( *arena ) );

    ReVirtualRegion region = RE_VirtualMemory_Reserve( maxSize, 0 );
    if ( !region.base )
    {
        return RE_False;
    }

    arena->region   = region;
    arena->base     = (ReUint8 *) region.base;
    arena->capacity = region.size;
    arena->kind     = ReArenaKind_Virtual;

    return RE_True;
}

void
RE_Arena_Shutdown( ReArena *arena )
{
    if ( !arena )
    {
        return;
    }

    if ( arena->kind == ReArenaKind_Virtual )
    {
        RE_VirtualMemory_Release( &arena->region );
    }

    RE_Memory_Zero( arena, sizeof( *arena ) );
}

void *
RE_Arena_Alloc( ReArena *arena, ReUint64 size, ReUint64 alignment )
{
    assert( arena );

    if ( alignment == 0 )
    {
        alignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    assert( ( alignment & ( alignment - 1 ) ) == 0 && "alignment must be a power of two" );

    if ( size == 0 )
    {
        return 0;
    }

    /* Align the absolute address, not the offset. Aligning the offset alone is wrong whenever the
     * base itself is not aligned to the requested boundary.
     */
    ReUint64 aligned = RE_Memory_AlignUp( (ReUint64) arena->base + arena->cursor, alignment );
    ReUint64 offset  = aligned - (ReUint64) arena->base;

    /* Compared against the remaining capacity rather than summed, so a colossal size cannot wrap
     * the addition around and slip past the bounds check.
     */
    if ( offset > arena->capacity || size > arena->capacity - offset )
    {
        return 0;
    }

    if ( !Arena_EnsureCommitted( arena, offset + size ) )
    {
        return 0;
    }

    arena->cursor = offset + size;

    if ( arena->cursor > arena->highWater )
    {
        arena->highWater = arena->cursor;
    }

    return arena->base + offset;
}

void
RE_Arena_Free( ReArena *arena, void *block, ReUint64 oldSize )
{
    assert( arena );

    if ( !block || oldSize == 0 )
    {
        return;
    }

    ReUint64 offset = (ReUint64) ( (ReUint8 *) block - arena->base );

    if ( offset + oldSize == arena->cursor )
    {
        arena->cursor = offset;
        Arena_Poison( arena, offset, offset + oldSize );
    }
}

void *
RE_Arena_Realloc( ReArena *arena, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    assert( arena );

    if ( !block || oldSize == 0 )
    {
        return RE_Arena_Alloc( arena, newSize, alignment );
    }

    if ( newSize == 0 )
    {
        RE_Arena_Free( arena, block, oldSize );

        return 0;
    }

    ReUint64 offset = (ReUint64) ( (ReUint8 *) block - arena->base );

    /* The newest allocation can be resized without moving, which is the case a growing container
     * hits over and over.
     */
    if ( offset + oldSize == arena->cursor )
    {
        if ( newSize <= oldSize )
        {
            arena->cursor = offset + newSize;
            Arena_Poison( arena, arena->cursor, offset + oldSize );

            return block;
        }

        if ( newSize <= arena->capacity - offset && Arena_EnsureCommitted( arena, offset + newSize ) )
        {
            arena->cursor = offset + newSize;

            if ( arena->cursor > arena->highWater )
            {
                arena->highWater = arena->cursor;
            }

            return block;
        }

        return 0;
    }

    void *moved = RE_Arena_Alloc( arena, newSize, alignment );
    if ( !moved )
    {
        return 0;
    }

    RE_Memory_Copy( moved, block, ( newSize < oldSize ) ? newSize : oldSize );

    return moved;
}

void
RE_Arena_Reset( ReArena *arena )
{
    assert( arena );

    Arena_Poison( arena, 0, arena->cursor );

    arena->cursor          = 0;
    arena->markerSequence += 1;
}

void
RE_Arena_Trim( ReArena *arena )
{
    assert( arena );

    if ( arena->kind != ReArenaKind_Virtual )
    {
        return;
    }

    /* Keep whatever page the cursor currently sits in - decommitting it would only force an
     * immediate fault on the very next allocation.
     */
    ReUint64 keep = RE_Memory_AlignUp( arena->cursor, RE_VirtualMemory_CommitGranularity() );

    if ( keep < arena->committed )
    {
        RE_VirtualMemory_Decommit( &arena->region, keep, arena->committed - keep );
        arena->committed = keep;
    }
}

ReArenaMarker
RE_Arena_Mark( const ReArena *arena )
{
    assert( arena );

    ReArenaMarker marker;
    marker.offset   = arena->cursor;
    marker.sequence = arena->markerSequence;

    return marker;
}

void
RE_Arena_Rewind( ReArena *arena, ReArenaMarker marker )
{
    assert( arena );

    /* A marker taken before a reset (or against a different arena) names a cursor position that no
     * longer means what the caller thinks. Catching it here beats debugging the silently clobbered
     * allocation it would otherwise produce.
     */
    assert( marker.sequence == arena->markerSequence && "arena marker is stale" );
    assert( marker.offset <= arena->cursor && "arena rewind must be LIFO" );

    if ( marker.offset > arena->cursor )
    {
        return;
    }

    Arena_Poison( arena, marker.offset, arena->cursor );

    arena->cursor = marker.offset;
}

ReUint64
RE_Arena_Used( const ReArena *arena )
{
    return arena->cursor;
}

ReUint64
RE_Arena_Remaining( const ReArena *arena )
{
    return arena->capacity - arena->cursor;
}

ReUint64
RE_Arena_HighWater( const ReArena *arena )
{
    return arena->highWater;
}

internal void *
Arena_AllocatorAlloc( void *context, ReUint64 size, ReUint64 alignment )
{
    return RE_Arena_Alloc( (ReArena *) context, size, alignment );
}

internal void *
Arena_AllocatorRealloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    return RE_Arena_Realloc( (ReArena *) context, block, oldSize, newSize, alignment );
}

internal void
Arena_AllocatorFree( void *context, void *block, ReUint64 oldSize )
{
    RE_Arena_Free( (ReArena *) context, block, oldSize );
}

/* An arena hands back exactly the bytes asked for - there are no size classes to round up to, so
 * there is never anything extra for a container to claim.
 */
internal ReUint64
Arena_AllocatorQuantize( void *context, ReUint64 size, ReUint64 alignment )
{
    (void) context;
    (void) alignment;

    return size;
}

ReAllocator
RE_Arena_AsAllocator( ReArena *arena )
{
    ReAllocator allocator;
    allocator.context                = arena;
    allocator.alloc                  = Arena_AllocatorAlloc;
    allocator.realloc                = Arena_AllocatorRealloc;
    allocator.free                   = Arena_AllocatorFree;
    allocator.quantize               = Arena_AllocatorQuantize;
    allocator.isInternallyThreadSafe = RE_False;

    return allocator;
}
