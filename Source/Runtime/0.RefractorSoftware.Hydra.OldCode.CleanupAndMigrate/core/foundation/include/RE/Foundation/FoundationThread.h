/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationThread.h

    The minimum thread surface foundation needs. Boundary point: declared here, defined by the
    platform kernel, same arrangement as RE_VirtualMemory_* and RE_Log_WriteRaw.

    Deliberately tiny. This is not a threading library and is not the future job system - it is
    the two calls the memory system cannot be written without. Thread creation belongs to the job
    system, and designing its API before that exists would be guessing.
*/

/* Yields the rest of this thread's timeslice to another runnable thread on the same core.
 *
 * Used by the spin locks after they have spun long enough to conclude the holder is descheduled
 * rather than merely busy. Spinning through a preemption is how a lock that is nominally held for
 * 50 nanoseconds turns into a millisecond stall.
 */
void RE_Thread_Yield( void );

/* A stable, non-zero identity for the calling thread, unique among live threads.
 *
 * Only ever compared for equality and used as a hash input - do not assume it is small, dense, or
 * meaningful across process boundaries. Values are recycled once a thread exits, which is fine
 * for the ownership checks it exists for and would not be fine for anything persistent.
 */
ReUint64 RE_Thread_CurrentId( void );

typedef void ( *ReThreadExitFn )( void *userData );

/* Arranges for callback( userData ) to run on this thread as it exits.
 *
 * Exists so the memory system can tear down a thread's cache without every thread having to
 * remember to say so. A thread that allocates once and exits should not leak its cached bins, and
 * requiring an explicit shutdown call makes that a matter of discipline rather than of design.
 *
 * One registration per thread; a second replaces the first. Returns RE_False if the platform
 * could not arrange it, in which case cleanup falls back to the explicit call.
 *
 * @warning The callback runs during thread teardown, where the set of things that are safe to do
 *          is narrow. Touch only memory the callback owns.
 */
ReBool RE_Thread_RegisterExitCallback( ReThreadExitFn callback, void *userData );
