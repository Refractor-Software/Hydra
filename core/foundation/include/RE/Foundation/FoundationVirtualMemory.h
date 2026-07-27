/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationVirtualMemory.h

    The engine's entire view of the OS memory manager. Everything above this - arenas, pools, the
    binned heap - is built on these five calls and knows nothing else about the platform.

    Boundary point: declared here, defined by the platform kernel (see Win64VirtualMemory.c), the
    same arrangement RE_Log_WriteRaw uses. Engine code calls these freely; the kernel never calls
    anything above them.

    The premise the whole memory system rests on: reserving address space is nearly free, and
    committing physical pages is not. Keeping the two separate is what lets an arena reserve a
    gigabyte and cost nothing until it is used.
*/

/* A reservation. base is page-aligned and stays fixed for the region's whole life - nothing here
 * ever moves memory, so pointers into a committed range stay valid until it is decommitted.
 */
typedef struct ReVirtualRegion
{
    void    *base;
    ReUint64 size;
} ReVirtualRegion;

/* Claims address space without physical backing. size is rounded up to the reserve granularity.
 * alignment must be a power of two, or 0 for the platform default; a larger alignment is served
 * by over-reserving and trimming, so ask for what you need and no more.
 *
 * Returns a region with base == 0 on failure. Reservation failure means the address space is
 * exhausted or fragmented, which on 64-bit is a real problem worth failing loudly on.
 */
ReVirtualRegion RE_VirtualMemory_Reserve( ReUint64 size, ReUint64 alignment );

/* Backs [offset, offset + size) of a reservation with physical pages. offset and size are rounded
 * out to the commit granularity, so committing a sub-page range commits the whole page.
 *
 * Committing an already-committed range is legal and is a no-op - callers that track a committed
 * watermark do not need to special-case the overlap.
 *
 * Returns RE_False if the OS refused, which is the ordinary out-of-memory signal. Callers are
 * expected to handle it; this is not an assert.
 */
ReBool RE_VirtualMemory_Commit( ReVirtualRegion *region, ReUint64 offset, ReUint64 size );

/* Releases physical pages while keeping the address space reserved. This is how memory is handed
 * back to the OS without surrendering the reservation - the range can be committed again later at
 * the same addresses.
 *
 * @warning Decommitting a range that is still being read is a use-after-free with a page fault
 *          instead of silent corruption. That is the better failure, but it is still a bug.
 */
void RE_VirtualMemory_Decommit( ReVirtualRegion *region, ReUint64 offset, ReUint64 size );

/* Returns the address space, decommitting anything still committed inside it. The region is
 * zeroed, so a double release is a no-op rather than a crash.
 */
void RE_VirtualMemory_Release( ReVirtualRegion *region );

/* The two granularities, both queried from the OS at startup and never assumed.
 *
 * These are different numbers and conflating them is a classic bug: reservations round to the
 * coarser one (64 KiB on Windows), commits to the finer one (the page size). Page size is not
 * universally 4 KiB either - 16 KiB is real on some targets, and a size-class table built for
 * 4 KiB wastes badly there.
 */
ReUint64 RE_VirtualMemory_ReserveGranularity( void );
ReUint64 RE_VirtualMemory_CommitGranularity( void );
