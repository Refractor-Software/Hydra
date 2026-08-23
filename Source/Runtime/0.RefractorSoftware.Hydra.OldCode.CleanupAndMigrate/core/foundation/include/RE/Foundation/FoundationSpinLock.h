/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationAtomic.h>
#include <RE/Foundation/FoundationThread.h>

/*
    FoundationSpinLock.h

    A spin lock with a yield fallback, for the very short critical sections inside the allocator.

    Why not a platform mutex: the sections this guards are a handful of instructions - pop a bin
    off a free list, flip a bitmap bit - and there is one of these per size class, so contention is
    already rare by construction. A kernel mutex would cost more in the uncontended case than the
    work it protects.

    Why it yields rather than spinning forever: a pure spin lock is fine until the holder is
    preempted, at which point every waiter burns its whole timeslice achieving nothing. Spinning
    briefly then yielding covers both cases without needing to know which one it is in.

    @threadsafe Yes, that is the point. Not recursive - taking one twice from the same thread
                deadlocks, and there is no ownership tracking to detect it.
*/

#define RE_SPIN_LOCK_SPINS_BEFORE_YIELD 64

typedef struct ReSpinLock
{
    ReAtomicUint32 locked;
} ReSpinLock;

RE_ALWAYS_INLINE_HINT void
RE_SpinLock_Init( ReSpinLock *lock )
{
    RE_Atomic_StoreUint32( &lock->locked, 0 );
}

/* Returns RE_True if the lock was taken. Never blocks.
 *
 * This is what lets a trimming thread walk other threads' caches without ever waiting on one: if
 * a cache is busy, skip it and let its owner flush at its own next epoch check.
 */
RE_ALWAYS_INLINE_HINT ReBool
RE_SpinLock_TryAcquire( ReSpinLock *lock )
{
    /* Read first and only attempt the exchange if it looks free. An unconditional exchange takes
     * the cache line exclusive on every attempt, which turns a read-mostly contended lock into a
     * cache-line ping-pong.
     */
    if ( RE_Atomic_LoadUint32( &lock->locked ) != 0 )
    {
        return RE_False;
    }

    return (ReBool) ( RE_Atomic_CompareExchangeUint32( &lock->locked, 0, 1 ) == 0 );
}

RE_ALWAYS_INLINE_HINT void
RE_SpinLock_Acquire( ReSpinLock *lock )
{
    for ( ;; )
    {
        if ( RE_SpinLock_TryAcquire( lock ) )
        {
            return;
        }

        for ( ReUint32 spin = 0; spin < RE_SPIN_LOCK_SPINS_BEFORE_YIELD; spin += 1 )
        {
            RE_Atomic_PauseHint();

            if ( RE_Atomic_LoadUint32( &lock->locked ) == 0 )
            {
                break;
            }
        }

        if ( RE_Atomic_LoadUint32( &lock->locked ) != 0 )
        {
            RE_Thread_Yield();
        }
    }
}

RE_ALWAYS_INLINE_HINT void
RE_SpinLock_Release( ReSpinLock *lock )
{
    RE_Atomic_StoreUint32( &lock->locked, 0 );
}
