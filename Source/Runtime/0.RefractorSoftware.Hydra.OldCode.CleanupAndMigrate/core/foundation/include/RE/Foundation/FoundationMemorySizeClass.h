/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationCompiler.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationMemorySizeClass.h

    Every small request is rounded up to one of a fixed set of sizes. That is what makes the heap
    fast - a bin is a stack, so pop and push are O(1) - and what very nearly eliminates external
    fragmentation, since every hole in a block is exactly one bin and therefore always fits the
    next request of that class.

    What it costs is internal fragmentation: the gap between what was asked for and what was
    given. The shape of this table is the knob that trades one against the other.

    Dense classes waste less per allocation but multiply metadata and thread-cache state; sparse
    classes do the opposite. A pure powers-of-two table averages about 25% internal waste, which
    is far too much at engine scale, so the table below is dense at the bottom and thins out as
    sizes grow.

    Every class is then snapped so that it divides the block size nearly evenly. A class that fits
    5 per block and leaves 15% of it unusable is strictly worse than a slightly smaller one that
    fits 6 and leaves 1%. The table is picked to minimise tail waste, not to be mathematically
    tidy.
*/

/* The unit the heap maps pointers with: every span starts on a multiple of this and is a whole
 * number of them, so "which span does this pointer belong to" is a mask rather than a search.
 *
 * 64 KiB is a whole number of pages on every target we care about - 4 KiB and 16 KiB alike - so
 * choosing classes for divisibility against this subsumes page divisibility rather than having to
 * be recomputed when the page size changes. It also matches Windows' allocation granularity.
 */
#define RE_HEAP_BLOCK_SIZE ( 64 * 1024 )

/* A span is the run of memory a class is carved from: one or more blocks.
 *
 * Large classes need more than one block. Only four 13 KiB bins fit in 64 KiB, so the sizes that
 * divide a single block evenly get very sparse near the ceiling - the nearest neighbours either
 * side of 13 KiB are 13104 and 16384, a 25% jump that every allocation in between would pay for.
 * Giving those classes a two-block span puts nine bins in 128 KiB and closes the gap.
 */
#define RE_HEAP_MIN_BINS_PER_SPAN 8
#define RE_HEAP_MAX_SPAN_SIZE     ( 1024 * 1024 )

/* The smallest class has a hard floor: a free bin has to hold the free-run node that threads it
 * onto its block's free list. Asserted at init rather than assumed.
 */
#define RE_HEAP_MIN_BIN_SIZE 8

/* Requests above this go to the large path. Higher means fewer trips to the page layer and a
 * larger resident footprint; lower means the opposite. Worth revisiting per platform.
 */
#define RE_HEAP_MAX_SMALL_SIZE ( 16 * 1024 )

/* All sizes are a multiple of this, which is what makes the lookup table a shift rather than a
 * divide.
 */
#define RE_HEAP_SIZE_GRANULE       8
#define RE_HEAP_SIZE_GRANULE_SHIFT 3

/* Comfortably above what the generator produces; asserted at init. Kept at or below 256 so a
 * class index still fits in a byte, which the side-table strategy depends on.
 */
#define RE_HEAP_MAX_CLASSES 128

/* Returned by the lookup for anything the small path cannot serve. */
#define RE_HEAP_CLASS_LARGE 0xFFFFFFFFu

/* Builds the table. Safe to call more than once; later calls are no-ops.
 *
 * Queries the page size at runtime and asserts the block size is a multiple of it - never
 * trusting a compile-time constant for that, because 16 KiB pages are real.
 */
ReBool RE_HeapSizeClass_Init( void );

ReUint32 RE_HeapSizeClass_Count( void );

/* Byte size of a class. Valid for index == count, which returns 0, so that callers walking
 * backwards from a class do not need a bounds check.
 */
ReUint64 RE_HeapSizeClass_Size( ReUint32 classIndex );

/* Bytes in one span of this class - always a whole number of RE_HEAP_BLOCK_SIZE blocks. */
ReUint64 RE_HeapSizeClass_SpanSize( ReUint32 classIndex );

/* How many bins of this class fit in one span. */
ReUint32 RE_HeapSizeClass_BinsPerSpan( ReUint32 classIndex );

/* The class that serves a request, or RE_HEAP_CLASS_LARGE if it is too big.
 *
 * One load from a flat table - never a search. The table is a few kilobytes and stays cache
 * resident.
 */
ReUint32 RE_HeapSizeClass_Of( ReUint64 size );

/* The size a request of this many bytes would actually receive, so a growable container can size
 * itself to the boundary rather than wasting the remainder. Returns size unchanged for anything
 * on the large path, where there is no rounding to exploit.
 */
ReUint64 RE_HeapSizeClass_Quantize( ReUint64 size );

/* Size of the class one step below the given one, or 0 at the bottom.
 *
 * Exists for realloc's shrink test, which needs to know whether a smaller request would still
 * belong in the same class. Defined at index 0 so that test needs no special case.
 */
RE_ALWAYS_INLINE_HINT ReUint64
RE_HeapSizeClass_SizeBelow( ReUint32 classIndex )
{
    return ( classIndex == 0 ) ? 0 : RE_HeapSizeClass_Size( classIndex - 1 );
}
