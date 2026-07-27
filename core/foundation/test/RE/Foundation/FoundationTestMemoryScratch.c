/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <RE/Foundation/FoundationMemoryScratch.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

/* Stands in for the failure this module exists to prevent: a helper that needs temporaries of its
 * own while its caller is holding a live buffer. Declaring the caller's arena as a conflict is
 * what guarantees it cannot be handed the same one.
 */
internal ReArena *
Scratch_TestHelper( ReArena *callerArena )
{
    ReArena  *conflicts[1] = { callerArena };
    ReScratch scratch      = RE_Scratch_Acquire( conflicts, 1 );

    ReArena *acquired = RE_Scratch_Arena( &scratch );

    if ( acquired )
    {
        void *temporary = RE_Scratch_Alloc( &scratch, 4096, 16 );
        if ( temporary )
        {
            RE_Memory_Set( temporary, 0xEE, 4096 );
        }
    }

    RE_Scratch_Release( &scratch );

    return acquired;
}

void
RE_Test_MemoryScratch( void )
{
    /* --- a helper never receives the arena its caller is holding --- */
    {
        ReScratch outer = RE_Scratch_Acquire( 0, 0 );
        RE_TEST_CHECK_NOT_NULL( RE_Scratch_Arena( &outer ) );

        ReUint8 *buffer = (ReUint8 *) RE_Scratch_Alloc( &outer, 1024, 16 );
        RE_TEST_CHECK_NOT_NULL( buffer );
        RE_Memory_Set( buffer, 0xAB, 1024 );

        ReArena *helperArena = Scratch_TestHelper( RE_Scratch_Arena( &outer ) );

        RE_TEST_CHECK_NOT_NULL( helperArena );
        RE_TEST_CHECK( helperArena != RE_Scratch_Arena( &outer ) );

        /* The caller's buffer is intact. Under a single shared scratch this is precisely the
         * check that would fail, and it would fail silently.
         */
        RE_TEST_CHECK_EQ_UINT( buffer[0], 0xAB );
        RE_TEST_CHECK_EQ_UINT( buffer[1023], 0xAB );

        RE_Scratch_Release( &outer );
        RE_TEST_CHECK_NULL( RE_Scratch_Arena( &outer ) );
    }

    /* --- release rewinds to the acquisition point, not to the start of the arena --- */
    {
        ReScratch first = RE_Scratch_Acquire( 0, 0 );
        RE_TEST_CHECK_NOT_NULL( first.arena );

        RE_TEST_CHECK_NOT_NULL( RE_Scratch_Alloc( &first, 256, 16 ) );
        ReUint64 usedInside = RE_Arena_Used( first.arena );
        RE_TEST_CHECK( usedInside >= 256 );

        ReArena *arena = first.arena;
        RE_Scratch_Release( &first );

        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( arena ), 0 );
    }

    /* --- an arena released by one scope is available to the next --- */
    {
        ReScratch a       = RE_Scratch_Acquire( 0, 0 );
        ReArena  *arenaA  = RE_Scratch_Arena( &a );
        RE_Scratch_Release( &a );

        ReScratch b      = RE_Scratch_Acquire( 0, 0 );
        ReArena  *arenaB = RE_Scratch_Arena( &b );
        RE_Scratch_Release( &b );

        RE_TEST_CHECK( arenaA == arenaB );
    }

    /* --- simultaneous holders each get a distinct arena --- */
    {
        ReScratch held[RE_SCRATCH_ARENA_COUNT];

        for ( ReUint32 i = 0; i < RE_SCRATCH_ARENA_COUNT; i += 1 )
        {
            held[i] = RE_Scratch_Acquire( 0, 0 );
            RE_TEST_CHECK_NOT_NULL( RE_Scratch_Arena( &held[i] ) );
        }

        for ( ReUint32 i = 0; i < RE_SCRATCH_ARENA_COUNT; i += 1 )
        {
            for ( ReUint32 j = i + 1; j < RE_SCRATCH_ARENA_COUNT; j += 1 )
            {
                RE_TEST_CHECK( RE_Scratch_Arena( &held[i] ) != RE_Scratch_Arena( &held[j] ) );
            }
        }

        for ( ReUint32 i = 0; i < RE_SCRATCH_ARENA_COUNT; i += 1 )
        {
            RE_Scratch_Release( &held[i] );
        }
    }

    /* --- releasing an already-released scratch is harmless --- */
    {
        ReScratch scratch = RE_Scratch_Acquire( 0, 0 );
        RE_Scratch_Release( &scratch );
        RE_Scratch_Release( &scratch );
    }
}
