/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitivePredef.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationMemoryAllocator

    The main front-end interface behind most memory allocation.
    We take inspiration from Odin's approach to memory allocation and management.
*/

/**
 * Generic memory allocator interface. It can be called with the RE_Memory_Allocate and RE_Memory_Free family of functions.
 */
typedef struct ReAllocator ReAllocator;
struct ReAllocator
{
    /* Named `opaque` rather than `internal`, which is now a storage-class spelling (see
     * FoundationCompiler.h) and would be macro-expanded here.
     */
    ReUint64 opaque[1];
};

void * RE_Memory_Allocate       (ReAllocator * RE_RESTRICT a, ReUint64 size, ReUint64 alignment);
void * RE_Memory_AllocateArray (ReAllocator * RE_RESTRICT a, ReUint64 size, ReUint64 alignment, ReUint64 count);
void   RE_Memory_Free           (ReAllocator * RE_RESTRICT a, void * RE_RESTRICT block);

/* TODO(will) use a more portable alignof specified in primitive predef (check for one provided by the compiler if pre-C11) */
#define RE_MEMORY_ALLOCATE_TYPE (T, allocator)              RE_STATIC_CAST (T*) (RE_Memory_Allocate       (allocator, sizeof (T), alignof (T)))
#define RE_MEMORY_ALLOCATE_ARRAY_TYPE (T, allocator, count) RE_STATIC_CAST (T*) (RE_Memory_AllocateArray (allocator, sizeof (T), alignof (T), count))
