/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryScratch.h>

#include <assert.h>

#include <RE/Foundation/FoundationCompiler.h>

/*
    One set of arenas per thread. Thread-local rather than shared because the whole value of an
    arena is that allocation costs an align and an add - putting a lock in front of that would
    cost more than the work it guards, and a shared cursor without one is a data race.
*/

typedef struct ReScratchThreadState
{
    ReArena arenas[RE_SCRATCH_ARENA_COUNT];

    /* Held rather than free. Checked on acquisition so that a caller's arena is never handed to a
     * callee even when the callee forgot to declare it as a conflict.
     */
    ReBool inUse[RE_SCRATCH_ARENA_COUNT];

    ReBool initialized;
} ReScratchThreadState;

global RE_THREAD_LOCAL ReScratchThreadState gScratchState;

internal ReBool
Scratch_EnsureInitialized( void )
{
    if ( gScratchState.initialized )
    {
        return RE_True;
    }

    for ( ReUint32 i = 0; i < RE_SCRATCH_ARENA_COUNT; i += 1 )
    {
        if ( !RE_Arena_InitVirtual( &gScratchState.arenas[i], RE_SCRATCH_ARENA_RESERVE_SIZE ) )
        {
            /* Unwind whatever was created, so a partial failure does not leave half a pool
             * behind for the next call to trip over.
             */
            for ( ReUint32 undo = 0; undo < i; undo += 1 )
            {
                RE_Arena_Shutdown( &gScratchState.arenas[undo] );
            }

            return RE_False;
        }
    }

    gScratchState.initialized = RE_True;

    return RE_True;
}

internal ReBool
Scratch_IsConflicting( const ReArena *candidate, ReArena *const *conflicts, ReUint32 conflictCount )
{
    for ( ReUint32 i = 0; i < conflictCount; i += 1 )
    {
        if ( conflicts[i] == candidate )
        {
            return RE_True;
        }
    }

    return RE_False;
}

ReScratch
RE_Scratch_Acquire( ReArena *const *conflicts, ReUint32 conflictCount )
{
    ReScratch scratch;
    scratch.arena           = 0;
    scratch.marker.offset   = 0;
    scratch.marker.sequence = 0;

    if ( !Scratch_EnsureInitialized() )
    {
        return scratch;
    }

    for ( ReUint32 i = 0; i < RE_SCRATCH_ARENA_COUNT; i += 1 )
    {
        ReArena *candidate = &gScratchState.arenas[i];

        if ( gScratchState.inUse[i] )
        {
            continue;
        }

        if ( Scratch_IsConflicting( candidate, conflicts, conflictCount ) )
        {
            continue;
        }

        gScratchState.inUse[i] = RE_True;

        scratch.arena  = candidate;
        scratch.marker = RE_Arena_Mark( candidate );

        return scratch;
    }

    /* Deeper conflicting nesting than the pool supports. Returning an arena the caller is already
     * using would produce exactly the silent corruption this module exists to prevent, so it
     * returns nothing instead and lets the null crash at the point of misuse.
     */
    assert( 0 && "scratch pool exhausted - too many simultaneous conflicting scratches" );

    return scratch;
}

void
RE_Scratch_Release( ReScratch *scratch )
{
    if ( !scratch || !scratch->arena )
    {
        return;
    }

    RE_Arena_Rewind( scratch->arena, scratch->marker );

    for ( ReUint32 i = 0; i < RE_SCRATCH_ARENA_COUNT; i += 1 )
    {
        if ( &gScratchState.arenas[i] == scratch->arena )
        {
            gScratchState.inUse[i] = RE_False;
            break;
        }
    }

    scratch->arena = 0;
}

void *
RE_Scratch_Alloc( ReScratch *scratch, ReUint64 size, ReUint64 alignment )
{
    assert( scratch && scratch->arena && "allocating from a scratch that was never acquired" );

    return RE_Arena_Alloc( scratch->arena, size, alignment );
}

void
RE_Scratch_ThreadShutdown( void )
{
    if ( !gScratchState.initialized )
    {
        return;
    }

    for ( ReUint32 i = 0; i < RE_SCRATCH_ARENA_COUNT; i += 1 )
    {
        assert( !gScratchState.inUse[i] && "scratch still held at thread shutdown" );
        RE_Arena_Shutdown( &gScratchState.arenas[i] );
    }

    gScratchState.initialized = RE_False;
}
