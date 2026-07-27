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
