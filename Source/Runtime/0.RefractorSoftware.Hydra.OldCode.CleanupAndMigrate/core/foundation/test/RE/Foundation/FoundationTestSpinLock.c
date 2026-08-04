/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <RE/Foundation/FoundationAtomic.h>
#include <RE/Foundation/FoundationSpinLock.h>

/* Threads are spawned directly with the platform API here rather than through an engine
 * primitive. The job system does not exist yet, and inventing its worker-spawn API from the
 * needs of a test would be designing it backwards.
 */
#include "RE/Win64/Win64.h"

#define SPIN_LOCK_TEST_THREADS    8
#define SPIN_LOCK_TEST_ITERATIONS 20000

typedef struct ReSpinLockTestShared
{
    ReSpinLock lock;
    ReUint64   guardedCounter; /* only ever touched under the lock */
    ReAtomicUint64 atomicCounter;
} ReSpinLockTestShared;

RE_INTERNAL DWORD WINAPI
SpinLock_TestWorker( LPVOID parameter )
{
    ReSpinLockTestShared *shared = (ReSpinLockTestShared *) parameter;

    for ( ReUint32 i = 0; i < SPIN_LOCK_TEST_ITERATIONS; i += 1 )
    {
        RE_SpinLock_Acquire( &shared->lock );

        /* A deliberately non-atomic read-modify-write. If the lock does not actually exclude,
         * updates are lost and the final total comes out short - which is the entire point of
         * the check below.
         */
        ReUint64 value = shared->guardedCounter;
        shared->guardedCounter = value + 1;

        RE_SpinLock_Release( &shared->lock );

        RE_Atomic_FetchAddUint64( &shared->atomicCounter, 1 );
    }

    return 0;
}

void
RE_Test_SpinLock( void )
{
    /* --- uncontended acquire/release and try-acquire semantics --- */
    {
        ReSpinLock lock;
        RE_SpinLock_Init( &lock );

        RE_TEST_CHECK( RE_SpinLock_TryAcquire( &lock ) );

        /* Held: a second attempt must fail rather than block or succeed. This is what lets the
         * trimmer skip a busy thread cache instead of waiting on it.
         */
        RE_TEST_CHECK( !RE_SpinLock_TryAcquire( &lock ) );

        RE_SpinLock_Release( &lock );

        RE_TEST_CHECK( RE_SpinLock_TryAcquire( &lock ) );
        RE_SpinLock_Release( &lock );

        RE_SpinLock_Acquire( &lock );
        RE_TEST_CHECK( !RE_SpinLock_TryAcquire( &lock ) );
        RE_SpinLock_Release( &lock );
    }

    /* --- contended: the guarded counter must not lose a single update --- */
    {
        ReSpinLockTestShared shared;
        RE_SpinLock_Init( &shared.lock );
        shared.guardedCounter = 0;
        RE_Atomic_StoreUint64( &shared.atomicCounter, 0 );

        HANDLE threads[SPIN_LOCK_TEST_THREADS];
        ReUint32 started = 0;

        for ( ReUint32 i = 0; i < SPIN_LOCK_TEST_THREADS; i += 1 )
        {
            threads[i] = CreateThread( NULL, 0, SpinLock_TestWorker, &shared, 0, NULL );
            if ( threads[i] )
            {
                started += 1;
            }
        }

        RE_TEST_CHECK_EQ_UINT( started, SPIN_LOCK_TEST_THREADS );

        WaitForMultipleObjects( started, threads, TRUE, INFINITE );

        for ( ReUint32 i = 0; i < started; i += 1 )
        {
            CloseHandle( threads[i] );
        }

        ReUint64 expected = (ReUint64) started * SPIN_LOCK_TEST_ITERATIONS;

        RE_TEST_CHECK_EQ_UINT( shared.guardedCounter, expected );
        RE_TEST_CHECK_EQ_UINT( RE_Atomic_LoadUint64( &shared.atomicCounter ), expected );

        /* The lock must be left free once every worker has finished. */
        RE_TEST_CHECK( RE_SpinLock_TryAcquire( &shared.lock ) );
        RE_SpinLock_Release( &shared.lock );
    }

    /* --- compare-exchange reports the witnessed value, not a bool --- */
    {
        ReAtomicUint32 value;
        RE_Atomic_StoreUint32( &value, 7 );

        RE_TEST_CHECK_EQ_UINT( RE_Atomic_CompareExchangeUint32( &value, 7, 9 ), 7 );
        RE_TEST_CHECK_EQ_UINT( RE_Atomic_LoadUint32( &value ), 9 );

        /* A failed exchange leaves the value alone and hands back what was really there, so a
         * retry loop already has the fresh value.
         */
        RE_TEST_CHECK_EQ_UINT( RE_Atomic_CompareExchangeUint32( &value, 7, 11 ), 9 );
        RE_TEST_CHECK_EQ_UINT( RE_Atomic_LoadUint32( &value ), 9 );
    }

    /* --- pointer-width atomics, which the depot depends on --- */
    {
        ReUint32    storageA = 1;
        ReUint32    storageB = 2;
        ReAtomicPtr slot     = 0;

        RE_Atomic_StorePtr( &slot, &storageA );
        RE_TEST_CHECK( RE_Atomic_LoadPtr( &slot ) == &storageA );

        RE_TEST_CHECK( RE_Atomic_CompareExchangePtr( &slot, &storageA, &storageB ) == &storageA );
        RE_TEST_CHECK( RE_Atomic_LoadPtr( &slot ) == &storageB );

        RE_TEST_CHECK( RE_Atomic_ExchangePtr( &slot, 0 ) == &storageB );
        RE_TEST_CHECK_NULL( RE_Atomic_LoadPtr( &slot ) );
    }
}
