/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <RE/Foundation/FoundationMemoryArena.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

void
RE_Test_MemoryArena( void )
{
    /* --- fixed mode: bounded by the caller's buffer, and says so rather than overrunning --- */
    {
        ReUint8 buffer[1024];
        ReArena arena;

        RE_TEST_CHECK( RE_Arena_InitFixed( &arena, buffer, sizeof( buffer ) ) );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( &arena ), 0 );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_Remaining( &arena ), sizeof( buffer ) );

        void *a = RE_Arena_Alloc( &arena, 100, 16 );
        RE_TEST_CHECK_NOT_NULL( a );
        RE_TEST_CHECK_EQ_UINT( (ReUint64) a & 15, 0 );

        void *b = RE_Arena_Alloc( &arena, 100, 16 );
        RE_TEST_CHECK_NOT_NULL( b );
        RE_TEST_CHECK( b != a );

        /* Consecutive allocations are adjacent - the spatial locality that makes arenas worth
         * reaching for in the first place.
         */
        RE_TEST_CHECK( (ReUint8 *) b >= (ReUint8 *) a + 100 );
        RE_TEST_CHECK( (ReUint8 *) b < (ReUint8 *) a + 100 + 16 );

        /* Exhaustion is a checkable condition, not an assert - a bounded budget doing its job. */
        RE_TEST_CHECK_NULL( RE_Arena_Alloc( &arena, sizeof( buffer ), 16 ) );

        /* ...and the failed request must not have disturbed the cursor. */
        ReUint64 usedBefore = RE_Arena_Used( &arena );
        RE_TEST_CHECK_NULL( RE_Arena_Alloc( &arena, sizeof( buffer ), 16 ) );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( &arena ), usedBefore );

        RE_Arena_Shutdown( &arena );
    }

    /* --- alignment, including over-alignment --- */
    {
        ReUint8 buffer[4096];
        ReArena arena;
        RE_TEST_CHECK( RE_Arena_InitFixed( &arena, buffer, sizeof( buffer ) ) );

        /* A one-byte allocation first, so the cursor is deliberately at an awkward offset. */
        RE_TEST_CHECK_NOT_NULL( RE_Arena_Alloc( &arena, 1, 1 ) );

        for ( ReUint64 alignment = 1; alignment <= 256; alignment *= 2 )
        {
            void *p = RE_Arena_Alloc( &arena, 8, alignment );
            RE_TEST_CHECK_NOT_NULL( p );
            RE_TEST_CHECK_EQ_UINT( (ReUint64) p & ( alignment - 1 ), 0 );
        }

        RE_Arena_Shutdown( &arena );
    }

    /* --- virtual mode: a huge reservation costs nothing until touched --- */
    {
        ReArena arena;
        RE_TEST_CHECK( RE_Arena_InitVirtual( &arena, 256ull * 1024ull * 1024ull ) );
        RE_TEST_CHECK( arena.capacity >= 256ull * 1024ull * 1024ull );
        RE_TEST_CHECK_EQ_UINT( arena.committed, 0 );

        ReUint8 *p = (ReUint8 *) RE_Arena_Alloc( &arena, 1, 16 );
        RE_TEST_CHECK_NOT_NULL( p );
        RE_TEST_CHECK( arena.committed > 0 );

        /* Commit grows to meet a large allocation rather than failing. */
        ReUint8 *big = (ReUint8 *) RE_Arena_Alloc( &arena, 8ull * 1024ull * 1024ull, 16 );
        RE_TEST_CHECK_NOT_NULL( big );

        if ( big )
        {
            big[0]                          = 0x42;
            big[8ull * 1024ull * 1024ull - 1] = 0x43;
            RE_TEST_CHECK_EQ_UINT( big[0], 0x42 );
            RE_TEST_CHECK_EQ_UINT( big[8ull * 1024ull * 1024ull - 1], 0x43 );
        }

        /* Reset keeps the committed pages: decommitting every reset would pay page faults and
         * first-touch zeroing forever.
         */
        ReUint64 committedBeforeReset = arena.committed;
        RE_Arena_Reset( &arena );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( &arena ), 0 );
        RE_TEST_CHECK_EQ_UINT( arena.committed, committedBeforeReset );

        /* Trim is the explicit path that does give them back. */
        RE_Arena_Trim( &arena );
        RE_TEST_CHECK( arena.committed < committedBeforeReset );

        RE_Arena_Shutdown( &arena );
        RE_TEST_CHECK_NULL( arena.base );
    }

    /* --- high-water survives a reset, since it is what sizing decisions are made from --- */
    {
        ReUint8 buffer[1024];
        ReArena arena;
        RE_TEST_CHECK( RE_Arena_InitFixed( &arena, buffer, sizeof( buffer ) ) );

        RE_TEST_CHECK_NOT_NULL( RE_Arena_Alloc( &arena, 500, 16 ) );
        ReUint64 peak = RE_Arena_HighWater( &arena );
        RE_TEST_CHECK( peak >= 500 );

        RE_Arena_Reset( &arena );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( &arena ), 0 );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_HighWater( &arena ), peak );

        RE_Arena_Shutdown( &arena );
    }

    /* --- markers and LIFO rewind --- */
    {
        ReUint8 buffer[1024];
        ReArena arena;
        RE_TEST_CHECK( RE_Arena_InitFixed( &arena, buffer, sizeof( buffer ) ) );

        RE_TEST_CHECK_NOT_NULL( RE_Arena_Alloc( &arena, 64, 16 ) );

        ReArenaMarker outer = RE_Arena_Mark( &arena );
        ReUint64      atOuter = RE_Arena_Used( &arena );

        RE_TEST_CHECK_NOT_NULL( RE_Arena_Alloc( &arena, 64, 16 ) );

        ReArenaMarker inner = RE_Arena_Mark( &arena );
        RE_TEST_CHECK_NOT_NULL( RE_Arena_Alloc( &arena, 64, 16 ) );

        /* Nested rewinds unwind in reverse order, each returning to its own position. */
        RE_Arena_Rewind( &arena, inner );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( &arena ), inner.offset );

        RE_Arena_Rewind( &arena, outer );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( &arena ), atOuter );

        /* Memory handed out after a rewind reuses the same addresses. */
        void *reused = RE_Arena_Alloc( &arena, 64, 16 );
        RE_TEST_CHECK_NOT_NULL( reused );
        RE_TEST_CHECK_EQ_UINT( (ReUint64) ( (ReUint8 *) reused - arena.base ), atOuter );

        RE_Arena_Shutdown( &arena );
    }

    /* --- freeing the most recent allocation reclaims it; anything older is a no-op --- */
    {
        ReUint8 buffer[1024];
        ReArena arena;
        RE_TEST_CHECK( RE_Arena_InitFixed( &arena, buffer, sizeof( buffer ) ) );

        void    *first     = RE_Arena_Alloc( &arena, 64, 16 );
        ReUint64 afterFirst = RE_Arena_Used( &arena );
        void    *second    = RE_Arena_Alloc( &arena, 64, 16 );

        RE_TEST_CHECK_NOT_NULL( first );
        RE_TEST_CHECK_NOT_NULL( second );

        RE_Arena_Free( &arena, second, 64 );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( &arena ), afterFirst );

        /* The freed slot is handed straight back out. */
        void *again = RE_Arena_Alloc( &arena, 64, 16 );
        RE_TEST_CHECK( again == second );

        /* Freeing something that is not the newest allocation changes nothing. */
        ReUint64 usedBefore = RE_Arena_Used( &arena );
        RE_Arena_Free( &arena, first, 64 );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( &arena ), usedBefore );

        RE_Arena_Shutdown( &arena );
    }

    /* --- realloc: grow and shrink in place for the newest block, copy otherwise --- */
    {
        ReUint8 buffer[2048];
        ReArena arena;
        RE_TEST_CHECK( RE_Arena_InitFixed( &arena, buffer, sizeof( buffer ) ) );

        ReUint8 *growing = (ReUint8 *) RE_Arena_Alloc( &arena, 64, 16 );
        RE_TEST_CHECK_NOT_NULL( growing );
        RE_Memory_Set( growing, 0xAB, 64 );

        /* Newest allocation: grows without moving, which is the case a growing container hits
         * over and over.
         */
        ReUint8 *grown = (ReUint8 *) RE_Arena_Realloc( &arena, growing, 64, 128, 16 );
        RE_TEST_CHECK( grown == growing );
        RE_TEST_CHECK_EQ_UINT( grown[63], 0xAB );

        /* Shrinking in place hands the tail back. */
        RE_TEST_CHECK( RE_Arena_Realloc( &arena, grown, 128, 32, 16 ) == grown );
        RE_TEST_CHECK_EQ_UINT( RE_Arena_Used( &arena ), (ReUint64) ( grown - arena.base ) + 32 );

        /* With something newer in the way it has to move, and the contents must survive. */
        RE_TEST_CHECK_NOT_NULL( RE_Arena_Alloc( &arena, 16, 16 ) );

        ReUint8 *moved = (ReUint8 *) RE_Arena_Realloc( &arena, grown, 32, 64, 16 );
        RE_TEST_CHECK_NOT_NULL( moved );
        RE_TEST_CHECK( moved != grown );
        RE_TEST_CHECK_EQ_UINT( moved[31], 0xAB );

        /* realloc from null behaves as a plain allocation; to zero behaves as a free. */
        RE_TEST_CHECK_NOT_NULL( RE_Arena_Realloc( &arena, 0, 0, 32, 16 ) );
        RE_TEST_CHECK_NULL( RE_Arena_Realloc( &arena, moved, 64, 0, 16 ) );

        RE_Arena_Shutdown( &arena );
    }

    /* --- the uniform interface reaches the same arena --- */
    {
        ReUint8 buffer[1024];
        ReArena arena;
        RE_TEST_CHECK( RE_Arena_InitFixed( &arena, buffer, sizeof( buffer ) ) );

        ReAllocator allocator = RE_Arena_AsAllocator( &arena );

        /* An arena is single-threaded by contract, and must say so - a wrapper that believed
         * otherwise would skip the synchronisation it actually needs.
         */
        RE_TEST_CHECK( !allocator.isInternallyThreadSafe );

        void *p = RE_Memory_Alloc( &allocator, 64, 16 );
        RE_TEST_CHECK_NOT_NULL( p );
        RE_TEST_CHECK( RE_Arena_Used( &arena ) >= 64 );

        /* An arena has no size classes, so it hands back exactly what was asked for. */
        RE_TEST_CHECK_EQ_UINT( RE_Memory_Quantize( &allocator, 100, 16 ), 100 );

        RE_Memory_Free( &allocator, p, 64 );

        RE_Arena_Shutdown( &arena );
    }

    /* --- degenerate inputs --- */
    {
        ReArena arena;

        RE_TEST_CHECK( !RE_Arena_InitFixed( &arena, 0, 1024 ) );
        RE_TEST_CHECK( !RE_Arena_InitVirtual( &arena, 0 ) );

        ReUint8 buffer[64];
        RE_TEST_CHECK( RE_Arena_InitFixed( &arena, buffer, sizeof( buffer ) ) );
        RE_TEST_CHECK_NULL( RE_Arena_Alloc( &arena, 0, 16 ) );
        RE_Arena_Free( &arena, 0, 0 );
        RE_Arena_Shutdown( &arena );
    }
}
