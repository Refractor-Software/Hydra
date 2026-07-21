/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include "foundation/primitive/foundation_primitive_types.h"

#include "foundation/memory/foundation_memory_allocator.h"

/*
    foundation_memory_pool.h

    A fixed-block-size, free-list-backed allocator over a caller-supplied block of memory - never
    allocates from the OS itself. O(1) alloc/free of same-sized blocks; good fit for the kind of
    fixed-size-object churn (entities, components, etc.) a game engine does constantly.
*/

typedef struct pool
{
    void  *base;
    usize  blockStride; /* real per-block stride actually used internally - may exceed the
                          * requested blockSize (rounded up for the intrusive free-list pointer
                          * and/or alignment). */
    usize  blockCount;
    void  *freeList;    /* intrusive singly-linked free list; nullptr when exhausted. */
} pool;

/* Asserts if memorySize is too small to hold even one aligned block - that's a config-time
 * programmer error (the pool was sized wrong), not a runtime condition.
 */
void pool_init (pool *p, void *memory, usize memorySize, usize blockSize, usize blockAlignment);

/* Returns nullptr if the pool is exhausted - expected/checkable, not a programmer error. */
void * pool_alloc (pool *p);

/* Asserts if ptr doesn't actually belong to this pool (out of range or misaligned to the block
 * stride) - catches the most common pool-misuse footgun (foreign pointer, double-free-shaped
 * corruption) cheaply.
 */
void pool_free (pool *p, void *ptr);

/* Bridges into the generic memory_allocator interface. The bridged _alloc returns nullptr (not
 * assert) if the requested size doesn't fit a block - through the generic interface, "can't
 * satisfy this request" and "out of memory" are indistinguishable to the caller, same as any
 * other memory_allocator failure.
 */
memory_allocator pool_as_allocator (pool *p);
