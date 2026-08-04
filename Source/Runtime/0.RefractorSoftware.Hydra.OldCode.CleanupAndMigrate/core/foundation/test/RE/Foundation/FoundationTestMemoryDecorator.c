/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <string.h>

#include <RE/Foundation/FoundationMemoryDecorator.h>
#include <RE/Foundation/FoundationMemoryHeap.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

#include "RE/Win64/Win64.h"

/* Captures whatever the decorators report, so the tests can assert on it instead of on a side
 * effect nobody can see.
 */
#define DECORATOR_TEST_LOG_SIZE 16384

RE_GLOBAL char     gDecoratorTestLog[DECORATOR_TEST_LOG_SIZE];
RE_GLOBAL ReUint64 gDecoratorTestLogUsed;
RE_GLOBAL ReUint32 gDecoratorTestMessages;

RE_INTERNAL void
Decorator_TestReport( const char *message )
{
    gDecoratorTestMessages += 1;

    ReUint64 length = strlen( message );

    if ( gDecoratorTestLogUsed + length + 2 < DECORATOR_TEST_LOG_SIZE )
    {
        memcpy( gDecoratorTestLog + gDecoratorTestLogUsed, message, length );
        gDecoratorTestLogUsed += length;

        gDecoratorTestLog[gDecoratorTestLogUsed] = '\n';
        gDecoratorTestLogUsed += 1;
        gDecoratorTestLog[gDecoratorTestLogUsed] = 0;
    }
}

RE_INTERNAL void
Decorator_TestResetLog( void )
{
    gDecoratorTestLog[0]   = 0;
    gDecoratorTestLogUsed  = 0;
    gDecoratorTestMessages = 0;
}

RE_INTERNAL ReBool
Decorator_TestLogContains( const char *needle )
{
    return (ReBool) ( strstr( gDecoratorTestLog, needle ) != 0 );
}

/* An allocator that always fails, for exercising the out-of-memory path without actually
 * exhausting the machine.
 */
RE_INTERNAL void *
Decorator_TestFailingAlloc( void *context, ReUint64 size, ReUint64 alignment )
{
    (void) context;
    (void) size;
    (void) alignment;

    return 0;
}

RE_INTERNAL void *
Decorator_TestFailingRealloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize,
    ReUint64 alignment )
{
    (void) context;
    (void) block;
    (void) oldSize;
    (void) newSize;
    (void) alignment;

    return 0;
}

RE_INTERNAL void
Decorator_TestFailingFree( void *context, void *block, ReUint64 oldSize )
{
    (void) context;
    (void) block;
    (void) oldSize;
}

RE_INTERNAL ReUint64
Decorator_TestFailingQuantize( void *context, ReUint64 size, ReUint64 alignment )
{
    (void) context;
    (void) alignment;

    return size;
}

RE_INTERNAL ReAllocator
Decorator_TestFailingAllocator( void )
{
    ReAllocator allocator;
    allocator.context                = 0;
    allocator.alloc                  = Decorator_TestFailingAlloc;
    allocator.realloc                = Decorator_TestFailingRealloc;
    allocator.free                   = Decorator_TestFailingFree;
    allocator.quantize               = Decorator_TestFailingQuantize;
    allocator.isInternallyThreadSafe = RE_False;

    return allocator;
}

void
RE_Test_MemoryDecorator( void )
{
    RE_TEST_CHECK( RE_Heap_Init() );
    RE_Memory_SetReportFn( Decorator_TestReport );

    ReAllocator heap = RE_Heap_AsAllocator();

    /* --- poison: fresh memory is conspicuous, freed memory is conspicuously different --- */
    {
        ReMemoryPoisonDecorator storage;
        ReAllocator             poisoned = RE_MemoryDecorator_Poison( heap, &storage );

        ReUint8 *block = (ReUint8 *) RE_Memory_Alloc( &poisoned, 128, 16 );
        RE_TEST_CHECK_NOT_NULL( block );

        /* Reading uninitialised memory now yields an obvious garbage value rather than whatever
         * the previous owner left behind, which might have looked plausible.
         */
        RE_TEST_CHECK_EQ_UINT( block[0], RE_MEMORY_POISON_ALLOCATED );
        RE_TEST_CHECK_EQ_UINT( block[127], RE_MEMORY_POISON_ALLOCATED );

        RE_Memory_Free( &poisoned, block, 128 );

        /* Deliberately reading freed memory - the exact mistake poisoning exists to expose.
         *
         * Checked past the head of the block, not at it: the heap threads its free list through
         * the first bytes of a free bin, which is what makes magazines cost no memory. Those
         * bytes legitimately hold a pointer now. Everything after them is still poison.
         */
        RE_TEST_CHECK_EQ_UINT( block[64], RE_MEMORY_POISON_FREED );
        RE_TEST_CHECK_EQ_UINT( block[127], RE_MEMORY_POISON_FREED );
    }

    /* --- leak tracker --- */
    {
        Decorator_TestResetLog();

        ReMemoryLeakDecorator storage;
        ReAllocator           tracked = RE_MemoryDecorator_LeakTracker( heap, &storage, 1024, RE_False );

        void *kept    = RE_Memory_Alloc( &tracked, 64, 16 );
        void *release = RE_Memory_Alloc( &tracked, 32, 16 );

        RE_TEST_CHECK_NOT_NULL( kept );
        RE_TEST_CHECK_NOT_NULL( release );

        RE_Memory_Free( &tracked, release, 32 );

        /* One deliberately not freed. */
        RE_TEST_CHECK_EQ_UINT( RE_MemoryDecorator_ReportLeaks( &storage ), 1 );
        RE_TEST_CHECK( Decorator_TestLogContains( "still outstanding" ) );
        RE_TEST_CHECK( Decorator_TestLogContains( "64 bytes" ) );

        RE_Memory_Free( &tracked, kept, 64 );

        Decorator_TestResetLog();
        RE_TEST_CHECK_EQ_UINT( RE_MemoryDecorator_ReportLeaks( &storage ), 0 );
        RE_TEST_CHECK_EQ_UINT( gDecoratorTestMessages, 0 );
    }

    /* --- attribution, which rides along with the leak tracker --- */
    {
        ReMemoryLeakDecorator storage;
        ReAllocator           tracked = RE_MemoryDecorator_LeakTracker( heap, &storage, 1024, RE_False );

        RE_MEMORY_SCOPE_BEGIN( "TestTextures" );

        void *texture = RE_Memory_Alloc( &tracked, 4096, 16 );
        RE_TEST_CHECK_NOT_NULL( texture );

#if RE_BUILD < RE_BUILD_SHIPPING
        RE_TEST_CHECK( RE_Memory_CurrentTag() != 0 );
#else
        /* The scope macros compile to nothing in shipping, so there is no tag to find and
         * nothing is attributed. That is the point of them, not a gap in the test.
         */
        RE_TEST_CHECK_NULL( RE_Memory_CurrentTag() );
#endif

        RE_MEMORY_SCOPE_END();

        /* Outside the scope, so this must not be attributed to it. */
        void *untagged = RE_Memory_Alloc( &tracked, 4096, 16 );

        ReMemoryTagUsage usage[RE_MEMORY_MAX_TAGS];
        ReUint32         tagCount = RE_Memory_GetTagUsage( usage, RE_MEMORY_MAX_TAGS );

        ReBool found = RE_False;

        for ( ReUint32 i = 0; i < tagCount; i += 1 )
        {
            if ( usage[i].tag && strcmp( usage[i].tag, "TestTextures" ) == 0 )
            {
                found = RE_True;

                RE_TEST_CHECK_EQ_UINT( usage[i].allocationsInUse, 1 );
                RE_TEST_CHECK( usage[i].bytesInUse >= 4096 );
                RE_TEST_CHECK( usage[i].peakBytes >= 4096 );
            }
        }

#if RE_BUILD < RE_BUILD_SHIPPING
        RE_TEST_CHECK( found );
#else
        RE_TEST_CHECK( !found );
#endif

        RE_Memory_Free( &tracked, texture, 4096 );
        RE_Memory_Free( &tracked, untagged, 4096 );

        /* Freeing must decrement it again, or every report drifts upward forever. Vacuously true
         * in shipping, where nothing was recorded in the first place.
         */
        tagCount = RE_Memory_GetTagUsage( usage, RE_MEMORY_MAX_TAGS );

        for ( ReUint32 i = 0; i < tagCount; i += 1 )
        {
            if ( usage[i].tag && strcmp( usage[i].tag, "TestTextures" ) == 0 )
            {
                RE_TEST_CHECK_EQ_UINT( usage[i].allocationsInUse, 0 );
                RE_TEST_CHECK_EQ_UINT( usage[i].bytesInUse, 0 );

                /* The peak is a high-water mark and must not be reset by the free. */
                RE_TEST_CHECK( usage[i].peakBytes >= 4096 );
            }
        }
    }

    /* --- double free names the original free site instead of corrupting the heap --- */
    {
        Decorator_TestResetLog();

        ReMemoryDoubleFreeDecorator storage;
        ReAllocator                 guarded = RE_MemoryDecorator_DoubleFreeFinder( heap, &storage, 1024, RE_False );

        void *block = RE_Memory_Alloc( &guarded, 64, 16 );
        RE_TEST_CHECK_NOT_NULL( block );

        RE_Memory_Free( &guarded, block, 64 );
        RE_TEST_CHECK_EQ_UINT( storage.detected, 0 );

        /* The mistake. */
        RE_Memory_Free( &guarded, block, 64 );

        RE_TEST_CHECK_EQ_UINT( storage.detected, 1 );
        RE_TEST_CHECK( Decorator_TestLogContains( "double free" ) );

        /* Reusing the address clears its record, so the next genuine free is not a false
         * positive - otherwise every recycled address would report forever.
         */
        void *reused = RE_Memory_Alloc( &guarded, 64, 16 );
        RE_TEST_CHECK_NOT_NULL( reused );
        RE_Memory_Free( &guarded, reused, 64 );

        RE_TEST_CHECK_EQ_UINT( storage.detected, 1 );
    }

    /* --- quarantine catches the use-after-free that poisoning alone misses --- */
    {
        Decorator_TestResetLog();

        ReMemoryQuarantineDecorator storage;
        ReAllocator quarantined = RE_MemoryDecorator_Quarantine( heap, &storage, 64, 2, 1024 * 1024 );

        ReUint8 *block = (ReUint8 *) RE_Memory_Alloc( &quarantined, 128, 16 );
        RE_TEST_CHECK_NOT_NULL( block );

        RE_Memory_Free( &quarantined, block, 128 );

        /* Held rather than released, so the memory has not been recycled and nothing else has
         * had a chance to overwrite it yet. This is precisely the window where poisoning cannot
         * help.
         */
        RE_TEST_CHECK_EQ_UINT( storage.count, 1 );
        RE_TEST_CHECK_EQ_UINT( block[0], RE_MEMORY_QUARANTINE_BYTE );

        /* The bug: writing through a stale pointer. */
        block[64] = 0x01;

        RE_MemoryDecorator_QuarantineTick( &storage );
        RE_MemoryDecorator_QuarantineTick( &storage );

        RE_TEST_CHECK_EQ_UINT( storage.violations, 1 );
        RE_TEST_CHECK( Decorator_TestLogContains( "use-after-free" ) );
        RE_TEST_CHECK( Decorator_TestLogContains( "offset 64" ) );
    }

    /* --- quarantine flushes early when its byte cap is hit, so a level load cannot grow it
           without bound --- */
    {
        ReMemoryQuarantineDecorator storage;
        ReAllocator quarantined = RE_MemoryDecorator_Quarantine( heap, &storage, 64, 1000, 4096 );

        for ( ReUint32 i = 0; i < 32; i += 1 )
        {
            void *block = RE_Memory_Alloc( &quarantined, 512, 16 );

            if ( block )
            {
                RE_Memory_Free( &quarantined, block, 512 );
            }
        }

        /* The delay is 1000 ticks and none have passed, so only the byte cap can be holding this
         * down.
         */
        RE_TEST_CHECK( storage.bytesHeld <= 4096 );
    }

    /* --- slack verifier catches a small overrun --- */
    {
        Decorator_TestResetLog();

        ReMemoryVerifierDecorator storage;
        ReAllocator               verified = RE_MemoryDecorator_Verifier( heap, &storage );

        /* 100 rounds up to a larger class, so there is slack to put a canary in. */
        ReUint8 *block = (ReUint8 *) RE_Memory_Alloc( &verified, 100, 16 );
        RE_TEST_CHECK_NOT_NULL( block );

        RE_TEST_CHECK( RE_Heap_AllocationSize( block ) > 100 );

        /* One byte past the request - the classic off-by-one, invisible to everything else
         * because the allocator really did hand over that byte.
         */
        block[100] = 0x00;

        RE_Memory_Free( &verified, block, 100 );

        RE_TEST_CHECK_EQ_UINT( RE_Atomic_LoadUint64( &storage.violations ), 1 );
        RE_TEST_CHECK( Decorator_TestLogContains( "overrun" ) );
    }

    /* --- a clean allocation must not report anything --- */
    {
        Decorator_TestResetLog();

        ReMemoryVerifierDecorator storage;
        ReAllocator               verified = RE_MemoryDecorator_Verifier( heap, &storage );

        ReUint8 *block = (ReUint8 *) RE_Memory_Alloc( &verified, 100, 16 );
        RE_Memory_Set( block, 0x11, 100 );
        RE_Memory_Free( &verified, block, 100 );

        RE_TEST_CHECK_EQ_UINT( RE_Atomic_LoadUint64( &storage.violations ), 0 );
        RE_TEST_CHECK_EQ_UINT( gDecoratorTestMessages, 0 );
    }

    /* --- guard page: an overrun faults at the instruction that caused it --- */
    {
        ReMemoryGuardPageDecorator storage;
        ReAllocator                guarded = RE_MemoryDecorator_GuardPage( heap, &storage );

        ReUint8 *block = (ReUint8 *) RE_Memory_Alloc( &guarded, 64, 16 );
        RE_TEST_CHECK_NOT_NULL( block );
        RE_TEST_CHECK_EQ_UINT( storage.activeAllocations, 1 );

        /* Writing inside the allocation is fine. */
        RE_Memory_Set( block, 0x22, 64 );
        RE_TEST_CHECK_EQ_UINT( block[63], 0x22 );

        ReBool faulted = RE_False;

        __try
        {
            /* Past the end. The next page is reserved but never committed, so this is an access
             * violation right here rather than silent damage to a neighbour.
             */
            block[64 + 4096] = 0x33;
        }
        __except ( EXCEPTION_EXECUTE_HANDLER )
        {
            faulted = RE_True;
        }

        RE_TEST_CHECK( faulted );

        RE_Memory_Free( &guarded, block, 64 );
        RE_TEST_CHECK_EQ_UINT( storage.activeAllocations, 0 );
    }

    /* --- out of memory reports rather than failing silently, and keeps a reserve to do it --- */
    {
        Decorator_TestResetLog();

        ReMemoryOutOfMemoryDecorator storage;
        ReAllocator failing = RE_MemoryDecorator_OutOfMemory( Decorator_TestFailingAllocator(),
            &storage, 64 * 1024 );

        RE_TEST_CHECK_NOT_NULL( storage.reserve );

        RE_TEST_CHECK_NULL( RE_Memory_Alloc( &failing, 128, 16 ) );

        RE_TEST_CHECK_EQ_UINT( storage.failures, 1 );
        RE_TEST_CHECK( Decorator_TestLogContains( "out of memory" ) );

        /* The reserve was handed back so that reporting had room to work. */
        RE_TEST_CHECK_NULL( storage.reserve );
    }

    /* --- the locking proxy goes on only where it is needed --- */
    {
        ReMemoryDecoratorConfig config = RE_Memory_DefaultDecoratorConfig();
        config.trackLeaks     = RE_False;
        config.poison         = RE_False;
        config.findDoubleFree = RE_False;

        /* Over the binned heap, which locks per size class internally. Wrapping it would
         * serialise every allocation in the engine.
         */
        {
            ReMemoryDecoratorChain chain;
            ReAllocator built = RE_Memory_BuildDecoratorChain( heap, &config, &chain );

            RE_TEST_CHECK( built.isInternallyThreadSafe );
            RE_TEST_CHECK( chain.locking.inner.alloc == 0 );
        }

        /* Over something that says it is not, where the proxy is exactly what is wanted. */
        {
            ReMemoryDecoratorChain chain;
            ReAllocator built = RE_Memory_BuildDecoratorChain( Decorator_TestFailingAllocator(),
                &config, &chain );

            RE_TEST_CHECK( built.isInternallyThreadSafe );
            RE_TEST_CHECK( chain.locking.inner.alloc != 0 );
        }
    }

    /* --- a chain end to end --- */
    {
        Decorator_TestResetLog();

        ReMemoryDecoratorConfig config = RE_Memory_DefaultDecoratorConfig();
        config.poison            = RE_True;
        config.trackLeaks        = RE_True;
        config.findDoubleFree    = RE_True;
        config.captureCallstacks = RE_False;
        config.recordCapacity    = 4096;

        ReMemoryDecoratorChain chain;
        ReAllocator            built = RE_Memory_BuildDecoratorChain( heap, &config, &chain );

        enum { Count = 64 };

        void *blocks[Count];

        for ( ReUint32 i = 0; i < Count; i += 1 )
        {
            blocks[i] = RE_Memory_Alloc( &built, 64 + i, 16 );
            RE_TEST_CHECK_NOT_NULL( blocks[i] );

            /* Every layer must have passed the request through intact. */
            if ( blocks[i] )
            {
                RE_TEST_CHECK_EQ_UINT( ( (ReUint8 *) blocks[i] )[0], RE_MEMORY_POISON_ALLOCATED );
            }
        }

        for ( ReUint32 i = 0; i < Count; i += 1 )
        {
            RE_Memory_Free( &built, blocks[i], 64 + i );
        }

        RE_TEST_CHECK_EQ_UINT( RE_Memory_ReportDecoratorFindings( &chain ), 0 );
        RE_TEST_CHECK_EQ_UINT( gDecoratorTestMessages, 0 );

        /* And a chain that has seen a problem must say so. */
        void *leaked = RE_Memory_Alloc( &built, 256, 16 );
        RE_TEST_CHECK_NOT_NULL( leaked );

        RE_TEST_CHECK( RE_Memory_ReportDecoratorFindings( &chain ) >= 1 );

        RE_Memory_Free( &built, leaked, 256 );
    }

    /* --- shipping builds must carry none of this --- */
    {
        ReMemoryDecoratorConfig config = RE_Memory_DefaultDecoratorConfig();

#if RE_BUILD == RE_BUILD_SHIPPING
        RE_TEST_CHECK( !config.poison );
        RE_TEST_CHECK( !config.trackLeaks );
        RE_TEST_CHECK( !config.findDoubleFree );
        RE_TEST_CHECK( !config.captureCallstacks );

        /* The scope macros compile to nothing, so the tag stack never moves. */
        RE_MEMORY_SCOPE_BEGIN( "ShouldNotRegister" );
        RE_TEST_CHECK_NULL( RE_Memory_CurrentTag() );
        RE_MEMORY_SCOPE_END();
#else
        RE_TEST_CHECK( config.poison );
        RE_TEST_CHECK( config.trackLeaks );
#endif
    }

    RE_Memory_SetReportFn( 0 );
    RE_Heap_Trim();
}
