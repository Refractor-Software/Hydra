/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    FoundationMemory.h

    Umbrella over the memory system. Include this to get the common surface; include the specific
    headers directly when only one piece is wanted.

    Choosing between these is mostly a question of what you already know about the lifetime:

      everything dies together          -> arena
      ...at the frame boundary          -> frame allocator
      ...in LIFO order within a scope   -> arena markers, or scratch
      everything is the same size       -> pool
      ...and needs safe outside refs    -> handle pool
      lifetime ends when a fence signals-> ring
      genuinely unpredictable           -> the general-purpose allocator
*/

#include <RE/Foundation/FoundationMemoryAllocator.h>
#include <RE/Foundation/FoundationMemoryArena.h>
#include <RE/Foundation/FoundationMemoryFrame.h>
#include <RE/Foundation/FoundationMemoryHandlePool.h>
#include <RE/Foundation/FoundationMemoryHeap.h>
#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemoryPool.h>
#include <RE/Foundation/FoundationMemoryRing.h>
#include <RE/Foundation/FoundationMemoryScratch.h>
#include <RE/Foundation/FoundationMemorySizeClass.h>
#include <RE/Foundation/FoundationMemoryThreadCache.h>
#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationVirtualMemory.h>
