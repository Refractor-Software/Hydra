/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include "foundation/primitive/foundation_primitive_predef.h"
#include "foundation/primitive/foundation_primitive_types.h"

/*
    foundation_memory_allocator

    The main front-end interface behind most memory allocation.
    We take inspiration from Odin's approach to memory allocation and management.
*/

/**
 * Generic memory allocator interface. It can be called with the memory_allocate and memory_free family of functions.
 */
typedef struct memory_allocator memory_allocator;
struct memory_allocator
{
    u64 internal[1];
};

void * memory_allocate       (memory_allocator * RESTRICT a, usize size, usize alignment);
void * memory_allocate_array (memory_allocator * RESTRICT a, usize size, usize alignment, usize count);
void   memory_free           (memory_allocator * RESTRICT a, void * RESTRICT block);

/* TODO(will) use a more portable alignof specified in primitive predef (check for one provided by the compiler if pre-C11) */
#define memory_allocate_type (T, allocator)              StaticCast (T*) (memory_allocate       (allocator, sizeof (T), alignof (T)))
#define memory_allocate_array_type (T, allocator, count) StaticCast (T*) (memory_allocate_array (allocator, sizeof (T), alignof (T), count))
