/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryMetadata.h>

#include <RE/Foundation/FoundationMemoryAllocator.h>
#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationSpinLock.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

/*
    A chain of virtual reservations, bump-allocated. Each chunk reserves a large range and commits
    only as far as the cursor has advanced, so an engine that registers few blocks pays for few
    pages while one that registers many never needs a bigger chunk.

    The chunk header lives in the chunk's own first bytes. It has to live somewhere, and putting it
    anywhere else would mean allocating metadata for the metadata allocator.
*/

#define RE_METADATA_CHUNK_RESERVE_SIZE ( 4 * 1024 * 1024 )
#define RE_METADATA_COMMIT_STEP        ( 64 * 1024 )

typedef struct ReMetadataChunk
{
    ReVirtualRegion         region;
    ReUint64                cursor;       /* offset of the next free byte */
    ReUint64                committedEnd; /* offset one past the last committed byte */
    struct ReMetadataChunk *next;
} ReMetadataChunk;

RE_GLOBAL ReSpinLock       gMetadataLock;
RE_GLOBAL ReMetadataChunk *gMetadataChunkHead;
RE_GLOBAL ReUint64         gMetadataBytesReserved;
RE_GLOBAL ReUint64         gMetadataBytesCommitted;
RE_GLOBAL ReUint64         gMetadataBytesUsed;
RE_GLOBAL ReUint64         gMetadataChunkCount;
RE_GLOBAL ReBool           gMetadataLockInitialized;

/* The lock itself needs initialising before first use, and there is no init entry point to do it
 * in - metadata is used during bootstrap, before anything has had a chance to call an init.
 *
 * Zero-initialised static storage means the lock starts unlocked, which is exactly the state
 * RE_SpinLock_Init would put it in. So the flag is really only documenting that the zero state is
 * deliberate rather than accidental.
 */
RE_INTERNAL void
Metadata_EnsureLock( void )
{
    if ( !gMetadataLockInitialized )
    {
        RE_SpinLock_Init( &gMetadataLock );
        gMetadataLockInitialized = RE_True;
    }
}

/* Caller holds the lock. Returns 0 if the reservation or the first commit failed. */
RE_INTERNAL ReMetadataChunk *
Metadata_PushChunk( ReUint64 minimumSize )
{
    ReUint64 reserveSize = RE_METADATA_CHUNK_RESERVE_SIZE;
    if ( minimumSize > reserveSize )
    {
        reserveSize = minimumSize;
    }

    ReVirtualRegion region = RE_VirtualMemory_Reserve( reserveSize, 0 );
    if ( !region.base )
    {
        return 0;
    }

    /* Commit enough for the header plus the first step. Committing the whole reservation up front
     * would defeat the point of reserving generously.
     */
    ReUint64 initialCommit = RE_METADATA_COMMIT_STEP;
    if ( initialCommit > region.size )
    {
        initialCommit = region.size;
    }

    if ( !RE_VirtualMemory_Commit( &region, 0, initialCommit ) )
    {
        RE_VirtualMemory_Release( &region );

        return 0;
    }

    ReMetadataChunk *chunk = (ReMetadataChunk *) region.base;
    chunk->region       = region;
    chunk->cursor       = sizeof( ReMetadataChunk );
    chunk->committedEnd = initialCommit;
    chunk->next         = gMetadataChunkHead;

    gMetadataChunkHead = chunk;

    gMetadataBytesReserved  += region.size;
    gMetadataBytesCommitted += initialCommit;
    gMetadataChunkCount     += 1;

    return chunk;
}

/* Caller holds the lock. Returns 0 if the chunk cannot satisfy the request. */
RE_INTERNAL void *
Metadata_AllocFromChunk( ReMetadataChunk *chunk, ReUint64 size, ReUint64 alignment )
{
    ReUint64 aligned = RE_Memory_AlignUp( (ReUint64) chunk->region.base + chunk->cursor, alignment );
    ReUint64 offset  = aligned - (ReUint64) chunk->region.base;

    if ( offset + size > chunk->region.size )
    {
        return 0;
    }

    if ( offset + size > chunk->committedEnd )
    {
        ReUint64 needed = RE_Memory_AlignUp( offset + size, RE_METADATA_COMMIT_STEP );
        if ( needed > chunk->region.size )
        {
            needed = chunk->region.size;
        }

        if ( !RE_VirtualMemory_Commit( &chunk->region, chunk->committedEnd, needed - chunk->committedEnd ) )
        {
            return 0;
        }

        gMetadataBytesCommitted += needed - chunk->committedEnd;
        chunk->committedEnd = needed;
    }

    chunk->cursor = offset + size;

    return (void *) aligned;
}

void *
RE_MemoryMetadata_Alloc( ReUint64 size, ReUint64 alignment )
{
    if ( size == 0 )
    {
        return 0;
    }

    if ( alignment == 0 )
    {
        alignment = RE_MEMORY_DEFAULT_ALIGNMENT;
    }

    Metadata_EnsureLock();
    RE_SpinLock_Acquire( &gMetadataLock );

    void *result = 0;

    if ( gMetadataChunkHead )
    {
        result = Metadata_AllocFromChunk( gMetadataChunkHead, size, alignment );
    }

    if ( !result )
    {
        /* Only the newest chunk is ever retried. Older ones are near-full by construction, and
         * walking the whole chain to reclaim a few tail bytes would trade a bounded waste for an
         * unbounded search on every allocation.
         */
        ReMetadataChunk *chunk = Metadata_PushChunk( size + alignment + sizeof( ReMetadataChunk ) );
        if ( chunk )
        {
            result = Metadata_AllocFromChunk( chunk, size, alignment );
        }
    }

    if ( result )
    {
        gMetadataBytesUsed += size;
        RE_Memory_Zero( result, size );
    }

    RE_SpinLock_Release( &gMetadataLock );

    return result;
}

void
RE_MemoryMetadata_Shutdown( void )
{
    Metadata_EnsureLock();
    RE_SpinLock_Acquire( &gMetadataLock );

    ReMetadataChunk *chunk = gMetadataChunkHead;
    while ( chunk )
    {
        /* The chunk header lives inside the memory being released, so the next pointer has to be
         * read before the release, not after.
         */
        ReMetadataChunk *next   = chunk->next;
        ReVirtualRegion  region = chunk->region;

        RE_VirtualMemory_Release( &region );

        chunk = next;
    }

    gMetadataChunkHead      = 0;
    gMetadataBytesReserved  = 0;
    gMetadataBytesCommitted = 0;
    gMetadataBytesUsed      = 0;
    gMetadataChunkCount     = 0;

    RE_SpinLock_Release( &gMetadataLock );
}

ReMemoryMetadataStats
RE_MemoryMetadata_GetStats( void )
{
    Metadata_EnsureLock();
    RE_SpinLock_Acquire( &gMetadataLock );

    ReMemoryMetadataStats stats;
    stats.bytesReserved  = gMetadataBytesReserved;
    stats.bytesCommitted = gMetadataBytesCommitted;
    stats.bytesUsed      = gMetadataBytesUsed;
    stats.chunkCount     = gMetadataChunkCount;

    RE_SpinLock_Release( &gMetadataLock );

    return stats;
}
