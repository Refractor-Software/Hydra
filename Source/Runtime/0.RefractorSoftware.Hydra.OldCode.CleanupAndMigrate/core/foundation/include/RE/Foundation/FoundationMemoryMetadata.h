/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationMemoryMetadata.h

    Backing store for the allocator's own bookkeeping: block descriptors, side tables, thread
    caches, bitmaps.

    This exists for one reason - the allocator cannot allocate its own metadata with itself, that
    recurses. So this takes memory straight from the virtual memory layer and depends on nothing
    above it. It is live before the heap initialises and must stay that way; do not make it use
    anything that might one day want the heap.

    Allocations here are permanent. There is no free, because metadata lives for the process
    lifetime and a free list would be bookkeeping about bookkeeping. RE_MemoryMetadata_Shutdown
    releases the lot at teardown.

    @threadsafe Yes. Guarded by an internal spin lock; contention is negligible because this is
                only touched when the heap grows a new block or a thread starts.
*/

/* Returns 0 if the underlying reservation failed, which the caller should treat as fatal - there
 * is no meaningful way to continue without metadata.
 *
 * alignment must be a power of two.
 */
void *RE_MemoryMetadata_Alloc( ReUint64 size, ReUint64 alignment );

/* Releases every chunk. Only valid once nothing holds a metadata pointer, i.e. after the heap and
 * every thread cache are gone.
 */
void RE_MemoryMetadata_Shutdown( void );

/*
    Metadata is real memory and a lean-looking design can lose a surprising amount to it -
    descriptors for hundreds of thousands of blocks, a side table proportional to touched address
    space, a cache per job worker. Report it as its own line item; anything not counted here shows
    up later as an unexplained gap between our numbers and the OS's.
*/
typedef struct ReMemoryMetadataStats
{
    ReUint64 bytesReserved;  /* address space claimed from the VM layer */
    ReUint64 bytesCommitted; /* physical pages actually backed */
    ReUint64 bytesUsed;      /* handed out to callers, excluding alignment padding */
    ReUint64 chunkCount;
} ReMemoryMetadataStats;

ReMemoryMetadataStats RE_MemoryMetadata_GetStats( void );
