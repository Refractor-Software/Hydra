/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemorySizeClass.h>

#include <assert.h>

#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

/*
    The generator walks a schedule of step sizes - dense at the bottom, coarser as sizes grow -
    and snaps each candidate to a size that divides the block nearly evenly, then removes the
    duplicates that snapping inevitably produces.
*/

typedef struct ReSizeClassStep
{
    ReUint64 limit; /* generate candidates up to and including this size... */
    ReUint64 step;  /* ...advancing by this much */
} ReSizeClassStep;

RE_GLOBAL const ReSizeClassStep gSizeClassSchedule[] =
{
    {    16,    8 },
    {   256,   16 },
    {   512,   32 },
    {  1024,   64 },
    {  2048,  128 },
    {  4096,  256 },
    {  8192,  512 },
    { 16384, 1024 },
};

#define RE_SIZE_CLASS_STEP_COUNT ( sizeof( gSizeClassSchedule ) / sizeof( gSizeClassSchedule[0] ) )

/* size -> class, indexed by (size + granule - 1) >> granuleShift. */
#define RE_SIZE_CLASS_LOOKUP_ENTRIES ( ( RE_HEAP_MAX_SMALL_SIZE / RE_HEAP_SIZE_GRANULE ) + 1 )

RE_GLOBAL ReUint64 gSizeClassSizes[RE_HEAP_MAX_CLASSES];
RE_GLOBAL ReUint64 gSizeClassSpanSizes[RE_HEAP_MAX_CLASSES];
RE_GLOBAL ReUint32 gSizeClassBinsPerSpan[RE_HEAP_MAX_CLASSES];
RE_GLOBAL ReUint8  gSizeClassLookup[RE_SIZE_CLASS_LOOKUP_ENTRIES];
RE_GLOBAL ReUint32 gSizeClassCount;
RE_GLOBAL ReBool   gSizeClassInitialized;

/* The smallest span that holds a decent number of bins of this size.
 *
 * Bin count is what governs how finely the block-divisible sizes are spaced, so a class that
 * would only get a handful of bins from a single block is given more blocks until it does.
 */
RE_INTERNAL ReUint64
SizeClass_SpanSizeFor( ReUint64 candidate )
{
    ReUint64 spanSize = RE_HEAP_BLOCK_SIZE;

    while ( ( spanSize / candidate ) < RE_HEAP_MIN_BINS_PER_SPAN && spanSize < RE_HEAP_MAX_SPAN_SIZE )
    {
        spanSize *= 2;
    }

    return spanSize;
}

/* Rounds a candidate up to the largest size that still fits the same number of bins per span.
 *
 * The point is tail waste. A 700-byte class fits 93 bins in 64 KiB and leaves 436 bytes unusable;
 * snapping it to 704 fits the same 93 and leaves 64. Same bin count, less waste, and callers
 * asking for 700 now get 704 for free.
 */
RE_INTERNAL ReUint64
SizeClass_Snap( ReUint64 candidate, ReUint64 spanSize )
{
    ReUint64 binsPerSpan = spanSize / candidate;

    if ( binsPerSpan == 0 )
    {
        return 0;
    }

    ReUint64 snapped = ( spanSize / binsPerSpan ) & ~( (ReUint64) RE_HEAP_SIZE_GRANULE - 1 );

    /* Snapping down to the granule can occasionally drop below the candidate, which would make
     * the table non-monotonic. Keep the candidate in that case; the waste is bounded by one
     * granule per bin.
     */
    return ( snapped < candidate ) ? candidate : snapped;
}

ReBool
RE_HeapSizeClass_Init( void )
{
    if ( gSizeClassInitialized )
    {
        return RE_True;
    }

    /* The block has to be a whole number of pages, or committing one would either over-commit
     * into a neighbour or leave part of it unbacked. Queried, never assumed - 16 KiB pages exist.
     */
    ReUint64 pageSize = RE_VirtualMemory_CommitGranularity();

    assert( pageSize != 0 && "page size queried before the platform layer was ready" );
    assert( RE_HEAP_BLOCK_SIZE % pageSize == 0 && "block size must be a multiple of the page size" );

    if ( pageSize == 0 || ( RE_HEAP_BLOCK_SIZE % pageSize ) != 0 )
    {
        return RE_False;
    }

    ReUint32 count    = 0;
    ReUint64 previous = 0;
    ReUint64 candidate = RE_HEAP_MIN_BIN_SIZE;

    for ( ReUint64 stepIndex = 0; stepIndex < RE_SIZE_CLASS_STEP_COUNT; stepIndex += 1 )
    {
        ReUint64 limit = gSizeClassSchedule[stepIndex].limit;
        ReUint64 step  = gSizeClassSchedule[stepIndex].step;

        /* Realign to the new step on entering a tier. Carrying the previous tier's offset forward
         * would put every class in this tier off-boundary - 24, 40, 56 rather than 32, 48, 64 -
         * which both wastes more per allocation and leaves a wide gap right after the switch.
         */
        candidate = RE_Memory_AlignUp( candidate, step );

        while ( candidate <= limit )
        {
            ReUint64 spanSize = SizeClass_SpanSizeFor( candidate );
            ReUint64 snapped  = SizeClass_Snap( candidate, spanSize );

            /* Snapping pulls neighbouring candidates onto the same size, so duplicates are
             * expected rather than exceptional.
             */
            if ( snapped > previous && snapped <= RE_HEAP_MAX_SMALL_SIZE )
            {
                if ( count >= RE_HEAP_MAX_CLASSES )
                {
                    assert( 0 && "size class table overflowed RE_HEAP_MAX_CLASSES" );

                    return RE_False;
                }

                gSizeClassSizes[count]       = snapped;
                gSizeClassSpanSizes[count]   = spanSize;
                gSizeClassBinsPerSpan[count] = (ReUint32) ( spanSize / snapped );

                previous = snapped;
                count   += 1;
            }

            candidate += step;
        }
    }

    assert( count > 0 );
    assert( gSizeClassSizes[0] == RE_HEAP_MIN_BIN_SIZE );

    gSizeClassCount = count;

    /* Flat size -> class table. Built once so selection is a single load rather than a search.
     * Walked upward with a moving class index, so building it is linear rather than a binary
     * search per entry.
     */
    ReUint32 classIndex = 0;
    for ( ReUint64 entry = 0; entry < RE_SIZE_CLASS_LOOKUP_ENTRIES; entry += 1 )
    {
        ReUint64 size = entry * RE_HEAP_SIZE_GRANULE;

        while ( classIndex < count && gSizeClassSizes[classIndex] < size )
        {
            classIndex += 1;
        }

        gSizeClassLookup[entry] = (ReUint8) ( ( classIndex < count ) ? classIndex : ( count - 1 ) );
    }

    gSizeClassInitialized = RE_True;

    return RE_True;
}

ReUint32
RE_HeapSizeClass_Count( void )
{
    return gSizeClassCount;
}

ReUint64
RE_HeapSizeClass_Size( ReUint32 classIndex )
{
    if ( classIndex >= gSizeClassCount )
    {
        return 0;
    }

    return gSizeClassSizes[classIndex];
}

ReUint64
RE_HeapSizeClass_SpanSize( ReUint32 classIndex )
{
    if ( classIndex >= gSizeClassCount )
    {
        return 0;
    }

    return gSizeClassSpanSizes[classIndex];
}

ReUint32
RE_HeapSizeClass_BinsPerSpan( ReUint32 classIndex )
{
    if ( classIndex >= gSizeClassCount )
    {
        return 0;
    }

    return gSizeClassBinsPerSpan[classIndex];
}

ReUint32
RE_HeapSizeClass_Of( ReUint64 size )
{
    if ( size == 0 || size > RE_HEAP_MAX_SMALL_SIZE )
    {
        return RE_HEAP_CLASS_LARGE;
    }

    assert( gSizeClassInitialized && "size classes used before RE_HeapSizeClass_Init" );

    ReUint64 entry = ( size + RE_HEAP_SIZE_GRANULE - 1 ) >> RE_HEAP_SIZE_GRANULE_SHIFT;

    return gSizeClassLookup[entry];
}

ReUint64
RE_HeapSizeClass_Quantize( ReUint64 size )
{
    ReUint32 classIndex = RE_HeapSizeClass_Of( size );

    if ( classIndex == RE_HEAP_CLASS_LARGE )
    {
        return size;
    }

    return gSizeClassSizes[classIndex];
}
