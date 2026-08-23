/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Foundation/FoundationMemoryDepot.h"

#include <RE/Foundation/FoundationAtomic.h>
#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemorySizeClass.h>

/*
    Each class gets its own array of slots, padded to a cache line. Without the padding, two
    threads working on different size classes false-share the line their slots sit on and most of
    the benefit disappears - invisibly, as scaling that never arrives.
*/

typedef struct ReDepotClass
{
    ReAtomicPtr slots[RE_DEPOT_SLOTS_PER_CLASS];
} ReDepotClass;

#define RE_DEPOT_CLASS_SLOT_SIZE \
    ( RE_CACHE_LINE_SIZE * ( ( sizeof( ReDepotClass ) + RE_CACHE_LINE_SIZE - 1 ) / RE_CACHE_LINE_SIZE ) )

typedef union ReDepotClassSlot
{
    ReDepotClass state;
    ReUint8      padding[RE_DEPOT_CLASS_SLOT_SIZE];
} ReDepotClassSlot;

RE_GLOBAL ReDepotClassSlot *gDepotClasses;
RE_GLOBAL ReUint32          gDepotClassCount;

ReBool
RE_Depot_Init( void )
{
    if ( gDepotClasses )
    {
        return RE_True;
    }

    gDepotClassCount = RE_HeapSizeClass_Count();

    gDepotClasses = (ReDepotClassSlot *) RE_MemoryMetadata_Alloc(
        gDepotClassCount * sizeof( ReDepotClassSlot ), RE_CACHE_LINE_SIZE );

    return (ReBool) ( gDepotClasses != 0 );
}

void
RE_Depot_Shutdown( void )
{
    gDepotClasses    = 0;
    gDepotClassCount = 0;
}

ReBool
RE_Depot_Push( ReUint32 classIndex, ReMagazineNode *magazine )
{
    if ( !gDepotClasses || classIndex >= gDepotClassCount )
    {
        return RE_False;
    }

    ReDepotClass *depot = &gDepotClasses[classIndex].state;

    for ( ReUint32 i = 0; i < RE_DEPOT_SLOTS_PER_CLASS; i += 1 )
    {
        if ( RE_Atomic_LoadPtr( &depot->slots[i] ) != 0 )
        {
            continue;
        }

        if ( RE_Atomic_CompareExchangePtr( &depot->slots[i], 0, magazine ) == 0 )
        {
            return RE_True;
        }
    }

    return RE_False;
}

ReMagazineNode *
RE_Depot_Pop( ReUint32 classIndex )
{
    if ( !gDepotClasses || classIndex >= gDepotClassCount )
    {
        return 0;
    }

    ReDepotClass *depot = &gDepotClasses[classIndex].state;

    for ( ReUint32 i = 0; i < RE_DEPOT_SLOTS_PER_CLASS; i += 1 )
    {
        ReMagazineNode *magazine = (ReMagazineNode *) RE_Atomic_LoadPtr( &depot->slots[i] );

        if ( !magazine )
        {
            continue;
        }

        /* No ABA hazard here, despite the shape. The classic problem is a compare-exchange
         * succeeding against stale associated state; here the slot's value *is* the ownership
         * token and carries nothing else. If this same magazine were popped by another thread,
         * consumed, rebuilt and pushed back before this exchange, then what is in the slot is a
         * valid unowned magazine either way - which is exactly what we are claiming.
         */
        if ( RE_Atomic_CompareExchangePtr( &depot->slots[i], magazine, 0 ) == magazine )
        {
            return magazine;
        }
    }

    return 0;
}

void
RE_Depot_Flush( void )
{
    if ( !gDepotClasses )
    {
        return;
    }

    for ( ReUint32 classIndex = 0; classIndex < gDepotClassCount; classIndex += 1 )
    {
        ReDepotClass *depot = &gDepotClasses[classIndex].state;

        for ( ReUint32 i = 0; i < RE_DEPOT_SLOTS_PER_CLASS; i += 1 )
        {
            ReMagazineNode *magazine = (ReMagazineNode *) RE_Atomic_ExchangePtr( &depot->slots[i], 0 );

            if ( magazine )
            {
                RE_HeapInternal_FreeChain( classIndex, magazine );
            }
        }
    }
}
