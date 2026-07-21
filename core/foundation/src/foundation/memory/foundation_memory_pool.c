/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "foundation/memory/foundation_memory_pool.h"

#include <assert.h>

#include "foundation/memory/foundation_memory.h"

void
pool_init (pool *p, void *memory, usize memorySize, usize blockSize, usize blockAlignment)
{
    assert (blockAlignment != 0 && (blockAlignment & (blockAlignment - 1)) == 0);

    uptr rawBase     = (uptr) memory;
    uptr alignedBase = (uptr) memory_align_up ((usize) rawBase, blockAlignment);
    usize lostBytes  = (usize) (alignedBase - rawBase);

    usize blockStride = blockSize > sizeof (void *) ? blockSize : sizeof (void *);
    blockStride        = memory_align_up (blockStride, blockAlignment);

    assert (memorySize > lostBytes);
    usize usableSize = memorySize - lostBytes;

    usize blockCount = usableSize / blockStride;
    assert (blockCount > 0);

    p->base        = (void *) alignedBase;
    p->blockStride = blockStride;
    p->blockCount  = blockCount;

    u8 *cursor = (u8 *) p->base;
    for (usize i = 0; i < blockCount; i += 1)
    {
        void *next = (i + 1 < blockCount) ? (void *) (cursor + blockStride) : 0;
        *(void **) cursor = next;
        cursor += blockStride;
    }

    p->freeList = p->base;
}

void *
pool_alloc (pool *p)
{
    if (!p->freeList)
    {
        return 0;
    }

    void *block = p->freeList;
    p->freeList = *(void **) block;

    return block;
}

void
pool_free (pool *p, void *ptr)
{
    uptr base = (uptr) p->base;
    uptr addr = (uptr) ptr;
    uptr end  = base + (uptr) (p->blockStride * p->blockCount);

    assert (addr >= base && addr < end && (addr - base) % p->blockStride == 0);

    *(void **) ptr = p->freeList;
    p->freeList    = ptr;
}

static void *
pool_allocator_alloc (void *ctx, usize size)
{
    pool *p = (pool *) ctx;

    if (size > p->blockStride)
    {
        return 0;
    }

    return pool_alloc (p);
}

static void
pool_allocator_free (void *ctx, void *ptr)
{
    pool_free ((pool *) ctx, ptr);
}

memory_allocator
pool_as_allocator (pool *p)
{
    memory_allocator allocator;
    allocator._context = p;
    allocator._alloc   = pool_allocator_alloc;
    allocator._free    = pool_allocator_free;

    return allocator;
}
