/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationMemoryArena.h>
#include <RE/Foundation/FoundationMemoryScratch.h>

/*
    FoundationContext.h

    Ambient per-thread state: the allocators a function can reach for without being handed one.

    The convention this exists to enforce, applied consistently:

      - The frame and scratch allocators are genuinely ambient. Nearly any function in a frame
        could plausibly want scratch memory, and threading a parameter for it through every caller
        buys a kind of clarity nobody actually reads.

      - Everything else is passed explicitly. A subsystem allocating from a specific budgeted
        arena should say so in its signature, because there the identity of the allocator is real
        information rather than noise.

      - A function that returns allocated memory takes the destination allocator as a parameter.
        Never return memory from your own scratch; the caller cannot know its lifetime.

    Not to be confused with ReAppContext, which is the per-process handoff from the platform
    kernel into the engine. This is per thread, and the engine owns it.

    @warning Every thread that allocates must call RE_Context_ThreadInit. A thread that skips it
             gets a null frame allocator, which crashes at the point of use - deliberately, since
             the alternative failure mode is inheriting a stale one and corrupting silently.
*/

typedef struct ReContext
{
    /* Unpredictable lifetimes. Decorated according to build level. */
    ReAllocator allocator;

    /* This thread's slot in the frame allocator. Valid until the next RE_MemorySystem_BeginFrame
     * that rotates past it, which is why nothing may hold a frame pointer across frames.
     */
    ReUint32 threadIndex;

    ReBool initialized;
} ReContext;

/* Sets this thread up: its heap cache, its scratch arenas, and its slot in the frame allocator.
 *
 * threadIndex must be unique among live threads and below the frame allocator's thread count. The
 * job system will assign these from its worker pool; until then, the main thread is 0.
 */
ReBool RE_Context_ThreadInit( ReUint32 threadIndex );

/* Releases this thread's scratch arenas and heap cache. Called automatically at thread exit. */
void RE_Context_ThreadShutdown( void );

/* This thread's context. Never null once RE_Context_ThreadInit has run. */
ReContext *RE_Context_Get( void );

/* The general-purpose allocator for this thread. Shorthand for the common case. */
ReAllocator *RE_Context_Allocator( void );

/* This thread's arena for the current frame.
 *
 * @warning Everything in it is reclaimed a fixed number of frames from now. Do not store a
 *          pointer into it anywhere that outlives the frame; that is the single most common way
 *          frame allocators go wrong, and it fails silently because the memory stays readable.
 */
ReArena *RE_Context_FrameArena( void );

/* Frame-lifetime memory for this thread. Returns 0 if the frame budget is exhausted and no
 * overflow allocator is installed, which in development is deliberate.
 */
void *RE_Context_FrameAlloc( ReUint64 size, ReUint64 alignment );
