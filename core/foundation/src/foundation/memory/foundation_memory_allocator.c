/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "foundation/memory/foundation_memory_allocator.h"

#include "foundation/memory/foundation_memory_allocator_internal.h"

void *
memory_allocate (memory_allocator * RESTRICT a, usize size, usize alignment)
{
    return a ? a->_alloc (a->_context, size, alignment) : 0;
}

void
memory_free (memory_allocator * RESTRICT a, void * RESTRICT block)
{
    a ? a->_free (a->_context, block) : (void) (0);
}

void *
memory_allocate_array (memory_allocator * RESTRICT a, usize size, usize alignment, usize count)
{
    if (count != 0 && size > ((usize) -1) / count)
    {
        return 0;
    }

    return memory_allocate (a, size * count, alignment);
}
