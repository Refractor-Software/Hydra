/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryPool.h>

#include <assert.h>

#include <RE/Foundation/FoundationMemory.h>

void
RE_Pool_Init (RePool *p, void *memory, ReUint64 memorySize, ReUint64 blockSize, ReUint64 blockAlignment)
{
    assert (blockAlignment != 0 && (blockAlignment & (blockAlignment - 1)) == 0);

    ReUint64 rawBase     = (ReUint64) memory;
    ReUint64 alignedBase = (ReUint64) RE_Memory_AlignUp ((ReUint64) rawBase, blockAlignment);
    ReUint64 lostBytes  = (ReUint64) (alignedBase - rawBase);

    ReUint64 blockStride = blockSize > sizeof (void *) ? blockSize : sizeof (void *);
    blockStride        = RE_Memory_AlignUp (blockStride, blockAlignment);

    assert (memorySize > lostBytes);
    ReUint64 usableSize = memorySize - lostBytes;

    ReUint64 blockCount = usableSize / blockStride;
    assert (blockCount > 0);

    p->base        = (void *) alignedBase;
    p->blockStride = blockStride;
    p->blockCount  = blockCount;

    ReUint8 *cursor = (ReUint8 *) p->base;
    for (ReUint64 i = 0; i < blockCount; i += 1)
    {
        void *next = (i + 1 < blockCount) ? (void *) (cursor + blockStride) : 0;
        *(void **) cursor = next;
        cursor += blockStride;
    }

    p->freeList = p->base;
}

void *
RE_Pool_Alloc (RePool *p)
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
RE_Pool_Free (RePool *p, void *ptr)
{
    ReUint64 base = (ReUint64) p->base;
    ReUint64 addr = (ReUint64) ptr;
    ReUint64 end  = base + (ReUint64) (p->blockStride * p->blockCount);

    assert (addr >= base && addr < end && (addr - base) % p->blockStride == 0);

    *(void **) ptr = p->freeList;
    p->freeList    = ptr;
}

internal void *
Pool_AllocatorAlloc (void *ctx, ReUint64 size)
{
    RePool *p = (RePool *) ctx;

    if (size > p->blockStride)
    {
        return 0;
    }

    return RE_Pool_Alloc (p);
}

internal void
Pool_AllocatorFree (void *ctx, void *ptr)
{
    RE_Pool_Free ((RePool *) ctx, ptr);
}

ReAllocator
RE_Pool_AsAllocator (RePool *p)
{
    ReAllocator allocator;
    allocator._context = p;
    allocator._alloc   = Pool_AllocatorAlloc;
    allocator._free    = Pool_AllocatorFree;

    return allocator;
}
