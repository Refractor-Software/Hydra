/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryFrame.h>

#include <assert.h>

#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

RE_INTERNAL ReFrameThreadArena *
Frame_SlotAt( ReFrameAllocator *allocator, ReUint32 bufferIndex, ReUint32 threadIndex )
{
    return &allocator->arenas[(ReUint64) bufferIndex * allocator->threadCount + threadIndex];
}

ReBool
RE_Frame_Init( ReFrameAllocator *allocator, ReUint32 bufferCount, ReUint32 threadCount,
    ReUint64 arenaReserveSize )
{
    if ( !allocator || bufferCount == 0 || threadCount == 0 || arenaReserveSize == 0 )
    {
        return RE_False;
    }

    if ( bufferCount > RE_FRAME_MAX_BUFFERS || threadCount > RE_FRAME_MAX_THREADS )
    {
        return RE_False;
    }

    RE_Memory_Zero( allocator, sizeof( *allocator ) );

    ReUint64 slotCount = (ReUint64) bufferCount * threadCount;

    /* From the metadata allocator rather than the heap: the frame allocator is part of the memory
     * system's own scaffolding and may be created before a general allocator exists. It is also
     * page-aligned, which is what keeps each arena on its own cache line.
     */
    allocator->arenas = (ReFrameThreadArena *) RE_MemoryMetadata_Alloc(
        slotCount * sizeof( ReFrameThreadArena ), RE_CACHE_LINE_SIZE );

    if ( !allocator->arenas )
    {
        return RE_False;
    }

    allocator->bufferCount = bufferCount;
    allocator->threadCount = threadCount;

    for ( ReUint64 i = 0; i < slotCount; i += 1 )
    {
        if ( !RE_Arena_InitVirtual( &allocator->arenas[i].arena, arenaReserveSize ) )
        {
            /* Unwind so a partial failure does not leave arenas half-created behind a pointer
             * the caller is about to discard.
             */
            for ( ReUint64 undo = 0; undo < i; undo += 1 )
            {
                RE_Arena_Shutdown( &allocator->arenas[undo].arena );
            }

            allocator->arenas = 0;

            return RE_False;
        }
    }

    return RE_True;
}

void
RE_Frame_Shutdown( ReFrameAllocator *allocator )
{
    if ( !allocator || !allocator->arenas )
    {
        return;
    }

    ReUint64 slotCount = (ReUint64) allocator->bufferCount * allocator->threadCount;

    for ( ReUint64 i = 0; i < slotCount; i += 1 )
    {
        RE_Arena_Shutdown( &allocator->arenas[i].arena );
    }

    /* The slot array itself came from the metadata allocator, which is permanent by design and
     * has no free. It is reclaimed at process teardown.
     */
    RE_Memory_Zero( allocator, sizeof( *allocator ) );
}

void
RE_Frame_SetOverflowAllocator( ReFrameAllocator *allocator, ReAllocator overflow )
{
    assert( allocator );

    allocator->overflowAllocator    = overflow;
    allocator->hasOverflowAllocator = RE_True;
}

void
RE_Frame_BeginFrame( ReFrameAllocator *allocator, ReUint64 frameIndex )
{
    assert( allocator && allocator->arenas );

    allocator->frameIndex    = frameIndex;
    allocator->currentBuffer = (ReUint32) ( frameIndex % allocator->bufferCount );

    /* Safe because this buffer was last written bufferCount frames ago, and with bufferCount set
     * to pipeline depth + 1 every consumer of that data has finished by now.
     */
    for ( ReUint32 thread = 0; thread < allocator->threadCount; thread += 1 )
    {
        RE_Arena_Reset( &Frame_SlotAt( allocator, allocator->currentBuffer, thread )->arena );
    }
}

void *
RE_Frame_Alloc( ReFrameAllocator *allocator, ReUint32 threadIndex, ReUint64 size, ReUint64 alignment )
{
    assert( allocator && allocator->arenas );
    assert( threadIndex < allocator->threadCount && "thread index outside the configured range" );

    ReArena *arena = &Frame_SlotAt( allocator, allocator->currentBuffer, threadIndex )->arena;

    void *block = RE_Arena_Alloc( arena, size, alignment );
    if ( block )
    {
        return block;
    }

    /* Exhausted. In development there is normally no overflow allocator, so this returns 0 and
     * the caller fails immediately - which is the point, because a frame budget that has been
     * exceeded is a regression worth finding now rather than shipping.
     */
    if ( !allocator->hasOverflowAllocator )
    {
        return 0;
    }

    RE_Atomic_FetchAddUint64( &allocator->overflowCount, 1 );
    RE_Atomic_FetchAddUint64( &allocator->overflowBytes, size );

    return RE_Memory_Alloc( &allocator->overflowAllocator, size, alignment );
}

ReArena *
RE_Frame_Arena( ReFrameAllocator *allocator, ReUint32 threadIndex )
{
    assert( allocator && allocator->arenas );
    assert( threadIndex < allocator->threadCount );

    return &Frame_SlotAt( allocator, allocator->currentBuffer, threadIndex )->arena;
}

ReUint64
RE_Frame_HighWater( const ReFrameAllocator *allocator )
{
    assert( allocator );

    ReUint64 peak      = 0;
    ReUint64 slotCount = (ReUint64) allocator->bufferCount * allocator->threadCount;

    for ( ReUint64 i = 0; i < slotCount; i += 1 )
    {
        ReUint64 arenaPeak = RE_Arena_HighWater( &allocator->arenas[i].arena );

        if ( arenaPeak > peak )
        {
            peak = arenaPeak;
        }
    }

    return peak;
}
