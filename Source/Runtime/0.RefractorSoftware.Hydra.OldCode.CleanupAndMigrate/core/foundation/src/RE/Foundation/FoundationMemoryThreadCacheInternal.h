/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationMemoryThreadCacheInternal.h

    The two calls the heap uses to route its small path through the per-thread caches. Private to
    the memory system.
*/

typedef struct ReThreadCache ReThreadCache;

/* A bin of this class from the calling thread's cache, or 0 if the cache could not serve it and
 * the caller should fall back to the locked heap path.
 */
void *RE_ThreadCache_Alloc( ReUint32 classIndex );

/* Parks a bin in the calling thread's cache. Returns RE_False if there is no usable cache, in
 * which case the caller still owns the block and must return it to the heap directly.
 */
ReBool RE_ThreadCache_Free( ReUint32 classIndex, void *block );
