/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#ifndef foundation_memory_allocator_internal_h
#define foundation_memory_allocator_internal_h

#include <RE/Foundation/FoundationPrimitiveTypes.h>

struct memory_allocator
{
    void *_context;
    void *(*_alloc) (void *ctx, usize size, usize align);
    void  (*_free)  (void *ctx, void *ptr);
};

#endif /* foundation_memory_allocator_internal_h */
