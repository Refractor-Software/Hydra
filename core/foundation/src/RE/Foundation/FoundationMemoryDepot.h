/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include "RE/Foundation/FoundationMemoryHeapInternal.h"

/*
    FoundationMemoryDepot.h

    A small lock-free pool of full magazines, per size class, that threads hand memory between.

    This is what makes asymmetric allocation work. Engines constantly have one thread producing
    objects that another consumes and frees - a loader filling structures a worker tears down, a
    job queue where the producer and the consumer are never the same thread. With per-thread free
    lists alone, the producer's freed memory is stranded in its own cache while the consumer
    starves into the slow path forever, and neither thread can fix it.

    A producer pushes its full magazines here; a consumer pops them. Neither takes the size class
    lock, so the transfer costs a single compare-exchange.

    @threadsafe Yes, and lock-free.
*/

/* Enough to buffer a burst without letting an idle class sit on much memory. */
#define RE_DEPOT_SLOTS_PER_CLASS 8

ReBool RE_Depot_Init( void );
void   RE_Depot_Shutdown( void );

/* Parks a full magazine. Returns RE_False when every slot is taken, in which case the caller
 * still owns the magazine and has to return it to the heap properly.
 */
ReBool RE_Depot_Push( ReUint32 classIndex, ReMagazineNode *magazine );

/* Claims a full magazine, or returns 0 if none is available. */
ReMagazineNode *RE_Depot_Pop( ReUint32 classIndex );

/* Empties every slot back into the heap. Called from trim. */
void RE_Depot_Flush( void );
