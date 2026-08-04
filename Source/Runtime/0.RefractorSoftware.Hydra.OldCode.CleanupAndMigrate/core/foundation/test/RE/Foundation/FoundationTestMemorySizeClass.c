/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <RE/Foundation/FoundationMemorySizeClass.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

void
RE_Test_MemorySizeClass( void )
{
    RE_TEST_CHECK( RE_HeapSizeClass_Init() );

    /* Idempotent, because several subsystems may reasonably want to make sure it is ready. */
    RE_TEST_CHECK( RE_HeapSizeClass_Init() );

    ReUint32 count = RE_HeapSizeClass_Count();

    RE_TEST_CHECK( count > 0 );
    RE_TEST_CHECK( count <= RE_HEAP_MAX_CLASSES );

    /* A class index has to fit in a byte for the side-table strategy to pack one per block. */
    RE_TEST_CHECK( count <= 256 );

    /* --- the block must be a whole number of pages, whatever the page size turns out to be --- */
    {
        ReUint64 pageSize = RE_VirtualMemory_CommitGranularity();

        RE_TEST_CHECK( pageSize > 0 );
        RE_TEST_CHECK_EQ_UINT( RE_HEAP_BLOCK_SIZE % pageSize, 0 );
    }

    /* --- the table is strictly increasing and starts at the free-run node floor --- */
    {
        RE_TEST_CHECK_EQ_UINT( RE_HeapSizeClass_Size( 0 ), RE_HEAP_MIN_BIN_SIZE );

        for ( ReUint32 i = 1; i < count; i += 1 )
        {
            RE_TEST_CHECK( RE_HeapSizeClass_Size( i ) > RE_HeapSizeClass_Size( i - 1 ) );
        }

        RE_TEST_CHECK( RE_HeapSizeClass_Size( count - 1 ) <= RE_HEAP_MAX_SMALL_SIZE );

        /* Reading one past the end is defined, so realloc's shrink test needs no bounds check. */
        RE_TEST_CHECK_EQ_UINT( RE_HeapSizeClass_Size( count ), 0 );
        RE_TEST_CHECK_EQ_UINT( RE_HeapSizeClass_SizeBelow( 0 ), 0 );
    }

    /* --- spans are whole blocks, and every class gets a workable number of bins --- */
    {
        for ( ReUint32 i = 0; i < count; i += 1 )
        {
            ReUint64 size     = RE_HeapSizeClass_Size( i );
            ReUint64 spanSize = RE_HeapSizeClass_SpanSize( i );
            ReUint32 bins     = RE_HeapSizeClass_BinsPerSpan( i );

            RE_TEST_CHECK_EQ_UINT( size % RE_HEAP_SIZE_GRANULE, 0 );
            RE_TEST_CHECK_EQ_UINT( spanSize % RE_HEAP_BLOCK_SIZE, 0 );
            RE_TEST_CHECK( spanSize <= RE_HEAP_MAX_SPAN_SIZE );

            RE_TEST_CHECK( bins >= 1 );
            RE_TEST_CHECK_EQ_UINT( bins, spanSize / size );
            RE_TEST_CHECK( bins * size <= spanSize );

            /* A span is only widened when a single block would not hold enough bins, so anything
             * bigger than one block has to have been widened for a reason.
             */
            if ( spanSize > RE_HEAP_BLOCK_SIZE )
            {
                RE_TEST_CHECK( ( RE_HEAP_BLOCK_SIZE / size ) < RE_HEAP_MIN_BINS_PER_SPAN );
            }
        }
    }

    /* --- tail waste is the whole reason the table is shaped this way --- */
    {
        ReUint64 worstWastePermille = 0;
        ReUint32 worstClass         = 0;

        for ( ReUint32 i = 0; i < count; i += 1 )
        {
            ReUint64 size     = RE_HeapSizeClass_Size( i );
            ReUint64 spanSize = RE_HeapSizeClass_SpanSize( i );
            ReUint64 bins     = RE_HeapSizeClass_BinsPerSpan( i );
            ReUint64 waste    = spanSize - ( bins * size );
            ReUint64 permille = ( waste * 1000 ) / spanSize;

            if ( permille > worstWastePermille )
            {
                worstWastePermille = permille;
                worstClass         = i;
            }
        }

        /* Under 2% of a span. This is the number that says the snapping step is doing its job -
         * an unsnapped table leaves far more sitting unusable at the end of every span.
         */
        if ( worstWastePermille >= 20 )
        {
            RE_Test_ReportFailure( __FILE__, __LINE__, "class %u (%llu bytes) wastes %llu permille of its span",
                worstClass, RE_HeapSizeClass_Size( worstClass ), worstWastePermille );
        }
    }

    /* --- class density, which is what bounds internal waste --- */
    {
        /* Checked as the gap between neighbouring classes rather than as worst-case waste for a
         * single request. Worst case is misleading at the bottom of the table: asking for 17
         * bytes and getting 32 is 88% waste in *any* sane table, including a hand-tuned one,
         * because the alternative is a class every 8 bytes all the way up.
         *
         * What actually matters is that a request never rounds up by more than a quarter once
         * sizes are big enough for a quarter to mean anything, and never by more than one
         * granule step below that.
         */
        for ( ReUint32 i = 1; i < count; i += 1 )
        {
            ReUint64 previous = RE_HeapSizeClass_Size( i - 1 );
            ReUint64 current  = RE_HeapSizeClass_Size( i );

            ReBool smallAbsoluteStep = (ReBool) ( current - previous <= 16 );
            ReBool withinAQuarter    = (ReBool) ( current * 4 <= previous * 5 );

            if ( !smallAbsoluteStep && !withinAQuarter )
            {
                RE_Test_ReportFailure( __FILE__, __LINE__,
                    "gap from class %u (%llu) to %u (%llu) is too wide", i - 1, previous, i, current );
                break;
            }
        }
    }

    /* --- lookup never under-delivers, which is the one thing that must never happen --- */
    {
        for ( ReUint64 size = 1; size <= RE_HEAP_MAX_SMALL_SIZE; size += 1 )
        {
            ReUint32 classIndex = RE_HeapSizeClass_Of( size );

            RE_TEST_CHECK( classIndex != RE_HEAP_CLASS_LARGE );

            if ( classIndex == RE_HEAP_CLASS_LARGE )
            {
                break;
            }

            ReUint64 given = RE_HeapSizeClass_Size( classIndex );

            if ( given < size )
            {
                RE_Test_ReportFailure( __FILE__, __LINE__,
                    "size %llu mapped to class %u of only %llu bytes", size, classIndex, given );
                break;
            }

            /* ...and it is the *smallest* class that fits, or the table has a gap that is
             * quietly costing memory on every allocation of this size.
             */
            if ( classIndex > 0 && RE_HeapSizeClass_Size( classIndex - 1 ) >= size )
            {
                RE_Test_ReportFailure( __FILE__, __LINE__,
                    "size %llu mapped to class %u but class %u would have fit", size, classIndex,
                    classIndex - 1 );
                break;
            }
        }
    }

    /* --- quantize agrees with the lookup, and is idempotent --- */
    {
        for ( ReUint64 size = 1; size <= RE_HEAP_MAX_SMALL_SIZE; size += 7 )
        {
            ReUint64 given = RE_HeapSizeClass_Quantize( size );

            RE_TEST_CHECK( given >= size );
            RE_TEST_CHECK_EQ_UINT( RE_HeapSizeClass_Quantize( given ), given );
        }
    }

    /* --- the large boundary --- */
    {
        RE_TEST_CHECK( RE_HeapSizeClass_Of( RE_HEAP_MAX_SMALL_SIZE ) != RE_HEAP_CLASS_LARGE );
        RE_TEST_CHECK_EQ_UINT( RE_HeapSizeClass_Of( RE_HEAP_MAX_SMALL_SIZE + 1 ), RE_HEAP_CLASS_LARGE );
        RE_TEST_CHECK_EQ_UINT( RE_HeapSizeClass_Of( 0 ), RE_HEAP_CLASS_LARGE );

        /* Nothing to reclaim by rounding on the large path, so quantize leaves it alone. */
        RE_TEST_CHECK_EQ_UINT( RE_HeapSizeClass_Quantize( 1024 * 1024 ), 1024 * 1024 );
    }
}
