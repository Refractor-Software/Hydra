/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationCompiler.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationMemoryAllocator.h

    The one interface every allocator in the engine presents, so that code can be written against
    "an allocator" rather than against a particular one. An arena, a pool, and the general-purpose
    heap are all reachable through this.

    Note what the caller passes on free and realloc: the size it originally asked for. The
    general-purpose heap could recover that from the pointer, but an arena cannot, and giving an
    arena a per-allocation header to make it possible would destroy the zero-overhead property
    that is the entire reason to reach for one. Making size the caller's responsibility keeps
    arenas free. Callers nearly always have it to hand anyway.
*/

typedef struct ReAllocator ReAllocator;

typedef void    *( *ReAllocatorAllocFn )( void *context, ReUint64 size, ReUint64 alignment );
typedef void    *( *ReAllocatorReallocFn )( void *context, void *block, ReUint64 oldSize,
                                            ReUint64 newSize, ReUint64 alignment );
typedef void     ( *ReAllocatorFreeFn )( void *context, void *block, ReUint64 oldSize );
typedef ReUint64 ( *ReAllocatorQuantizeFn )( void *context, ReUint64 size, ReUint64 alignment );

struct ReAllocator
{
    void *context;

    ReAllocatorAllocFn    alloc;
    ReAllocatorReallocFn  realloc;
    ReAllocatorFreeFn     free;
    ReAllocatorQuantizeFn quantize;

    /* Whether this allocator does its own synchronisation.
     *
     * A plain field rather than a query, because it is a fixed property of the implementation and
     * a call would only ever return a constant. It is load-bearing: the binned heap locks per size
     * class internally and must never be wrapped in a global locking proxy, which would collapse
     * all of that back down to one lock.
     */
    ReBool isInternallyThreadSafe;
};

/* Every allocator honours this as a floor. 16 bytes covers the SIMD types on mainstream 64-bit
 * targets, which is what makes it the right default rather than 8.
 */
#define RE_MEMORY_DEFAULT_ALIGNMENT 16

/*
    The calls below are thin forwarding wrappers. They exist so call sites read as
    RE_Memory_Alloc( allocator, ... ) rather than reaching through the struct by hand, and so that
    alignment defaulting lives in exactly one place.
*/

RE_ALWAYS_INLINE_HINT void *
RE_Memory_Alloc( const ReAllocator *allocator, ReUint64 size, ReUint64 alignment )
{
    if ( alignment == 0 )
    {
        alignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    return allocator->alloc( allocator->context, size, alignment );
}

RE_ALWAYS_INLINE_HINT void *
RE_Memory_Realloc( const ReAllocator *allocator, void *block, ReUint64 oldSize, ReUint64 newSize,
    ReUint64 alignment )
{
    if ( alignment == 0 )
    {
        alignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    return allocator->realloc( allocator->context, block, oldSize, newSize, alignment );
}

RE_ALWAYS_INLINE_HINT void
RE_Memory_Free( const ReAllocator *allocator, void *block, ReUint64 oldSize )
{
    allocator->free( allocator->context, block, oldSize );
}

/* What a request of this size would *actually* cost, so a growable container can size itself to
 * the boundary instead of silently wasting the difference between its request and its bin.
 */
RE_ALWAYS_INLINE_HINT ReUint64
RE_Memory_Quantize( const ReAllocator *allocator, ReUint64 size, ReUint64 alignment )
{
    if ( alignment == 0 )
    {
        alignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    return allocator->quantize( allocator->context, size, alignment );
}

/* Returns 0 rather than a wrapped-around allocation when size * count overflows. An unchecked
 * multiply here is a classic path to a buffer far smaller than the caller believes it got.
 */
RE_ALWAYS_INLINE_HINT void *
RE_Memory_AllocArray( const ReAllocator *allocator, ReUint64 size, ReUint64 alignment, ReUint64 count )
{
    if ( count != 0 && size > ( (ReUint64) -1 ) / count )
    {
        return 0;
    }

    return RE_Memory_Alloc( allocator, size * count, alignment );
}

/* Type-directed convenience. The type's own alignment is used, so an over-aligned struct gets what
 * it needs without the call site restating it and eventually getting it wrong.
 */
#define RE_MEMORY_ALLOC_TYPE( allocator, T ) \
    ( (T *) RE_Memory_Alloc( (allocator), sizeof( T ), RE_ALIGNOF( T ) ) )

#define RE_MEMORY_ALLOC_ARRAY( allocator, T, count ) \
    ( (T *) RE_Memory_AllocArray( (allocator), sizeof( T ), RE_ALIGNOF( T ), (count) ) )

#define RE_MEMORY_FREE_TYPE( allocator, block, T ) \
    RE_Memory_Free( (allocator), (block), sizeof( T ) )

#define RE_MEMORY_FREE_ARRAY( allocator, block, T, count ) \
    RE_Memory_Free( (allocator), (block), sizeof( T ) * (ReUint64) (count) )
