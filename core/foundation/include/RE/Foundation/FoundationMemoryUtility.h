/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <string.h>

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/* Matches typical malloc-class alignment guarantees (16 on 64-bit) - the default alignment used
 * when going through the generic ReAllocator interface, which has no alignment parameter
 * of its own.
 */
#define RE_MEMORY_DEFAULT_ALIGNMENT (2 * sizeof (void *))

internal inline void *
RE_Memory_Copy (void *dest, const void *src, ReUint64 size)
{
    return memcpy (dest, src, size);
}

internal inline void *
RE_Memory_Move (void *dest, const void *src, ReUint64 size)
{
    return memmove (dest, src, size);
}

internal inline void *
RE_Memory_Set (void *dest, ReUint8 value, ReUint64 size)
{
    return memset (dest, value, size);
}

internal inline void *
RE_Memory_Zero (void *dest, ReUint64 size)
{
    return memset (dest, 0, size);
}

/* Deliberately not a raw memcmp() passthrough - memcmp()'s return magnitude beyond its sign is
 * implementation-defined; this gives a well-defined result at the first differing byte.
 */
internal inline ReSint32
RE_Memory_Compare (const void *a, const void *b, ReUint64 size)
{
    const ReUint8 *ab = (const ReUint8 *) a;
    const ReUint8 *bb = (const ReUint8 *) b;

    for (ReUint64 i = 0; i < size; i += 1)
    {
        if (ab[i] != bb[i])
        {
            return (ReSint32) ab[i] - (ReSint32) bb[i];
        }
    }

    return 0;
}

/* Rounds value up to the next multiple of alignment. alignment must be a nonzero power of two -
 * shared by the arena/pool allocators, which both need to align an absolute address, not just an
 * offset (aligning the offset alone is wrong whenever the base address itself isn't aligned).
 */
internal inline ReUint64
RE_Memory_AlignUp (ReUint64 value, ReUint64 alignment)
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}
