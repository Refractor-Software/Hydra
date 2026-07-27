/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationMemoryDecorator.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include <RE/Foundation/FoundationBuild.h>
#include <RE/Foundation/FoundationMemoryMetadata.h>
#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationVirtualMemory.h>

/*
    All of the decorators live together because they are the same shape: a stored inner allocator,
    four forwarding functions, and a little state. Splitting them across nine files would multiply
    the boilerplate without separating anything that is actually independent.
*/

/* ------------------------------------------------------------------------------------------- */
/* Reporting                                                                                    */
/* ------------------------------------------------------------------------------------------- */

RE_GLOBAL ReMemoryReportFn gMemoryReport;

void
RE_Memory_SetReportFn( ReMemoryReportFn report )
{
    gMemoryReport = report;
}

void
RE_Memory_Report( const char *format, ... )
{
    if ( !gMemoryReport )
    {
        return;
    }

    char buffer[1024];

    va_list args;
    va_start( args, format );
    vsnprintf( buffer, sizeof( buffer ), format, args );
    va_end( args );

    gMemoryReport( buffer );
}

/* ------------------------------------------------------------------------------------------- */
/* Attribution                                                                                  */
/* ------------------------------------------------------------------------------------------- */

RE_GLOBAL RE_THREAD_LOCAL const char *gMemoryTagStack[RE_MEMORY_TAG_STACK_DEPTH];
RE_GLOBAL RE_THREAD_LOCAL ReUint32    gMemoryTagDepth;

RE_GLOBAL ReMemoryTagUsage gMemoryTagUsage[RE_MEMORY_MAX_TAGS];
RE_GLOBAL ReUint32         gMemoryTagCount;
RE_GLOBAL ReSpinLock       gMemoryTagLock;

void
RE_Memory_PushTag( const char *tag )
{
    if ( gMemoryTagDepth < RE_MEMORY_TAG_STACK_DEPTH )
    {
        gMemoryTagStack[gMemoryTagDepth] = tag;
    }

    /* Counted past the ceiling so that pop stays balanced even when a scope nests too deeply -
     * an unbalanced stack would misattribute everything after it, which is worse than losing
     * attribution for the over-deep scope itself.
     */
    gMemoryTagDepth += 1;
}

void
RE_Memory_PopTag( void )
{
    assert( gMemoryTagDepth > 0 && "RE_MEMORY_SCOPE_END without a matching begin" );

    if ( gMemoryTagDepth > 0 )
    {
        gMemoryTagDepth -= 1;
    }
}

const char *
RE_Memory_CurrentTag( void )
{
    if ( gMemoryTagDepth == 0 || gMemoryTagDepth > RE_MEMORY_TAG_STACK_DEPTH )
    {
        return 0;
    }

    return gMemoryTagStack[gMemoryTagDepth - 1];
}

/* Tags are string literals, so identity is a pointer compare and registration never copies. */
RE_INTERNAL void
Memory_TagAccount( const char *tag, ReSint64 byteDelta, ReSint64 countDelta )
{
    if ( !tag )
    {
        return;
    }

    RE_SpinLock_Acquire( &gMemoryTagLock );

    ReMemoryTagUsage *usage = 0;

    for ( ReUint32 i = 0; i < gMemoryTagCount; i += 1 )
    {
        if ( gMemoryTagUsage[i].tag == tag )
        {
            usage = &gMemoryTagUsage[i];
            break;
        }
    }

    if ( !usage && gMemoryTagCount < RE_MEMORY_MAX_TAGS )
    {
        usage      = &gMemoryTagUsage[gMemoryTagCount];
        usage->tag = tag;

        gMemoryTagCount += 1;
    }

    if ( usage )
    {
        usage->bytesInUse       = (ReUint64) ( (ReSint64) usage->bytesInUse + byteDelta );
        usage->allocationsInUse = (ReUint64) ( (ReSint64) usage->allocationsInUse + countDelta );

        if ( usage->bytesInUse > usage->peakBytes )
        {
            usage->peakBytes = usage->bytesInUse;
        }
    }

    RE_SpinLock_Release( &gMemoryTagLock );
}

ReUint32
RE_Memory_GetTagUsage( ReMemoryTagUsage *outTags, ReUint32 maxTags )
{
    RE_SpinLock_Acquire( &gMemoryTagLock );

    ReUint32 count = gMemoryTagCount;

    if ( outTags )
    {
        ReUint32 copy = ( count < maxTags ) ? count : maxTags;

        for ( ReUint32 i = 0; i < copy; i += 1 )
        {
            outTags[i] = gMemoryTagUsage[i];
        }
    }

    RE_SpinLock_Release( &gMemoryTagLock );

    return count;
}

/* ------------------------------------------------------------------------------------------- */
/* Record table                                                                                 */
/* ------------------------------------------------------------------------------------------- */

/* Open addressing with linear probing. A removed slot becomes a tombstone rather than empty, or
 * a probe sequence that ran through it would terminate early and lose later entries.
 */
#define RECORD_TOMBSTONE ( (void *) (ReUint64) 1 )

RE_INTERNAL ReBool
Record_TableInit( ReMemoryRecordTable *table, ReUint64 capacity )
{
    ReUint64 rounded = 16;
    while ( rounded < capacity )
    {
        rounded *= 2;
    }

    table->records = (ReMemoryAllocationRecord *) RE_MemoryMetadata_Alloc(
        rounded * sizeof( ReMemoryAllocationRecord ), 64 );

    if ( !table->records )
    {
        return RE_False;
    }

    table->capacity = rounded;
    table->count    = 0;

    RE_SpinLock_Init( &table->lock );

    return RE_True;
}

RE_INTERNAL ReUint64
Record_Slot( const ReMemoryRecordTable *table, const void *block )
{
    ReUint64 value = ( (ReUint64) block >> 4 ) * 11400714819323198485ull;

    return ( value >> 32 ) & ( table->capacity - 1 );
}

/* Caller holds the table lock. */
RE_INTERNAL ReMemoryAllocationRecord *
Record_Find( ReMemoryRecordTable *table, const void *block )
{
    ReUint64 slot = Record_Slot( table, block );

    for ( ReUint64 probe = 0; probe < table->capacity; probe += 1 )
    {
        ReMemoryAllocationRecord *record = &table->records[( slot + probe ) & ( table->capacity - 1 )];

        if ( record->block == 0 )
        {
            return 0;
        }

        if ( record->block == block )
        {
            return record;
        }
    }

    return 0;
}

/* Caller holds the table lock. Returns 0 when the table is full. */
RE_INTERNAL ReMemoryAllocationRecord *
Record_Insert( ReMemoryRecordTable *table, void *block )
{
    ReUint64 slot = Record_Slot( table, block );

    for ( ReUint64 probe = 0; probe < table->capacity; probe += 1 )
    {
        ReMemoryAllocationRecord *record = &table->records[( slot + probe ) & ( table->capacity - 1 )];

        if ( record->block == 0 || record->block == RECORD_TOMBSTONE || record->block == block )
        {
            RE_Memory_Zero( record, sizeof( *record ) );

            record->block = block;
            table->count += 1;

            return record;
        }
    }

    return 0;
}

/* Caller holds the table lock. */
RE_INTERNAL ReBool
Record_Remove( ReMemoryRecordTable *table, const void *block, ReMemoryAllocationRecord *outCopy )
{
    ReMemoryAllocationRecord *record = Record_Find( table, block );

    if ( !record )
    {
        return RE_False;
    }

    if ( outCopy )
    {
        *outCopy = *record;
    }

    RE_Memory_Zero( record, sizeof( *record ) );
    record->block = RECORD_TOMBSTONE;

    table->count -= 1;

    return RE_True;
}

RE_INTERNAL void
Record_CaptureStack( ReMemoryAllocationRecord *record, ReBool capture )
{
    if ( !capture )
    {
        record->frameCount = 0;

        return;
    }

    /* Two frames skipped: this helper and the decorator entry point that called it. */
    record->frameCount = RE_Debug_CaptureCallstack( record->frames, RE_CALLSTACK_MAX_FRAMES, 2 );
}

RE_INTERNAL void
Record_ReportStack( const ReMemoryAllocationRecord *record )
{
    if ( record->frameCount == 0 )
    {
        return;
    }

    char stack[2048];
    RE_Debug_FormatCallstack( record->frames, record->frameCount, stack, sizeof( stack ) );

    RE_Memory_Report( "%s", stack );
}

/* ------------------------------------------------------------------------------------------- */
/* Poison                                                                                       */
/* ------------------------------------------------------------------------------------------- */

RE_INTERNAL void *
Poison_Alloc( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryPoisonDecorator *self  = (ReMemoryPoisonDecorator *) context;
    void                    *block = RE_Memory_Alloc( &self->inner, size, alignment );

    if ( block )
    {
        RE_Memory_Set( block, RE_MEMORY_POISON_ALLOCATED, size );
    }

    return block;
}

RE_INTERNAL void
Poison_Free( void *context, void *block, ReUint64 oldSize )
{
    ReMemoryPoisonDecorator *self = (ReMemoryPoisonDecorator *) context;

    if ( block && oldSize )
    {
        RE_Memory_Set( block, RE_MEMORY_POISON_FREED, oldSize );
    }

    RE_Memory_Free( &self->inner, block, oldSize );
}

/* Realloc is alloc/copy/free rather than a forward, so both fills happen. Costs the inner
 * allocator's in-place resize, which only matters in a build that has decorators installed.
 */
RE_INTERNAL void *
Poison_Realloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    if ( !block )
    {
        return Poison_Alloc( context, newSize, alignment );
    }

    if ( newSize == 0 )
    {
        Poison_Free( context, block, oldSize );

        return 0;
    }

    void *moved = Poison_Alloc( context, newSize, alignment );

    if ( moved )
    {
        RE_Memory_Copy( moved, block, ( newSize < oldSize ) ? newSize : oldSize );
        Poison_Free( context, block, oldSize );
    }

    return moved;
}

RE_INTERNAL ReUint64
Poison_Quantize( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryPoisonDecorator *self = (ReMemoryPoisonDecorator *) context;

    return RE_Memory_Quantize( &self->inner, size, alignment );
}

ReAllocator
RE_MemoryDecorator_Poison( ReAllocator inner, ReMemoryPoisonDecorator *storage )
{
    storage->inner = inner;

    ReAllocator allocator;
    allocator.context                = storage;
    allocator.alloc                  = Poison_Alloc;
    allocator.realloc                = Poison_Realloc;
    allocator.free                   = Poison_Free;
    allocator.quantize               = Poison_Quantize;
    allocator.isInternallyThreadSafe = inner.isInternallyThreadSafe;

    return allocator;
}

/* ------------------------------------------------------------------------------------------- */
/* Leak tracker                                                                                 */
/* ------------------------------------------------------------------------------------------- */

RE_INTERNAL void *
Leak_Alloc( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryLeakDecorator *self  = (ReMemoryLeakDecorator *) context;
    void                  *block = RE_Memory_Alloc( &self->inner, size, alignment );

    if ( !block )
    {
        return 0;
    }

    const char *tag = RE_Memory_CurrentTag();

    RE_SpinLock_Acquire( &self->table.lock );

    ReMemoryAllocationRecord *record = Record_Insert( &self->table, block );

    if ( record )
    {
        record->size   = size;
        record->tag    = tag;
        record->serial = RE_Atomic_FetchAddUint64( &self->nextSerial, 1 );

        Record_CaptureStack( record, self->captureCallstacks );
    }

    RE_SpinLock_Release( &self->table.lock );

    /* Attribution is maintained here rather than in a decorator of its own, because this is
     * already keeping a record per live allocation - a separate one would hash the same data
     * twice to learn the same thing.
     */
    Memory_TagAccount( tag, (ReSint64) size, 1 );

    return block;
}

RE_INTERNAL void
Leak_Free( void *context, void *block, ReUint64 oldSize )
{
    ReMemoryLeakDecorator *self = (ReMemoryLeakDecorator *) context;

    if ( block )
    {
        ReMemoryAllocationRecord removed;
        RE_Memory_Zero( &removed, sizeof( removed ) );

        RE_SpinLock_Acquire( &self->table.lock );
        ReBool found = Record_Remove( &self->table, block, &removed );
        RE_SpinLock_Release( &self->table.lock );

        if ( found )
        {
            Memory_TagAccount( removed.tag, -(ReSint64) removed.size, -1 );
        }
    }

    RE_Memory_Free( &self->inner, block, oldSize );
}

RE_INTERNAL void *
Leak_Realloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    if ( !block )
    {
        return Leak_Alloc( context, newSize, alignment );
    }

    if ( newSize == 0 )
    {
        Leak_Free( context, block, oldSize );

        return 0;
    }

    void *moved = Leak_Alloc( context, newSize, alignment );

    if ( moved )
    {
        RE_Memory_Copy( moved, block, ( newSize < oldSize ) ? newSize : oldSize );
        Leak_Free( context, block, oldSize );
    }

    return moved;
}

RE_INTERNAL ReUint64
Leak_Quantize( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryLeakDecorator *self = (ReMemoryLeakDecorator *) context;

    return RE_Memory_Quantize( &self->inner, size, alignment );
}

ReAllocator
RE_MemoryDecorator_LeakTracker( ReAllocator inner, ReMemoryLeakDecorator *storage, ReUint64 capacity,
    ReBool captureCallstacks )
{
    RE_Memory_Zero( storage, sizeof( *storage ) );

    storage->inner             = inner;
    storage->captureCallstacks = captureCallstacks;

    if ( !Record_TableInit( &storage->table, capacity ) )
    {
        /* Without a table there is nothing to track, so pass the inner allocator straight
         * through rather than silently pretending to track.
         */
        return inner;
    }

    ReAllocator allocator;
    allocator.context                = storage;
    allocator.alloc                  = Leak_Alloc;
    allocator.realloc                = Leak_Realloc;
    allocator.free                   = Leak_Free;
    allocator.quantize               = Leak_Quantize;
    allocator.isInternallyThreadSafe = inner.isInternallyThreadSafe;

    return allocator;
}

ReUint64
RE_MemoryDecorator_ReportLeaks( ReMemoryLeakDecorator *tracker )
{
    if ( !tracker || !tracker->table.records )
    {
        return 0;
    }

    RE_SpinLock_Acquire( &tracker->table.lock );

    ReUint64 outstanding = tracker->table.count;
    ReUint64 leakedBytes = 0;

    if ( outstanding > 0 )
    {
        RE_Memory_Report( "[memory] %llu allocation(s) still outstanding:", (unsigned long long) outstanding );

        for ( ReUint64 i = 0; i < tracker->table.capacity; i += 1 )
        {
            ReMemoryAllocationRecord *record = &tracker->table.records[i];

            if ( record->block == 0 || record->block == RECORD_TOMBSTONE )
            {
                continue;
            }

            leakedBytes += record->size;

            RE_Memory_Report( "  #%llu  %llu bytes at %p  [%s]",
                (unsigned long long) record->serial,
                (unsigned long long) record->size,
                record->block,
                record->tag ? record->tag : "untagged" );

            Record_ReportStack( record );
        }

        RE_Memory_Report( "[memory] %llu bytes leaked in total.", (unsigned long long) leakedBytes );
    }

    RE_SpinLock_Release( &tracker->table.lock );

    return outstanding;
}

/* ------------------------------------------------------------------------------------------- */
/* Double-free finder                                                                           */
/* ------------------------------------------------------------------------------------------- */

RE_INTERNAL void *
DoubleFree_Alloc( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryDoubleFreeDecorator *self  = (ReMemoryDoubleFreeDecorator *) context;
    void                        *block = RE_Memory_Alloc( &self->inner, size, alignment );

    if ( block )
    {
        /* Reusing an address clears its free record - it is legitimately live again, and leaving
         * the old record would make the next genuine free look like a duplicate.
         */
        RE_SpinLock_Acquire( &self->table.lock );
        Record_Remove( &self->table, block, 0 );
        RE_SpinLock_Release( &self->table.lock );
    }

    return block;
}

RE_INTERNAL void
DoubleFree_Free( void *context, void *block, ReUint64 oldSize )
{
    ReMemoryDoubleFreeDecorator *self = (ReMemoryDoubleFreeDecorator *) context;

    if ( !block )
    {
        return;
    }

    RE_SpinLock_Acquire( &self->table.lock );

    ReMemoryAllocationRecord *previous = Record_Find( &self->table, block );

    if ( previous )
    {
        self->detected += 1;

        RE_Memory_Report( "[memory] double free of %p (%llu bytes). First freed here:",
            block, (unsigned long long) previous->size );

        Record_ReportStack( previous );

        RE_SpinLock_Release( &self->table.lock );

        /* Deliberately not forwarded. Freeing twice into the underlying allocator is what
         * corrupts its free list, and swallowing the second free leaves the heap intact so the
         * run can continue to whatever else it was going to find.
         *
         * Not an assert, for two reasons: a hard abort here would stop the run at the first
         * duplicate rather than surfacing all of them, and the counter gives the caller the
         * choice of treating it as fatal at a point of its own choosing.
         */
        return;
    }

    ReMemoryAllocationRecord *record = Record_Insert( &self->table, block );

    if ( record )
    {
        record->size = oldSize;
        Record_CaptureStack( record, self->captureCallstacks );
    }

    RE_SpinLock_Release( &self->table.lock );

    RE_Memory_Free( &self->inner, block, oldSize );
}

RE_INTERNAL void *
DoubleFree_Realloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    if ( !block )
    {
        return DoubleFree_Alloc( context, newSize, alignment );
    }

    if ( newSize == 0 )
    {
        DoubleFree_Free( context, block, oldSize );

        return 0;
    }

    void *moved = DoubleFree_Alloc( context, newSize, alignment );

    if ( moved )
    {
        RE_Memory_Copy( moved, block, ( newSize < oldSize ) ? newSize : oldSize );
        DoubleFree_Free( context, block, oldSize );
    }

    return moved;
}

RE_INTERNAL ReUint64
DoubleFree_Quantize( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryDoubleFreeDecorator *self = (ReMemoryDoubleFreeDecorator *) context;

    return RE_Memory_Quantize( &self->inner, size, alignment );
}

ReAllocator
RE_MemoryDecorator_DoubleFreeFinder( ReAllocator inner, ReMemoryDoubleFreeDecorator *storage,
    ReUint64 capacity, ReBool captureCallstacks )
{
    RE_Memory_Zero( storage, sizeof( *storage ) );

    storage->inner             = inner;
    storage->historyCapacity   = capacity;
    storage->captureCallstacks = captureCallstacks;

    if ( !Record_TableInit( &storage->table, capacity ) )
    {
        return inner;
    }

    ReAllocator allocator;
    allocator.context                = storage;
    allocator.alloc                  = DoubleFree_Alloc;
    allocator.realloc                = DoubleFree_Realloc;
    allocator.free                   = DoubleFree_Free;
    allocator.quantize               = DoubleFree_Quantize;
    allocator.isInternallyThreadSafe = inner.isInternallyThreadSafe;

    return allocator;
}

/* ------------------------------------------------------------------------------------------- */
/* Quarantine                                                                                   */
/* ------------------------------------------------------------------------------------------- */

/* Caller holds the lock. Releases the oldest entry, checking its canary on the way out. */
RE_INTERNAL void
Quarantine_ReleaseOldest( ReMemoryQuarantineDecorator *self )
{
    ReMemoryQuarantineEntry *entry = &self->entries[self->head];

    ReUint8 *bytes = (ReUint8 *) entry->block;

    for ( ReUint64 i = 0; i < entry->size; i += 1 )
    {
        if ( bytes[i] != RE_MEMORY_QUARANTINE_BYTE )
        {
            self->violations += 1;

            RE_Memory_Report( "[memory] use-after-free: %p written at offset %llu while quarantined",
                entry->block, (unsigned long long) i );

            break;
        }
    }

    RE_Memory_Free( &self->inner, entry->block, entry->size );

    self->bytesHeld -= entry->size;
    self->head       = ( self->head + 1 ) % self->capacity;
    self->count     -= 1;
}

RE_INTERNAL void
Quarantine_Free( void *context, void *block, ReUint64 oldSize )
{
    ReMemoryQuarantineDecorator *self = (ReMemoryQuarantineDecorator *) context;

    if ( !block )
    {
        return;
    }

    /* Nothing to verify without a size, so it goes straight through rather than being held with
     * an unknown extent.
     */
    if ( oldSize == 0 )
    {
        RE_Memory_Free( &self->inner, block, oldSize );

        return;
    }

    RE_SpinLock_Acquire( &self->lock );

    while ( self->count == self->capacity || self->bytesHeld + oldSize > self->byteLimit )
    {
        if ( self->count == 0 )
        {
            break;
        }

        Quarantine_ReleaseOldest( self );
    }

    if ( self->count == self->capacity )
    {
        RE_SpinLock_Release( &self->lock );
        RE_Memory_Free( &self->inner, block, oldSize );

        return;
    }

    RE_Memory_Set( block, RE_MEMORY_QUARANTINE_BYTE, oldSize );

    ReUint64 slot = ( self->head + self->count ) % self->capacity;

    self->entries[slot].block = block;
    self->entries[slot].size  = oldSize;
    self->entries[slot].tick  = self->tick;

    self->count     += 1;
    self->bytesHeld += oldSize;

    RE_SpinLock_Release( &self->lock );
}

RE_INTERNAL void *
Quarantine_Alloc( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryQuarantineDecorator *self = (ReMemoryQuarantineDecorator *) context;

    return RE_Memory_Alloc( &self->inner, size, alignment );
}

RE_INTERNAL void *
Quarantine_Realloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    if ( !block )
    {
        return Quarantine_Alloc( context, newSize, alignment );
    }

    if ( newSize == 0 )
    {
        Quarantine_Free( context, block, oldSize );

        return 0;
    }

    void *moved = Quarantine_Alloc( context, newSize, alignment );

    if ( moved )
    {
        RE_Memory_Copy( moved, block, ( newSize < oldSize ) ? newSize : oldSize );
        Quarantine_Free( context, block, oldSize );
    }

    return moved;
}

RE_INTERNAL ReUint64
Quarantine_Quantize( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryQuarantineDecorator *self = (ReMemoryQuarantineDecorator *) context;

    return RE_Memory_Quantize( &self->inner, size, alignment );
}

ReAllocator
RE_MemoryDecorator_Quarantine( ReAllocator inner, ReMemoryQuarantineDecorator *storage,
    ReUint64 capacity, ReUint64 delayTicks, ReUint64 byteLimit )
{
    RE_Memory_Zero( storage, sizeof( *storage ) );

    storage->inner      = inner;
    storage->capacity   = capacity;
    storage->delayTicks = delayTicks;
    storage->byteLimit  = byteLimit;

    storage->entries = (ReMemoryQuarantineEntry *) RE_MemoryMetadata_Alloc(
        capacity * sizeof( ReMemoryQuarantineEntry ), 64 );

    if ( !storage->entries )
    {
        return inner;
    }

    RE_SpinLock_Init( &storage->lock );

    ReAllocator allocator;
    allocator.context                = storage;
    allocator.alloc                  = Quarantine_Alloc;
    allocator.realloc                = Quarantine_Realloc;
    allocator.free                   = Quarantine_Free;
    allocator.quantize               = Quarantine_Quantize;
    allocator.isInternallyThreadSafe = inner.isInternallyThreadSafe;

    return allocator;
}

void
RE_MemoryDecorator_QuarantineTick( ReMemoryQuarantineDecorator *quarantine )
{
    if ( !quarantine || !quarantine->entries )
    {
        return;
    }

    RE_SpinLock_Acquire( &quarantine->lock );

    quarantine->tick += 1;

    while ( quarantine->count > 0 )
    {
        ReMemoryQuarantineEntry *oldest = &quarantine->entries[quarantine->head];

        if ( quarantine->tick - oldest->tick < quarantine->delayTicks )
        {
            break;
        }

        Quarantine_ReleaseOldest( quarantine );
    }

    RE_SpinLock_Release( &quarantine->lock );
}

/* ------------------------------------------------------------------------------------------- */
/* Guard page                                                                                   */
/* ------------------------------------------------------------------------------------------- */

RE_INTERNAL void *
GuardPage_Alloc( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryGuardPageDecorator *self = (ReMemoryGuardPageDecorator *) context;

    if ( size == 0 )
    {
        return 0;
    }

    ReUint64 pageSize  = RE_VirtualMemory_CommitGranularity();
    ReUint64 dataBytes = RE_Memory_AlignUp( size, pageSize );

    ReVirtualRegion region = RE_VirtualMemory_Reserve( dataBytes + pageSize, 0 );
    if ( !region.base )
    {
        return 0;
    }

    /* Only the data pages are committed. The page past them stays reserved and unmapped, so
     * touching it faults immediately rather than corrupting whatever came next.
     */
    if ( !RE_VirtualMemory_Commit( &region, 0, dataBytes ) )
    {
        RE_VirtualMemory_Release( &region );

        return 0;
    }

    /* Placed so its last byte sits against the guard, which is what makes a one-byte overrun
     * fault instead of landing harmlessly in the tail of the final page.
     */
    ReUint8 *block = (ReUint8 *) region.base + ( dataBytes - size );

    if ( alignment > 1 )
    {
        block = (ReUint8 *) ( (ReUint64) block & ~( alignment - 1 ) );
    }

    self->activeAllocations += 1;
    self->bytesReserved     += dataBytes + pageSize;

    return block;
}

RE_INTERNAL void
GuardPage_Free( void *context, void *block, ReUint64 oldSize )
{
    ReMemoryGuardPageDecorator *self = (ReMemoryGuardPageDecorator *) context;

    if ( !block )
    {
        return;
    }

    assert( oldSize > 0 && "guard pages need an accurate oldSize to find the region base" );

    ReUint64 pageSize = RE_VirtualMemory_CommitGranularity();

    /* The region base is recovered by rounding down to the page, since the block was placed
     * within the last data page.
     */
    ReUint64 dataBytes = RE_Memory_AlignUp( oldSize, pageSize );
    ReUint64 blockEnd  = (ReUint64) block + oldSize;
    ReUint64 base      = RE_Memory_AlignUp( blockEnd, pageSize ) - dataBytes;

    ReVirtualRegion region;
    region.base = (void *) base;
    region.size = dataBytes + pageSize;

    RE_VirtualMemory_Release( &region );

    self->activeAllocations -= 1;
    self->bytesReserved     -= dataBytes + pageSize;
}

RE_INTERNAL void *
GuardPage_Realloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    if ( !block )
    {
        return GuardPage_Alloc( context, newSize, alignment );
    }

    if ( newSize == 0 )
    {
        GuardPage_Free( context, block, oldSize );

        return 0;
    }

    void *moved = GuardPage_Alloc( context, newSize, alignment );

    if ( moved )
    {
        RE_Memory_Copy( moved, block, ( newSize < oldSize ) ? newSize : oldSize );
        GuardPage_Free( context, block, oldSize );
    }

    return moved;
}

RE_INTERNAL ReUint64
GuardPage_Quantize( void *context, ReUint64 size, ReUint64 alignment )
{
    (void) context;
    (void) alignment;

    /* Exactly what was asked for. Claiming the rest of the page would put the extra bytes on the
     * wrong side of the block, away from the guard, defeating the point.
     */
    return size;
}

ReAllocator
RE_MemoryDecorator_GuardPage( ReAllocator inner, ReMemoryGuardPageDecorator *storage )
{
    RE_Memory_Zero( storage, sizeof( *storage ) );

    storage->inner = inner;

    ReAllocator allocator;
    allocator.context                = storage;
    allocator.alloc                  = GuardPage_Alloc;
    allocator.realloc                = GuardPage_Realloc;
    allocator.free                   = GuardPage_Free;
    allocator.quantize               = GuardPage_Quantize;
    allocator.isInternallyThreadSafe = RE_True; /* goes straight to the OS, which is */

    return allocator;
}

/* ------------------------------------------------------------------------------------------- */
/* Slack verifier                                                                               */
/* ------------------------------------------------------------------------------------------- */

RE_INTERNAL void *
Verifier_Alloc( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryVerifierDecorator *self  = (ReMemoryVerifierDecorator *) context;
    void                      *block = RE_Memory_Alloc( &self->inner, size, alignment );

    if ( block )
    {
        ReUint64 given = RE_Memory_Quantize( &self->inner, size, alignment );

        if ( given > size )
        {
            RE_Memory_Set( (ReUint8 *) block + size, RE_MEMORY_SLACK_CANARY, given - size );
        }
    }

    return block;
}

RE_INTERNAL void
Verifier_Free( void *context, void *block, ReUint64 oldSize )
{
    ReMemoryVerifierDecorator *self = (ReMemoryVerifierDecorator *) context;

    if ( block && oldSize )
    {
        ReUint64 given = RE_Memory_Quantize( &self->inner, oldSize, RE_MEMORY_DEFAULT_ALIGNMENT );

        RE_Atomic_FetchAddUint64( &self->checked, 1 );

        ReUint8 *slack = (ReUint8 *) block + oldSize;

        for ( ReUint64 i = 0; given > oldSize && i < given - oldSize; i += 1 )
        {
            if ( slack[i] != RE_MEMORY_SLACK_CANARY )
            {
                RE_Atomic_FetchAddUint64( &self->violations, 1 );

                RE_Memory_Report( "[memory] overrun: %p wrote %llu byte(s) past its %llu-byte request",
                    block, (unsigned long long) ( i + 1 ), (unsigned long long) oldSize );

                break;
            }
        }
    }

    RE_Memory_Free( &self->inner, block, oldSize );
}

RE_INTERNAL void *
Verifier_Realloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    if ( !block )
    {
        return Verifier_Alloc( context, newSize, alignment );
    }

    if ( newSize == 0 )
    {
        Verifier_Free( context, block, oldSize );

        return 0;
    }

    void *moved = Verifier_Alloc( context, newSize, alignment );

    if ( moved )
    {
        RE_Memory_Copy( moved, block, ( newSize < oldSize ) ? newSize : oldSize );
        Verifier_Free( context, block, oldSize );
    }

    return moved;
}

RE_INTERNAL ReUint64
Verifier_Quantize( void *context, ReUint64 size, ReUint64 alignment )
{
    (void) context;
    (void) alignment;

    /* The slack is where the canary lives, so it cannot be offered to the caller as usable. */
    return size;
}

ReAllocator
RE_MemoryDecorator_Verifier( ReAllocator inner, ReMemoryVerifierDecorator *storage )
{
    RE_Memory_Zero( storage, sizeof( *storage ) );

    storage->inner = inner;

    ReAllocator allocator;
    allocator.context                = storage;
    allocator.alloc                  = Verifier_Alloc;
    allocator.realloc                = Verifier_Realloc;
    allocator.free                   = Verifier_Free;
    allocator.quantize               = Verifier_Quantize;
    allocator.isInternallyThreadSafe = inner.isInternallyThreadSafe;

    return allocator;
}

/* ------------------------------------------------------------------------------------------- */
/* Trace                                                                                        */
/* ------------------------------------------------------------------------------------------- */

RE_INTERNAL void *
Trace_Alloc( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryTraceDecorator *self  = (ReMemoryTraceDecorator *) context;
    void                   *block = RE_Memory_Alloc( &self->inner, size, alignment );

    RE_Atomic_FetchAddUint64( &self->events, 1 );
    RE_Memory_Report( "[memory] alloc %p %llu align %llu [%s]", block, (unsigned long long) size,
        (unsigned long long) alignment, RE_Memory_CurrentTag() ? RE_Memory_CurrentTag() : "untagged" );

    return block;
}

RE_INTERNAL void
Trace_Free( void *context, void *block, ReUint64 oldSize )
{
    ReMemoryTraceDecorator *self = (ReMemoryTraceDecorator *) context;

    RE_Atomic_FetchAddUint64( &self->events, 1 );
    RE_Memory_Report( "[memory] free %p %llu", block, (unsigned long long) oldSize );

    RE_Memory_Free( &self->inner, block, oldSize );
}

RE_INTERNAL void *
Trace_Realloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    ReMemoryTraceDecorator *self = (ReMemoryTraceDecorator *) context;

    void *moved = RE_Memory_Realloc( &self->inner, block, oldSize, newSize, alignment );

    RE_Atomic_FetchAddUint64( &self->events, 1 );
    RE_Memory_Report( "[memory] realloc %p %llu -> %p %llu", block, (unsigned long long) oldSize,
        moved, (unsigned long long) newSize );

    return moved;
}

RE_INTERNAL ReUint64
Trace_Quantize( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryTraceDecorator *self = (ReMemoryTraceDecorator *) context;

    return RE_Memory_Quantize( &self->inner, size, alignment );
}

ReAllocator
RE_MemoryDecorator_Trace( ReAllocator inner, ReMemoryTraceDecorator *storage )
{
    RE_Memory_Zero( storage, sizeof( *storage ) );

    storage->inner = inner;

    ReAllocator allocator;
    allocator.context                = storage;
    allocator.alloc                  = Trace_Alloc;
    allocator.realloc                = Trace_Realloc;
    allocator.free                   = Trace_Free;
    allocator.quantize               = Trace_Quantize;
    allocator.isInternallyThreadSafe = inner.isInternallyThreadSafe;

    return allocator;
}

/* ------------------------------------------------------------------------------------------- */
/* Locking proxy                                                                                */
/* ------------------------------------------------------------------------------------------- */

RE_INTERNAL void *
Locking_Alloc( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryLockingDecorator *self = (ReMemoryLockingDecorator *) context;

    RE_SpinLock_Acquire( &self->lock );
    void *block = RE_Memory_Alloc( &self->inner, size, alignment );
    RE_SpinLock_Release( &self->lock );

    return block;
}

RE_INTERNAL void
Locking_Free( void *context, void *block, ReUint64 oldSize )
{
    ReMemoryLockingDecorator *self = (ReMemoryLockingDecorator *) context;

    RE_SpinLock_Acquire( &self->lock );
    RE_Memory_Free( &self->inner, block, oldSize );
    RE_SpinLock_Release( &self->lock );
}

RE_INTERNAL void *
Locking_Realloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    ReMemoryLockingDecorator *self = (ReMemoryLockingDecorator *) context;

    RE_SpinLock_Acquire( &self->lock );
    void *moved = RE_Memory_Realloc( &self->inner, block, oldSize, newSize, alignment );
    RE_SpinLock_Release( &self->lock );

    return moved;
}

RE_INTERNAL ReUint64
Locking_Quantize( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryLockingDecorator *self = (ReMemoryLockingDecorator *) context;

    return RE_Memory_Quantize( &self->inner, size, alignment );
}

ReAllocator
RE_MemoryDecorator_Locking( ReAllocator inner, ReMemoryLockingDecorator *storage )
{
    RE_Memory_Zero( storage, sizeof( *storage ) );

    storage->inner = inner;
    RE_SpinLock_Init( &storage->lock );

    ReAllocator allocator;
    allocator.context                = storage;
    allocator.alloc                  = Locking_Alloc;
    allocator.realloc                = Locking_Realloc;
    allocator.free                   = Locking_Free;
    allocator.quantize               = Locking_Quantize;
    allocator.isInternallyThreadSafe = RE_True;

    return allocator;
}

/* ------------------------------------------------------------------------------------------- */
/* Out-of-memory handler                                                                        */
/* ------------------------------------------------------------------------------------------- */

RE_INTERNAL void *
OutOfMemory_Alloc( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryOutOfMemoryDecorator *self  = (ReMemoryOutOfMemoryDecorator *) context;
    void                         *block = RE_Memory_Alloc( &self->inner, size, alignment );

    if ( !block && size > 0 )
    {
        self->failures += 1;

        /* Hand the reserve back before reporting. Reporting allocates - formatting, symbol
         * lookup, whatever the report sink does - and out of memory is exactly when that would
         * otherwise fail too, leaving no diagnosis at all.
         */
        if ( self->reserve )
        {
            ReVirtualRegion region;
            region.base = self->reserve;
            region.size = self->reserveSize;

            RE_VirtualMemory_Release( &region );

            self->reserve = 0;
        }

        RE_Memory_Report( "[memory] out of memory: %llu byte request failed (alignment %llu)",
            (unsigned long long) size, (unsigned long long) alignment );
    }

    return block;
}

RE_INTERNAL void
OutOfMemory_Free( void *context, void *block, ReUint64 oldSize )
{
    ReMemoryOutOfMemoryDecorator *self = (ReMemoryOutOfMemoryDecorator *) context;

    RE_Memory_Free( &self->inner, block, oldSize );
}

RE_INTERNAL void *
OutOfMemory_Realloc( void *context, void *block, ReUint64 oldSize, ReUint64 newSize, ReUint64 alignment )
{
    ReMemoryOutOfMemoryDecorator *self = (ReMemoryOutOfMemoryDecorator *) context;

    return RE_Memory_Realloc( &self->inner, block, oldSize, newSize, alignment );
}

RE_INTERNAL ReUint64
OutOfMemory_Quantize( void *context, ReUint64 size, ReUint64 alignment )
{
    ReMemoryOutOfMemoryDecorator *self = (ReMemoryOutOfMemoryDecorator *) context;

    return RE_Memory_Quantize( &self->inner, size, alignment );
}

ReAllocator
RE_MemoryDecorator_OutOfMemory( ReAllocator inner, ReMemoryOutOfMemoryDecorator *storage,
    ReUint64 reserveSize )
{
    RE_Memory_Zero( storage, sizeof( *storage ) );

    storage->inner       = inner;
    storage->reserveSize = reserveSize;

    if ( reserveSize > 0 )
    {
        ReVirtualRegion region = RE_VirtualMemory_Reserve( reserveSize, 0 );

        if ( region.base && RE_VirtualMemory_Commit( &region, 0, region.size ) )
        {
            storage->reserve     = region.base;
            storage->reserveSize = region.size;
        }
        else
        {
            RE_VirtualMemory_Release( &region );
        }
    }

    ReAllocator allocator;
    allocator.context                = storage;
    allocator.alloc                  = OutOfMemory_Alloc;
    allocator.realloc                = OutOfMemory_Realloc;
    allocator.free                   = OutOfMemory_Free;
    allocator.quantize               = OutOfMemory_Quantize;
    allocator.isInternallyThreadSafe = inner.isInternallyThreadSafe;

    return allocator;
}

/* ------------------------------------------------------------------------------------------- */
/* Chain assembly                                                                               */
/* ------------------------------------------------------------------------------------------- */

ReMemoryDecoratorConfig
RE_Memory_DefaultDecoratorConfig( void )
{
    ReMemoryDecoratorConfig config;
    RE_Memory_Zero( &config, sizeof( config ) );

    /* Every entry is committed up front and carries an inline callstack, so this is roughly
     * 170 bytes per entry per tracking decorator. At 65536 that was 30 MB of metadata to watch a
     * few hundred KiB of allocations - which the stats report made obvious the first time it ran.
     * Raise it deliberately, and check RE_MemorySystem_ReportStats afterwards.
     */
    config.recordCapacity     = 1u << 14;
    config.quarantineDelay    = 4;
    config.quarantineBytes    = 16 * 1024 * 1024;
    config.outOfMemoryReserve = 1 * 1024 * 1024;

#if RE_BUILD == RE_BUILD_DEBUG
    config.poison            = RE_True;
    config.trackLeaks        = RE_True;
    config.findDoubleFree    = RE_True;
    config.captureCallstacks = RE_True;
#elif RE_BUILD == RE_BUILD_DEVELOPMENT
    config.poison     = RE_True;
    config.trackLeaks = RE_True;
#endif

    /* Guard pages, quarantine, slack verification and tracing stay off by default even in debug.
     * Each is expensive enough to change what the program does - guard pages alone cost two pages
     * per allocation - so they are switched on to chase a specific problem, not left running.
     */

    return config;
}

ReAllocator
RE_Memory_BuildDecoratorChain( ReAllocator base, const ReMemoryDecoratorConfig *config,
    ReMemoryDecoratorChain *storage )
{
    assert( config && storage );

    RE_Memory_Zero( storage, sizeof( *storage ) );

    ReAllocator current = base;

    /*
        Ordered so that the decorators closest to the caller see the caller's request, and those
        closest to the real allocator see what actually happened. Guard pages replace the
        underlying allocator entirely rather than forwarding, so they go on first.
    */
    if ( config->guardPages )
    {
        current = RE_MemoryDecorator_GuardPage( current, &storage->guardPage );
    }

    current = RE_MemoryDecorator_OutOfMemory( current, &storage->outOfMemory, config->outOfMemoryReserve );

    /* The one query that matters here. A well-built binned allocator does its own fine-grained
     * locking, and wrapping it would serialise every allocation in the engine.
     */
    if ( !current.isInternallyThreadSafe )
    {
        current = RE_MemoryDecorator_Locking( current, &storage->locking );
    }

    if ( config->verifySlack )
    {
        current = RE_MemoryDecorator_Verifier( current, &storage->verifier );
    }

    if ( config->quarantine )
    {
        current = RE_MemoryDecorator_Quarantine( current, &storage->quarantine, config->recordCapacity,
            config->quarantineDelay, config->quarantineBytes );

        storage->hasQuarantine = RE_True;
    }

    if ( config->poison )
    {
        current = RE_MemoryDecorator_Poison( current, &storage->poison );
    }

    if ( config->findDoubleFree )
    {
        current = RE_MemoryDecorator_DoubleFreeFinder( current, &storage->doubleFree,
            config->recordCapacity, config->captureCallstacks );
    }

    if ( config->trackLeaks )
    {
        current = RE_MemoryDecorator_LeakTracker( current, &storage->leaks, config->recordCapacity,
            config->captureCallstacks );

        storage->hasLeakTracker = RE_True;
    }

    if ( config->trace )
    {
        current = RE_MemoryDecorator_Trace( current, &storage->trace );
    }

    storage->result = current;

    return current;
}

void
RE_Memory_DecoratorTick( ReMemoryDecoratorChain *chain )
{
    if ( chain && chain->hasQuarantine )
    {
        RE_MemoryDecorator_QuarantineTick( &chain->quarantine );
    }
}

ReUint64
RE_Memory_ReportDecoratorFindings( ReMemoryDecoratorChain *chain )
{
    if ( !chain )
    {
        return 0;
    }

    ReUint64 problems = 0;

    if ( chain->hasLeakTracker )
    {
        problems += RE_MemoryDecorator_ReportLeaks( &chain->leaks );
    }

    problems += chain->doubleFree.detected;
    problems += chain->quarantine.violations;
    problems += RE_Atomic_LoadUint64( &chain->verifier.violations );

    if ( chain->outOfMemory.failures > 0 )
    {
        RE_Memory_Report( "[memory] %llu allocation failure(s) during this run.",
            (unsigned long long) chain->outOfMemory.failures );

        problems += chain->outOfMemory.failures;
    }

    return problems;
}
