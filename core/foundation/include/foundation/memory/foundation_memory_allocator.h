/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include "foundation/primitive/foundation_primitive_predef.h"
#include "foundation/primitive/foundation_primitive_types.h"

/* TODO(will) Consider making this private (move to src/foundation/memory/foundation_memory_allocator_pimpl.h) so that users only see this interface.
 *            You can argue that this adds indirection but
 *            1) LTO will probably sort it out in a static build for shipping, and
 *            2) if you're calling any allocator frequently in hot loops, your code is shit and you should fix it (allocate ahead-of-time, for example)
 *            AFAIK this means we'd need to do something like pimpl in C (maybe a plain u8 pimpl[MEMORY_ALLOCATOR_SIZE] that we reinterpret in implementation)
 */
typedef struct memory_allocator memory_allocator;
struct memory_allocator
{
    void *_context;
    void *(*_alloc) (void *ctx, usize size);
    void  (*_free)  (void *ctx, void *ptr);
};

void *
memory_allocate (memory_allocator *a, usize size)
{
    return a ? a->_alloc (a->_context, size) : 0;
}

void
memory_free (memory_allocator *a, void *ptr)
{
    a ? a->_free (a->_context, ptr) : (void)(0);
}

#define memory_allocate_t(T, allocator)            StaticCast (T*) (memory_allocate (a, sizeof (T))
#define memory_allocate_array_t(T, num, allocator) StaticCast (T*) (memory_allocate (a, sizeof (T) * num))
