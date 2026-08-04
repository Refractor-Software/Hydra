/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationMemoryAllocator.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationMemoryHeap.h

    The general-purpose allocator: the fallback for lifetimes that are genuinely unpredictable.

    Everything the specialised allocators can serve, they serve better - an arena bumps a pointer,
    a pool pops a free list, and neither has to work out what a pointer is on the way back. This
    exists for what is left over, which once the specialised allocators are in place is a much
    smaller fraction of engine traffic than people expect.

    Small requests are rounded to a size class and served from spans of same-class bins, with one
    lock per class so that 16-byte allocations never contend with 256-byte ones. Large requests go
    straight to the virtual memory layer with their bookkeeping in a side table.

    @threadsafe Yes, and internally - do not wrap this in a locking proxy, which would collapse
                the per-class locking back down to one global lock and undo the entire design.
*/

/* Alignments above this are served by the large path instead of by promoting to a bigger class.
 * Past a few hundred bytes the promotion wastes more than a page allocation would.
 */
#define RE_HEAP_MAX_PROMOTABLE_ALIGNMENT 256

typedef struct ReHeapStats
{
    ReUint64 smallBytesInUse;     /* bins handed out, counted at their class size */
    ReUint64 smallBytesCommitted; /* physical memory behind the spans */
    ReUint64 smallSpansCommitted;

    ReUint64 largeBytesRequested; /* what callers asked for */
    ReUint64 largeBytesCommitted; /* what the OS actually gave - the difference is the overhead */
    ReUint64 largeCount;

    ReUint64 mapReservedBytes; /* address space claimed by the pointer-to-span map */
    ReUint64 metadataBytes;    /* the allocator's own bookkeeping, counted rather than hidden */
} ReHeapStats;

/* Idempotent. Brings up the size class table and the pointer map. */
ReBool RE_Heap_Init( void );

/* @warning Only valid once nothing holds heap memory. Frees every span and large allocation. */
void RE_Heap_Shutdown( void );

/* Returns 0 on failure. alignment must be a power of two, or 0 for the default. */
void *RE_Heap_Alloc( ReUint64 size, ReUint64 alignment );

/* Grows or shrinks, moving only when it has to. A null pointer allocates; a new size of zero
 * frees and returns null. Returns 0 on failure, leaving the original block intact.
 */
void *RE_Heap_Realloc( void *block, ReUint64 newSize, ReUint64 alignment );

/* @warning A pointer this heap did not hand out is a programmer error, and is reported as one
 *          rather than quietly ignored. A corrupted heap has already lost; the value is in the
 *          stack trace at the point of detection.
 */
void RE_Heap_Free( void *block );

/* Bytes actually available at this pointer, which is at least what was asked for.
 *
 * The uniform allocator interface takes the size from the caller so that arenas can stay
 * header-free; this exists for the occasional caller that genuinely does not have it.
 */
ReUint64 RE_Heap_AllocationSize( const void *block );

/* What a request would really cost, before making it. */
ReUint64 RE_Heap_Quantize( ReUint64 size, ReUint64 alignment );

/* Returns cached-but-unused memory to the OS. Call at a quiet point - a level transition, a
 * suspend - never mid-frame.
 */
void RE_Heap_Trim( void );

ReHeapStats RE_Heap_GetStats( void );

/* The heap through the uniform interface. oldSize is accepted and ignored, since unlike an arena
 * this can recover the size from the pointer.
 */
ReAllocator RE_Heap_AsAllocator( void );
