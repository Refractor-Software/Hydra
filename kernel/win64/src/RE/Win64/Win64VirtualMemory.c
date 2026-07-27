/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64.h"

#include <RE/Foundation/FoundationVirtualMemory.h>

/*
    Win64VirtualMemory.c

    Windows side of the RE_VirtualMemory_* boundary. This is the only place in the project that
    knows VirtualAlloc, MEM_RESERVE, or the difference between a page and an allocation granule.
*/

global ReUint64 gWin64VirtualMemoryPageSize;
global ReUint64 gWin64VirtualMemoryAllocationGranularity;

/* Queried once, lazily, because reservations can happen before any explicit kernel init runs -
 * command-line parsing is the current example. GetSystemInfo cannot fail and is cheap enough that
 * a guarded call on each entry point costs nothing measurable.
 */
internal void
Win64_VirtualMemory_EnsureGranularities( void )
{
    if ( gWin64VirtualMemoryPageSize != 0 )
    {
        return;
    }

    SYSTEM_INFO systemInfo;
    GetSystemInfo( &systemInfo );

    gWin64VirtualMemoryPageSize = systemInfo.dwPageSize;
    gWin64VirtualMemoryAllocationGranularity = systemInfo.dwAllocationGranularity;
}

internal ReUint64
Win64_VirtualMemory_AlignUp( ReUint64 value, ReUint64 alignment )
{
    return ( value + ( alignment - 1 ) ) & ~( alignment - 1 );
}

ReUint64
RE_VirtualMemory_ReserveGranularity( void )
{
    Win64_VirtualMemory_EnsureGranularities();

    return gWin64VirtualMemoryAllocationGranularity;
}

ReUint64
RE_VirtualMemory_CommitGranularity( void )
{
    Win64_VirtualMemory_EnsureGranularities();

    return gWin64VirtualMemoryPageSize;
}

ReVirtualRegion
RE_VirtualMemory_Reserve( ReUint64 size, ReUint64 alignment )
{
    Win64_VirtualMemory_EnsureGranularities();

    ReVirtualRegion region = {0};

    if ( size == 0 )
    {
        return region;
    }

    ReUint64 granularity = gWin64VirtualMemoryAllocationGranularity;
    ReUint64 reserveSize = Win64_VirtualMemory_AlignUp( size, granularity );

    /* VirtualAlloc already returns granularity-aligned addresses, so anything at or below that
     * needs no special handling.
     */
    if ( alignment <= granularity )
    {
        void *base = VirtualAlloc( NULL, reserveSize, MEM_RESERVE, PAGE_READWRITE );
        if ( !base )
        {
            return region;
        }

        region.base = base;
        region.size = reserveSize;

        return region;
    }

    /* Over-reserve, find the aligned address inside it, then release and re-reserve exactly there.
     * Windows has no aligned reservation primitive and will not let a reservation be trimmed at
     * its edges, so the release/re-reserve dance is the documented approach.
     *
     * The window between the two calls is racy in principle - another thread could take the range.
     * Retry rather than fail, since losing the race twice in a row is vanishingly unlikely and a
     * spurious reservation failure would be far more disruptive than a second attempt.
     */
    for ( ReUint32 attempt = 0; attempt < 4; attempt += 1 )
    {
        void *probe = VirtualAlloc( NULL, reserveSize + alignment, MEM_RESERVE, PAGE_READWRITE );
        if ( !probe )
        {
            return region;
        }

        ReUint64 alignedBase = Win64_VirtualMemory_AlignUp( (ReUint64) probe, alignment );
        VirtualFree( probe, 0, MEM_RELEASE );

        void *base = VirtualAlloc( (void *) alignedBase, reserveSize, MEM_RESERVE, PAGE_READWRITE );
        if ( base )
        {
            region.base = base;
            region.size = reserveSize;

            return region;
        }
    }

    return region;
}

ReBool
RE_VirtualMemory_Commit( ReVirtualRegion *region, ReUint64 offset, ReUint64 size )
{
    Win64_VirtualMemory_EnsureGranularities();

    if ( !region || !region->base || size == 0 )
    {
        return RE_False;
    }

    ReUint64 pageSize = gWin64VirtualMemoryPageSize;

    /* Round the range outward, not the length inward - a commit of 1 byte at offset 4095 spans
     * two pages, and rounding the size alone would leave the second one uncommitted.
     */
    ReUint64 start = offset & ~( pageSize - 1 );
    ReUint64 end   = Win64_VirtualMemory_AlignUp( offset + size, pageSize );

    if ( end > region->size )
    {
        return RE_False;
    }

    void *result = VirtualAlloc( (ReUint8 *) region->base + start, end - start, MEM_COMMIT, PAGE_READWRITE );

    return (ReBool) ( result != NULL );
}

void
RE_VirtualMemory_Decommit( ReVirtualRegion *region, ReUint64 offset, ReUint64 size )
{
    Win64_VirtualMemory_EnsureGranularities();

    if ( !region || !region->base || size == 0 )
    {
        return;
    }

    ReUint64 pageSize = gWin64VirtualMemoryPageSize;

    /* Round inward here, the opposite of commit: a partially covered page at either end is still
     * partly live, and decommitting it would pull memory out from under data the caller kept.
     */
    ReUint64 start = Win64_VirtualMemory_AlignUp( offset, pageSize );
    ReUint64 end   = ( offset + size ) & ~( pageSize - 1 );

    if ( end <= start || end > region->size )
    {
        return;
    }

    VirtualFree( (ReUint8 *) region->base + start, end - start, MEM_DECOMMIT );
}

void
RE_VirtualMemory_Release( ReVirtualRegion *region )
{
    if ( !region || !region->base )
    {
        return;
    }

    VirtualFree( region->base, 0, MEM_RELEASE );

    region->base = 0;
    region->size = 0;
}
