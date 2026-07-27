/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

void
RE_Test_MemoryMetadata( void )
{
    ReMemoryMetadataStats before = RE_MemoryMetadata_GetStats();

    /* --- basic allocation, alignment, and that the memory is genuinely writable --- */
    {
        void *a = RE_MemoryMetadata_Alloc( 64, 16 );
        void *b = RE_MemoryMetadata_Alloc( 64, 16 );

        RE_TEST_CHECK_NOT_NULL( a );
        RE_TEST_CHECK_NOT_NULL( b );
        RE_TEST_CHECK( a != b );

        RE_TEST_CHECK_EQ_UINT( (ReUint64) a & 15, 0 );
        RE_TEST_CHECK_EQ_UINT( (ReUint64) b & 15, 0 );

        RE_Memory_Set( a, 0xCD, 64 );
        RE_TEST_CHECK_EQ_UINT( ( (ReUint8 *) a )[63], 0xCD );
    }

    /* --- metadata hands back zeroed memory, which the bitmaps and descriptors rely on --- */
    {
        ReUint8 *zeroed = (ReUint8 *) RE_MemoryMetadata_Alloc( 256, 16 );
        RE_TEST_CHECK_NOT_NULL( zeroed );

        ReUint8 accumulated = 0;
        for ( ReUint64 i = 0; i < 256; i += 1 )
        {
            accumulated |= zeroed[i];
        }

        RE_TEST_CHECK_EQ_UINT( accumulated, 0 );
    }

    /* --- large alignments are honoured --- */
    {
        void *aligned = RE_MemoryMetadata_Alloc( 32, 4096 );

        RE_TEST_CHECK_NOT_NULL( aligned );
        RE_TEST_CHECK_EQ_UINT( (ReUint64) aligned & 4095, 0 );
    }

    /* --- an allocation larger than the default chunk forces a bigger chunk rather than failing --- */
    {
        void *huge = RE_MemoryMetadata_Alloc( 8 * 1024 * 1024, 16 );

        RE_TEST_CHECK_NOT_NULL( huge );

        /* Writing the last byte proves the whole span is really committed, not just the head. */
        ( (ReUint8 *) huge )[8 * 1024 * 1024 - 1] = 0x11;
    }

    /* --- many small allocations stay distinct and non-overlapping --- */
    {
        enum { AllocationCount = 512 };

        ReUint8 *pointers[AllocationCount];

        for ( ReUint64 i = 0; i < AllocationCount; i += 1 )
        {
            pointers[i] = (ReUint8 *) RE_MemoryMetadata_Alloc( 24, 8 );
            RE_TEST_CHECK_NOT_NULL( pointers[i] );

            if ( pointers[i] )
            {
                RE_Memory_Set( pointers[i], (ReUint8) ( i & 0xFF ), 24 );
            }
        }

        /* If any two overlapped, an earlier fill would have been clobbered by a later one. */
        for ( ReUint64 i = 0; i < AllocationCount; i += 1 )
        {
            if ( pointers[i] )
            {
                RE_TEST_CHECK_EQ_UINT( pointers[i][0], (ReUint8) ( i & 0xFF ) );
                RE_TEST_CHECK_EQ_UINT( pointers[i][23], (ReUint8) ( i & 0xFF ) );
            }
        }
    }

    /* --- degenerate input --- */
    RE_TEST_CHECK_NULL( RE_MemoryMetadata_Alloc( 0, 16 ) );

    /* --- the accounting actually moved, since uncounted metadata is the whole failure mode --- */
    {
        ReMemoryMetadataStats after = RE_MemoryMetadata_GetStats();

        RE_TEST_CHECK( after.bytesUsed > before.bytesUsed );
        RE_TEST_CHECK( after.bytesReserved >= after.bytesCommitted );
        RE_TEST_CHECK( after.bytesCommitted >= after.bytesUsed );
        RE_TEST_CHECK( after.chunkCount >= 1 );
    }

    /* Deliberately no RE_MemoryMetadata_Shutdown() here: later tests in the same process may hold
     * metadata pointers, and releasing underneath them would turn a test failure into a fault.
     * The process exiting reclaims it.
     */
}
