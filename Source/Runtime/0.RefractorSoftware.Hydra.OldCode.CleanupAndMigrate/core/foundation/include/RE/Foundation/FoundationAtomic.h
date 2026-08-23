/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationCompiler.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationAtomic.h

    The atomic operations the memory system needs, and nothing more. Compiler-level rather than a
    platform boundary - these are intrinsics, not syscalls, so the kernel has no part in them.

    Deliberately not <stdatomic.h>: MSVC's C11 atomics support is recent and still gated behind
    /experimental:c11atomics, and we would rather own four intrinsics than depend on that.

    Memory ordering here is acquire/release, not sequentially consistent. That is what the depot
    and the trim epoch need and it is free on x64. Anything wanting stronger ordering should say
    so explicitly rather than assuming.
*/

#if defined( _MSC_VER )
#include <intrin.h>
#endif

typedef volatile ReUint32 ReAtomicUint32;
typedef volatile ReUint64 ReAtomicUint64;
typedef void *volatile    ReAtomicPtr;

#if defined( _MSC_VER ) && defined( _M_X64 )

/* x64 is total-store-ordered: aligned loads and stores are already atomic and already carry
 * acquire/release semantics in hardware. All that is needed is to stop the *compiler* reordering
 * across them, which is what _ReadWriteBarrier does.
 *
 * This is not true on ARM64. When a Switch 2 or other AArch64 target arrives, these need
 * __iso_volatile_load/store plus __dmb - hence the #error below rather than a silent fallthrough
 * that would appear to work and be subtly wrong.
 */

#define RE_ATOMIC_COMPILER_BARRIER() _ReadWriteBarrier()

RE_ALWAYS_INLINE_HINT ReUint32
RE_Atomic_LoadUint32( const ReAtomicUint32 *target )
{
    ReUint32 value = *target;
    RE_ATOMIC_COMPILER_BARRIER();

    return value;
}

RE_ALWAYS_INLINE_HINT void
RE_Atomic_StoreUint32( ReAtomicUint32 *target, ReUint32 value )
{
    RE_ATOMIC_COMPILER_BARRIER();
    *target = value;
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Atomic_LoadUint64( const ReAtomicUint64 *target )
{
    ReUint64 value = *target;
    RE_ATOMIC_COMPILER_BARRIER();

    return value;
}

RE_ALWAYS_INLINE_HINT void
RE_Atomic_StoreUint64( ReAtomicUint64 *target, ReUint64 value )
{
    RE_ATOMIC_COMPILER_BARRIER();
    *target = value;
}

RE_ALWAYS_INLINE_HINT void *
RE_Atomic_LoadPtr( const ReAtomicPtr *target )
{
    void *value = *target;
    RE_ATOMIC_COMPILER_BARRIER();

    return value;
}

RE_ALWAYS_INLINE_HINT void
RE_Atomic_StorePtr( ReAtomicPtr *target, void *value )
{
    RE_ATOMIC_COMPILER_BARRIER();
    *target = value;
}

RE_ALWAYS_INLINE_HINT ReUint32
RE_Atomic_FetchAddUint32( ReAtomicUint32 *target, ReUint32 addend )
{
    return (ReUint32) _InterlockedExchangeAdd( (volatile long *) target, (long) addend );
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Atomic_FetchAddUint64( ReAtomicUint64 *target, ReUint64 addend )
{
    return (ReUint64) _InterlockedExchangeAdd64( (volatile __int64 *) target, (__int64) addend );
}

RE_ALWAYS_INLINE_HINT ReUint32
RE_Atomic_ExchangeUint32( ReAtomicUint32 *target, ReUint32 value )
{
    return (ReUint32) _InterlockedExchange( (volatile long *) target, (long) value );
}

/* Returns the value that was actually there. The caller compares it against `expected` to learn
 * whether the exchange happened - returning the witnessed value rather than a bool means a failed
 * CAS in a retry loop already has the fresh value in hand.
 */
RE_ALWAYS_INLINE_HINT ReUint32
RE_Atomic_CompareExchangeUint32( ReAtomicUint32 *target, ReUint32 expected, ReUint32 desired )
{
    return (ReUint32) _InterlockedCompareExchange( (volatile long *) target, (long) desired, (long) expected );
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Atomic_CompareExchangeUint64( ReAtomicUint64 *target, ReUint64 expected, ReUint64 desired )
{
    return (ReUint64) _InterlockedCompareExchange64( (volatile __int64 *) target, (__int64) desired, (__int64) expected );
}

RE_ALWAYS_INLINE_HINT void *
RE_Atomic_CompareExchangePtr( ReAtomicPtr *target, void *expected, void *desired )
{
    return _InterlockedCompareExchangePointer( (void *volatile *) target, desired, expected );
}

RE_ALWAYS_INLINE_HINT void *
RE_Atomic_ExchangePtr( ReAtomicPtr *target, void *value )
{
    return _InterlockedExchangePointer( (void *volatile *) target, value );
}

/* Tells the CPU this is a spin-wait iteration: reduces power draw and, more importantly, avoids
 * the memory-order violation penalty when the lock is finally released.
 */
RE_ALWAYS_INLINE_HINT void
RE_Atomic_PauseHint( void )
{
    _mm_pause();
}

#elif defined( __GNUC__ ) || defined( __clang__ )

#define RE_ATOMIC_COMPILER_BARRIER() __atomic_signal_fence( __ATOMIC_SEQ_CST )

RE_ALWAYS_INLINE_HINT ReUint32
RE_Atomic_LoadUint32( const ReAtomicUint32 *target )
{
    return __atomic_load_n( target, __ATOMIC_ACQUIRE );
}

RE_ALWAYS_INLINE_HINT void
RE_Atomic_StoreUint32( ReAtomicUint32 *target, ReUint32 value )
{
    __atomic_store_n( target, value, __ATOMIC_RELEASE );
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Atomic_LoadUint64( const ReAtomicUint64 *target )
{
    return __atomic_load_n( target, __ATOMIC_ACQUIRE );
}

RE_ALWAYS_INLINE_HINT void
RE_Atomic_StoreUint64( ReAtomicUint64 *target, ReUint64 value )
{
    __atomic_store_n( target, value, __ATOMIC_RELEASE );
}

RE_ALWAYS_INLINE_HINT void *
RE_Atomic_LoadPtr( const ReAtomicPtr *target )
{
    return __atomic_load_n( target, __ATOMIC_ACQUIRE );
}

RE_ALWAYS_INLINE_HINT void
RE_Atomic_StorePtr( ReAtomicPtr *target, void *value )
{
    __atomic_store_n( target, value, __ATOMIC_RELEASE );
}

RE_ALWAYS_INLINE_HINT ReUint32
RE_Atomic_FetchAddUint32( ReAtomicUint32 *target, ReUint32 addend )
{
    return __atomic_fetch_add( target, addend, __ATOMIC_ACQ_REL );
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Atomic_FetchAddUint64( ReAtomicUint64 *target, ReUint64 addend )
{
    return __atomic_fetch_add( target, addend, __ATOMIC_ACQ_REL );
}

RE_ALWAYS_INLINE_HINT ReUint32
RE_Atomic_ExchangeUint32( ReAtomicUint32 *target, ReUint32 value )
{
    return __atomic_exchange_n( target, value, __ATOMIC_ACQ_REL );
}

RE_ALWAYS_INLINE_HINT ReUint32
RE_Atomic_CompareExchangeUint32( ReAtomicUint32 *target, ReUint32 expected, ReUint32 desired )
{
    __atomic_compare_exchange_n( target, &expected, desired, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE );

    return expected;
}

RE_ALWAYS_INLINE_HINT ReUint64
RE_Atomic_CompareExchangeUint64( ReAtomicUint64 *target, ReUint64 expected, ReUint64 desired )
{
    __atomic_compare_exchange_n( target, &expected, desired, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE );

    return expected;
}

RE_ALWAYS_INLINE_HINT void *
RE_Atomic_CompareExchangePtr( ReAtomicPtr *target, void *expected, void *desired )
{
    __atomic_compare_exchange_n( target, &expected, desired, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE );

    return expected;
}

RE_ALWAYS_INLINE_HINT void *
RE_Atomic_ExchangePtr( ReAtomicPtr *target, void *value )
{
    return __atomic_exchange_n( target, value, __ATOMIC_ACQ_REL );
}

RE_ALWAYS_INLINE_HINT void
RE_Atomic_PauseHint( void )
{
#if defined( __i386__ ) || defined( __x86_64__ )
    __builtin_ia32_pause();
#else
    __asm__ __volatile__( "yield" ::: "memory" );
#endif
}

#else
#error "FoundationAtomic.h: no atomic implementation for this compiler/architecture."
#endif

/* Cache line size, used to pad anything two threads touch independently. Getting this wrong does
 * not break correctness, only scaling - and invisibly, which is why §15 of the research suggests
 * asserting the alignment rather than trusting it.
 */
#define RE_CACHE_LINE_SIZE 64
