/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

RE_INTERNAL ReBool
VirtualMemory_IsPowerOfTwo( ReUint64 value )
{
    return (ReBool) ( value != 0 && ( value & ( value - 1 ) ) == 0 );
}

void
RE_Test_VirtualMemory( void )
{
    ReUint64 reserveGranularity = RE_VirtualMemory_ReserveGranularity();
    ReUint64 commitGranularity  = RE_VirtualMemory_CommitGranularity();

    RE_TEST_CHECK( VirtualMemory_IsPowerOfTwo( reserveGranularity ) );
    RE_TEST_CHECK( VirtualMemory_IsPowerOfTwo( commitGranularity ) );

    /* Reservation is the coarser of the two. Conflating them is the classic bug this ordering
     * exists to make obvious.
     */
    RE_TEST_CHECK( reserveGranularity >= commitGranularity );

    /* --- reserve costs no physical memory, so an extravagant reservation must succeed --- */
    {
        ReVirtualRegion region = RE_VirtualMemory_Reserve( 1024ull * 1024ull * 1024ull, 0 );
        RE_TEST_CHECK_NOT_NULL( region.base );
        RE_TEST_CHECK( region.size >= 1024ull * 1024ull * 1024ull );

        RE_VirtualMemory_Release( &region );
        RE_TEST_CHECK_NULL( region.base );
        RE_TEST_CHECK_EQ_UINT( region.size, 0 );

        /* Release is idempotent - a second one must not fault. */
        RE_VirtualMemory_Release( &region );
    }

    /* --- commit then write, to prove the pages are really backed --- */
    {
        ReVirtualRegion region = RE_VirtualMemory_Reserve( 1024ull * 1024ull, 0 );
        RE_TEST_CHECK_NOT_NULL( region.base );

        RE_TEST_CHECK( RE_VirtualMemory_Commit( &region, 0, 4096 ) );

        ReUint8 *bytes = (ReUint8 *) region.base;
        for ( ReUint64 i = 0; i < 4096; i += 1 )
        {
            bytes[i] = (ReUint8) ( i & 0xFF );
        }

        for ( ReUint64 i = 0; i < 4096; i += 1 )
        {
            if ( bytes[i] != (ReUint8) ( i & 0xFF ) )
            {
                RE_TEST_CHECK( bytes[i] == (ReUint8) ( i & 0xFF ) );
                break;
            }
        }

        /* Re-committing an already-committed range is defined as a no-op, so that callers
         * tracking a watermark need not special-case the overlap.
         */
        RE_TEST_CHECK( RE_VirtualMemory_Commit( &region, 0, 4096 ) );

        /* A commit that spans a page boundary must back *both* pages. Asking for one byte at the
         * last byte of a page is the case that catches rounding the length instead of the range.
         */
        RE_TEST_CHECK( RE_VirtualMemory_Commit( &region, commitGranularity - 1, 2 ) );
        bytes[commitGranularity]     = 0x5A;
        bytes[commitGranularity + 1] = 0xA5;
        RE_TEST_CHECK_EQ_UINT( bytes[commitGranularity], 0x5A );

        /* Committing past the end of the reservation must fail rather than silently succeed. */
        RE_TEST_CHECK( !RE_VirtualMemory_Commit( &region, region.size, 4096 ) );

        RE_VirtualMemory_Release( &region );
    }

    /* --- decommit keeps the address space, so the range can be committed again --- */
    {
        ReVirtualRegion region = RE_VirtualMemory_Reserve( 1024ull * 1024ull, 0 );
        RE_TEST_CHECK_NOT_NULL( region.base );

        void *originalBase = region.base;

        RE_TEST_CHECK( RE_VirtualMemory_Commit( &region, 0, 64 * 1024 ) );

        ReUint8 *bytes = (ReUint8 *) region.base;
        bytes[0] = 0x7E;

        RE_VirtualMemory_Decommit( &region, 0, 64 * 1024 );

        RE_TEST_CHECK( region.base == originalBase );
        RE_TEST_CHECK( RE_VirtualMemory_Commit( &region, 0, 64 * 1024 ) );

        /* Recommitted pages are zero-filled by the OS - relying on the old contents surviving a
         * decommit is a bug this pins down.
         */
        RE_TEST_CHECK_EQ_UINT( bytes[0], 0 );

        RE_VirtualMemory_Release( &region );
    }

    /* --- over-aligned reservation --- */
    {
        ReUint64        alignment = 2ull * 1024ull * 1024ull;
        ReVirtualRegion region    = RE_VirtualMemory_Reserve( 4ull * 1024ull * 1024ull, alignment );

        RE_TEST_CHECK_NOT_NULL( region.base );
        RE_TEST_CHECK_EQ_UINT( (ReUint64) region.base & ( alignment - 1 ), 0 );

        RE_VirtualMemory_Release( &region );
    }

    /* --- degenerate inputs return cleanly rather than faulting --- */
    {
        ReVirtualRegion empty = RE_VirtualMemory_Reserve( 0, 0 );
        RE_TEST_CHECK_NULL( empty.base );

        RE_TEST_CHECK( !RE_VirtualMemory_Commit( 0, 0, 4096 ) );
        RE_VirtualMemory_Decommit( 0, 0, 4096 );
        RE_VirtualMemory_Release( 0 );
    }
}
