/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationMemorySizeClass.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationMemoryHeapInternal.h

    Private to the heap implementation. Not in the public include tree, because none of this is
    anyone else's business.

    The one decision that defines this allocator's character is how free() gets from a bare
    pointer to the size class it belongs to, without a cache miss. Two answers are implemented
    here and chosen at build time:

      SIDE_TABLE     - a two-level radix table from span address to descriptor. Two dependent
                       loads on the free path, no large reservation, no per-class ceiling, and
                       the table doubles as a validity check.

      ADDRESS_SLICE  - each class owns a fixed slice of one enormous reservation, so the class is
                       encoded in the address itself. A subtract and a shift, no lookup at all,
                       but it costs a large reservation and imposes a hard per-class cap.

    Both satisfy the same interface below. Note that the interface covers span *acquisition*, not
    just lookup: the slice strategy requires a class's spans to come from that class's own address
    range, because the address is what carries the class. A boundary drawn only around the lookup
    would not actually be substitutable.

    Selected at compile time, not run time. A runtime switch would put an indirect branch on the
    single hottest path in the allocator, which is exactly the cost the slice strategy exists to
    avoid paying.
*/

#define RE_HEAP_MAP_STRATEGY_SIDE_TABLE    0
#define RE_HEAP_MAP_STRATEGY_ADDRESS_SLICE 1

#if !defined( RE_HEAP_MAP_STRATEGY )
#define RE_HEAP_MAP_STRATEGY RE_HEAP_MAP_STRATEGY_SIDE_TABLE
#endif

/* Address space each class gets under the slice strategy. Also its hard ceiling: a class that
 * fills its slice cannot borrow from another, and falls back to promoting or to the large path.
 */
#define RE_HEAP_SLICE_SIZE ( 256ull * 1024ull * 1024ull )

/* Marks "no bin" in a free-run link. Bin indices are small, so the top value is free for this. */
#define RE_HEAP_BIN_NONE 0xFFFFFFFFu

#define RE_HEAP_SPAN_CANARY 0x5AFEB10Cu

/*
    Free bins are threaded together inside the bins themselves - a free bin is by definition
    memory nobody is using, so the link costs nothing.

    runLength is what keeps carving a fresh span O(1): a newly committed span is one node saying
    "all N bins here are free" rather than N separate nodes.
*/
typedef struct ReHeapFreeRun
{
    ReUint32 nextRun;   /* bin index of the next free run, or RE_HEAP_BIN_NONE */
    ReUint32 runLength; /* bins free from this one onward, contiguously */
} ReHeapFreeRun;

/* The smallest class has to be able to hold one of these. */
typedef char ReHeapFreeRunFitsSmallestBin[( sizeof( ReHeapFreeRun ) <= RE_HEAP_MIN_BIN_SIZE ) ? 1 : -1];

/*
    Per-span bookkeeping. Kept beside the span rather than inside it so that the span's first bin
    is usable and so that a free() never has to touch the span's own pages to find its class.

    Deliberately a plain struct rather than the four bit-packed bytes the research suggests.
    Packing pays off when there are millions of descriptors; at 64 KiB per span a million of them
    is 64 GiB of heap, and until that is a real prospect the clarity is worth more than the bytes.
*/
typedef struct ReHeapSpan
{
    struct ReHeapSpan *next; /* partial-span list for this class */
    struct ReHeapSpan *prev;

    ReUint8 *base;

    ReUint32 freeRun;   /* bin index of the first free run, or RE_HEAP_BIN_NONE */
    ReUint32 binsInUse;
    ReUint32 binCount;
    ReUint32 classIndex;

    ReUint32 canary;
} ReHeapSpan;

/* Brings up whichever strategy was selected. Idempotent. */
ReBool RE_HeapMap_Init( void );
void   RE_HeapMap_Shutdown( void );

/* A span for this class, committed and ready to be carved, with base/binCount/classIndex filled
 * in and the free list initialised to one full run. Returns 0 if memory could not be obtained.
 */
ReHeapSpan *RE_HeapMap_AcquireSpan( ReUint32 classIndex );

/* Returns the span's memory. The descriptor may be recycled. */
void RE_HeapMap_ReleaseSpan( ReHeapSpan *span );

/* The span a pointer belongs to, or 0 if it is not inside any small-object span - which means it
 * is either a large allocation or not ours at all.
 *
 * This is the hot path. Everything above is arranged to make it cheap.
 */
ReHeapSpan *RE_HeapMap_SpanOf( const void *ptr );

/* Bytes of address space currently reserved by the map, for reporting. */
ReUint64 RE_HeapMap_ReservedBytes( void );
