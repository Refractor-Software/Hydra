/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

#include <RE/Foundation/FoundationMemoryAllocator.h>

/*
    FoundationMemoryPool.h

    A fixed-block-size, free-list-backed allocator over a caller-supplied block of memory - never
    allocates from the OS itself. O(1) alloc/free of same-sized blocks; good fit for the kind of
    fixed-size-object churn (entities, components, etc.) a game engine does constantly.
*/

typedef struct RePool
{
    void  *base;
    ReUint64  blockStride; /* real per-block stride actually used internally - may exceed the
                          * requested blockSize (rounded up for the intrusive free-list pointer
                          * and/or alignment). */
    ReUint64  blockCount;
    void  *freeList;    /* intrusive singly-linked free list; nullptr when exhausted. */
} RePool;

/* Asserts if memorySize is too small to hold even one aligned block - that's a config-time
 * programmer error (the pool was sized wrong), not a runtime condition.
 */
void RE_Pool_Init (RePool *p, void *memory, ReUint64 memorySize, ReUint64 blockSize, ReUint64 blockAlignment);

/* Returns nullptr if the pool is exhausted - expected/checkable, not a programmer error. */
void * RE_Pool_Alloc (RePool *p);

/* Asserts if ptr doesn't actually belong to this pool (out of range or misaligned to the block
 * stride) - catches the most common pool-misuse footgun (foreign pointer, double-free-shaped
 * corruption) cheaply.
 */
void RE_Pool_Free (RePool *p, void *ptr);

/* Bridges into the generic ReAllocator interface. The bridged _alloc returns nullptr (not
 * assert) if the requested size doesn't fit a block - through the generic interface, "can't
 * satisfy this request" and "out of memory" are indistinguishable to the caller, same as any
 * other ReAllocator failure.
 */
ReAllocator RE_Pool_AsAllocator (RePool *p);
