/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationTest.h"

#include <RE/Foundation/FoundationMemoryHandlePool.h>
#include <RE/Foundation/FoundationMemoryPool.h>
#include <RE/Foundation/FoundationMemoryUtility.h>

typedef struct RePoolTestItem
{
    ReUint64 identifier;
    ReUint64 payload;
} RePoolTestItem;

void
RE_Test_MemoryPool( void )
{
    /* --- fixed mode: capacity derived from the buffer, exhaustion reported --- */
    {
        ReUint8 buffer[1024];
        RePool  pool;

        RE_TEST_CHECK( RE_Pool_InitFixed( &pool, buffer, sizeof( buffer ), 32, 16 ) );
        RE_TEST_CHECK_EQ_UINT( RE_Pool_SlotStride( &pool ), 32 );
        RE_TEST_CHECK( RE_Pool_Capacity( &pool ) >= 31 );
        RE_TEST_CHECK_EQ_UINT( RE_Pool_Count( &pool ), 0 );

        ReUint64 capacity = RE_Pool_Capacity( &pool );

        for ( ReUint64 i = 0; i < capacity; i += 1 )
        {
            void *slot = RE_Pool_Alloc( &pool );
            RE_TEST_CHECK_NOT_NULL( slot );

            if ( slot )
            {
                RE_TEST_CHECK_EQ_UINT( (ReUint64) slot & 15, 0 );
            }
        }

        RE_TEST_CHECK_EQ_UINT( RE_Pool_Count( &pool ), capacity );
        RE_TEST_CHECK_NULL( RE_Pool_Alloc( &pool ) );

        RE_Pool_Shutdown( &pool );
    }

    /* --- the stride floor: a slot must be able to hold the free-list link --- */
    {
        ReUint8 buffer[256];
        RePool  pool;

        RE_TEST_CHECK( RE_Pool_InitFixed( &pool, buffer, sizeof( buffer ), 1, 1 ) );
        RE_TEST_CHECK( RE_Pool_SlotStride( &pool ) >= sizeof( void * ) );

        RE_Pool_Shutdown( &pool );
    }

    /* --- free then alloc reuses the slot, and the list is LIFO --- */
    {
        ReUint8 buffer[1024];
        RePool  pool;
        RE_TEST_CHECK( RE_Pool_InitFixed( &pool, buffer, sizeof( buffer ), 32, 16 ) );

        void *a = RE_Pool_Alloc( &pool );
        void *b = RE_Pool_Alloc( &pool );
        void *c = RE_Pool_Alloc( &pool );

        RE_TEST_CHECK( a && b && c );
        RE_TEST_CHECK_EQ_UINT( RE_Pool_Count( &pool ), 3 );

        RE_Pool_Free( &pool, b );
        RE_TEST_CHECK_EQ_UINT( RE_Pool_Count( &pool ), 2 );

        RE_TEST_CHECK( RE_Pool_Alloc( &pool ) == b );

        RE_Pool_Free( &pool, a );
        RE_Pool_Free( &pool, c );

        /* Most recently freed comes back first - it is the one most likely still in cache. */
        RE_TEST_CHECK( RE_Pool_Alloc( &pool ) == c );
        RE_TEST_CHECK( RE_Pool_Alloc( &pool ) == a );

        RE_Pool_Shutdown( &pool );
    }

    /* --- virtual mode grows without ever moving what is already handed out --- */
    {
        RePool pool;
        RE_TEST_CHECK( RE_Pool_InitVirtual( &pool, 200000, sizeof( RePoolTestItem ), 16 ) );

        enum { TrackedCount = 4096 };

        RePoolTestItem *tracked[TrackedCount];

        for ( ReUint64 i = 0; i < TrackedCount; i += 1 )
        {
            tracked[i] = (RePoolTestItem *) RE_Pool_Alloc( &pool );
            RE_TEST_CHECK_NOT_NULL( tracked[i] );

            if ( tracked[i] )
            {
                tracked[i]->identifier = i;
                tracked[i]->payload    = i * 3;
            }
        }

        /* Force growth well past the initial commit, then confirm nothing already handed out has
         * moved or been overwritten. Growth by reallocation would fail exactly here.
         */
        for ( ReUint64 i = 0; i < 100000; i += 1 )
        {
            RE_TEST_CHECK_NOT_NULL( RE_Pool_Alloc( &pool ) );
        }

        for ( ReUint64 i = 0; i < TrackedCount; i += 1 )
        {
            if ( tracked[i] )
            {
                RE_TEST_CHECK_EQ_UINT( tracked[i]->identifier, i );
                RE_TEST_CHECK_EQ_UINT( tracked[i]->payload, i * 3 );
            }
        }

        RE_TEST_CHECK_EQ_UINT( RE_Pool_HighWater( &pool ), TrackedCount + 100000 );

        RE_Pool_Reset( &pool );
        RE_TEST_CHECK_EQ_UINT( RE_Pool_Count( &pool ), 0 );

        /* After a reset the pool hands out from the beginning again. */
        RE_TEST_CHECK( RE_Pool_Alloc( &pool ) == (void *) pool.base );

        RE_Pool_Shutdown( &pool );
    }

    /* --- the uniform interface, including the size limits a pool imposes --- */
    {
        RePool pool;
        RE_TEST_CHECK( RE_Pool_InitVirtual( &pool, 64, 32, 16 ) );

        ReAllocator allocator = RE_Pool_AsAllocator( &pool );

        RE_TEST_CHECK( !allocator.isInternallyThreadSafe );

        void *fits = RE_Memory_Alloc( &allocator, 32, 16 );
        RE_TEST_CHECK_NOT_NULL( fits );

        /* Too large for a slot is indistinguishable from out of memory through a generic
         * interface, which is the right thing for the caller to see either way.
         */
        RE_TEST_CHECK_NULL( RE_Memory_Alloc( &allocator, 33, 16 ) );

        /* A request that fits gets the whole slot, so a container may as well use all of it. */
        RE_TEST_CHECK_EQ_UINT( RE_Memory_Quantize( &allocator, 20, 16 ), 32 );

        /* Growing within the slot keeps the same block; growing beyond it cannot be served. */
        RE_TEST_CHECK( RE_Memory_Realloc( &allocator, fits, 20, 30, 16 ) == fits );
        RE_TEST_CHECK_NULL( RE_Memory_Realloc( &allocator, fits, 20, 64, 16 ) );

        RE_Memory_Free( &allocator, fits, 32 );

        RE_Pool_Shutdown( &pool );
    }

    /* --- degenerate inputs --- */
    {
        RePool pool;

        RE_TEST_CHECK( !RE_Pool_InitFixed( &pool, 0, 1024, 32, 16 ) );
        RE_TEST_CHECK( !RE_Pool_InitVirtual( &pool, 0, 32, 16 ) );

        /* A buffer too small for even one slot has to be refused, not silently accepted with a
         * capacity of zero that fails on first use.
         */
        ReUint8 tiny[8];
        RE_TEST_CHECK( !RE_Pool_InitFixed( &pool, tiny, sizeof( tiny ), 64, 16 ) );
    }
}

void
RE_Test_MemoryHandlePool( void )
{
    /* --- a zeroed handle is invalid, which is what makes default-initialised fields safe --- */
    {
        ReHandle zeroed;
        zeroed.value = 0;

        RE_TEST_CHECK( RE_Handle_IsNull( zeroed ) );
        RE_TEST_CHECK( RE_Handle_IsNull( RE_Handle_Null() ) );
    }

    /* --- allocate, resolve, free --- */
    {
        ReHandlePool pool;
        RE_TEST_CHECK( RE_HandlePool_Init( &pool, 1024, sizeof( RePoolTestItem ), 16 ) );

        ReHandle handle = RE_HandlePool_Alloc( &pool );
        RE_TEST_CHECK( !RE_Handle_IsNull( handle ) );
        RE_TEST_CHECK( RE_HandlePool_IsValid( &pool, handle ) );
        RE_TEST_CHECK_EQ_UINT( RE_HandlePool_Count( &pool ), 1 );

        RePoolTestItem *item = (RePoolTestItem *) RE_HandlePool_Resolve( &pool, handle );
        RE_TEST_CHECK_NOT_NULL( item );

        if ( item )
        {
            item->identifier = 0xABCD;
            RE_TEST_CHECK_EQ_UINT( ( (RePoolTestItem *) RE_HandlePool_Resolve( &pool, handle ) )->identifier, 0xABCD );
        }

        RE_HandlePool_Free( &pool, handle );
        RE_TEST_CHECK_EQ_UINT( RE_HandlePool_Count( &pool ), 0 );

        /* The whole point: after the free the handle resolves to null instead of to whatever now
         * occupies the slot.
         */
        RE_TEST_CHECK( !RE_HandlePool_IsValid( &pool, handle ) );
        RE_TEST_CHECK_NULL( RE_HandlePool_Resolve( &pool, handle ) );

        RE_HandlePool_Shutdown( &pool );
    }

    /* --- a recycled slot does not answer to the old handle --- */
    {
        ReHandlePool pool;
        RE_TEST_CHECK( RE_HandlePool_Init( &pool, 1024, sizeof( RePoolTestItem ), 16 ) );

        ReHandle first = RE_HandlePool_Alloc( &pool );
        void    *firstAddress = RE_HandlePool_Resolve( &pool, first );

        RE_HandlePool_Free( &pool, first );

        ReHandle second = RE_HandlePool_Alloc( &pool );

        /* Same storage reused... */
        RE_TEST_CHECK( RE_HandlePool_Resolve( &pool, second ) == firstAddress );

        /* ...but the old handle is dead and the new one is live. Without the generation counter
         * the stale handle would happily resolve to the new object.
         */
        RE_TEST_CHECK( first.value != second.value );
        RE_TEST_CHECK_NULL( RE_HandlePool_Resolve( &pool, first ) );
        RE_TEST_CHECK_NOT_NULL( RE_HandlePool_Resolve( &pool, second ) );

        RE_HandlePool_Shutdown( &pool );
    }

    /* --- double free is a no-op rather than corruption --- */
    {
        ReHandlePool pool;
        RE_TEST_CHECK( RE_HandlePool_Init( &pool, 64, sizeof( RePoolTestItem ), 16 ) );

        ReHandle handle = RE_HandlePool_Alloc( &pool );
        RE_HandlePool_Free( &pool, handle );
        RE_HandlePool_Free( &pool, handle );
        RE_HandlePool_Free( &pool, RE_Handle_Null() );

        RE_TEST_CHECK_EQ_UINT( RE_HandlePool_Count( &pool ), 0 );

        /* The slot is still usable afterwards - the redundant frees must not have corrupted the
         * free list by pushing the same slot twice.
         */
        RE_TEST_CHECK( !RE_Handle_IsNull( RE_HandlePool_Alloc( &pool ) ) );
        RE_TEST_CHECK_EQ_UINT( RE_HandlePool_Count( &pool ), 1 );

        RE_HandlePool_Shutdown( &pool );
    }

    /* --- a fabricated out-of-range handle is rejected, not dereferenced --- */
    {
        ReHandlePool pool;
        RE_TEST_CHECK( RE_HandlePool_Init( &pool, 64, sizeof( RePoolTestItem ), 16 ) );

        ReHandle bogus;
        bogus.value = 5000;

        RE_TEST_CHECK( !RE_HandlePool_IsValid( &pool, bogus ) );
        RE_TEST_CHECK_NULL( RE_HandlePool_Resolve( &pool, bogus ) );

        RE_HandlePool_Shutdown( &pool );
    }

    /* --- generation wrap: after enough recycles an old handle can collide again --- */
    {
        ReHandlePool pool;
        RE_TEST_CHECK( RE_HandlePool_Init( &pool, 8, sizeof( RePoolTestItem ), 16 ) );

        ReHandle original = RE_HandlePool_Alloc( &pool );

        ReUint32 cycles = 1u << RE_HANDLE_GENERATION_BITS;
        for ( ReUint32 i = 0; i < cycles; i += 1 )
        {
            ReHandle current = RE_HandlePool_Alloc( &pool );
            RE_HandlePool_Free( &pool, current );
        }

        /* Documenting the limit rather than pretending it does not exist: the original handle is
         * still live here because slot 0 was never recycled, and a pool that churns one slot
         * 2^RE_HANDLE_GENERATION_BITS times will hand out a colliding handle. Raise the
         * generation width for high-churn pools.
         */
        RE_TEST_CHECK( RE_HandlePool_IsValid( &pool, original ) );

        RE_HandlePool_Shutdown( &pool );
    }

    /* --- iteration visits exactly the live slots --- */
    {
        ReHandlePool pool;
        RE_TEST_CHECK( RE_HandlePool_Init( &pool, 256, sizeof( RePoolTestItem ), 16 ) );

        enum { Created = 32 };

        ReHandle handles[Created];

        for ( ReUint64 i = 0; i < Created; i += 1 )
        {
            handles[i] = RE_HandlePool_Alloc( &pool );

            RePoolTestItem *item = (RePoolTestItem *) RE_HandlePool_Resolve( &pool, handles[i] );
            if ( item )
            {
                item->identifier = i;
            }
        }

        /* Free every other one, leaving holes for the iterator to skip. */
        for ( ReUint64 i = 0; i < Created; i += 2 )
        {
            RE_HandlePool_Free( &pool, handles[i] );
        }

        RE_TEST_CHECK_EQ_UINT( RE_HandlePool_Count( &pool ), Created / 2 );

        ReUint64 cursor  = 0;
        ReUint64 visited = 0;
        ReHandle handle;
        void    *item;

        while ( RE_HandlePool_Next( &pool, &cursor, &handle, &item ) )
        {
            RE_TEST_CHECK( RE_HandlePool_IsValid( &pool, handle ) );

            /* Only the odd-numbered items survived. */
            RE_TEST_CHECK( ( ( (RePoolTestItem *) item )->identifier & 1 ) == 1 );

            visited += 1;
        }

        RE_TEST_CHECK_EQ_UINT( visited, Created / 2 );

        RE_HandlePool_Shutdown( &pool );
    }

    /* --- exhaustion returns the null handle --- */
    {
        ReHandlePool pool;
        RE_TEST_CHECK( RE_HandlePool_Init( &pool, 4, sizeof( RePoolTestItem ), 16 ) );

        for ( ReUint32 i = 0; i < 4; i += 1 )
        {
            RE_TEST_CHECK( !RE_Handle_IsNull( RE_HandlePool_Alloc( &pool ) ) );
        }

        RE_TEST_CHECK( RE_Handle_IsNull( RE_HandlePool_Alloc( &pool ) ) );

        RE_HandlePool_Shutdown( &pool );
    }

    /* --- capacity beyond what the index field can express is refused --- */
    {
        ReHandlePool pool;
        RE_TEST_CHECK( !RE_HandlePool_Init( &pool, RE_HANDLE_MAX_SLOTS, sizeof( RePoolTestItem ), 16 ) );
    }
}
