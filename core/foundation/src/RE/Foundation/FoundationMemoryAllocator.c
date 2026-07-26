/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryAllocator.h>

#include "RE/Foundation/FoundationMemoryAllocatorInternal.h"

void *
RE_Memory_Allocate (ReAllocator * RE_RESTRICT a, ReUint64 size, ReUint64 alignment)
{
    return a ? a->_alloc (a->_context, size, alignment) : 0;
}

void
RE_Memory_Free (ReAllocator * RE_RESTRICT a, void * RE_RESTRICT block)
{
    a ? a->_free (a->_context, block) : (void) (0);
}

void *
RE_Memory_AllocateArray (ReAllocator * RE_RESTRICT a, ReUint64 size, ReUint64 alignment, ReUint64 count)
{
    if (count != 0 && size > ((ReUint64) -1) / count)
    {
        return 0;
    }

    return RE_Memory_Allocate (a, size * count, alignment);
}
