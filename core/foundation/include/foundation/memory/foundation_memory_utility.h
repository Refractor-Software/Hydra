/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <string.h>

#include "foundation/primitive/foundation_primitive_types.h"

/* Matches typical malloc-class alignment guarantees (16 on 64-bit) - the default alignment used
 * when going through the generic memory_allocator interface, which has no alignment parameter
 * of its own.
 */
#define MEMORY_DEFAULT_ALIGNMENT (2 * sizeof (void *))

static inline void *
memory_copy (void *dest, const void *src, usize size)
{
    return memcpy (dest, src, size);
}

static inline void *
memory_move (void *dest, const void *src, usize size)
{
    return memmove (dest, src, size);
}

static inline void *
memory_set (void *dest, u8 value, usize size)
{
    return memset (dest, value, size);
}

static inline void *
memory_zero (void *dest, usize size)
{
    return memset (dest, 0, size);
}

/* Deliberately not a raw memcmp() passthrough - memcmp()'s return magnitude beyond its sign is
 * implementation-defined; this gives a well-defined result at the first differing byte.
 */
static inline s32
memory_compare (const void *a, const void *b, usize size)
{
    const u8 *ab = (const u8 *) a;
    const u8 *bb = (const u8 *) b;

    for (usize i = 0; i < size; i += 1)
    {
        if (ab[i] != bb[i])
        {
            return (s32) ab[i] - (s32) bb[i];
        }
    }

    return 0;
}

/* Rounds value up to the next multiple of alignment. alignment must be a nonzero power of two -
 * shared by the arena/pool allocators, which both need to align an absolute address, not just an
 * offset (aligning the offset alone is wrong whenever the base address itself isn't aligned).
 */
static inline usize
memory_align_up (usize value, usize alignment)
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}
