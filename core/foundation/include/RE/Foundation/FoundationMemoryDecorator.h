/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationAtomic.h>
#include <RE/Foundation/FoundationDebug.h>
#include <RE/Foundation/FoundationMemoryAllocator.h>
#include <RE/Foundation/FoundationSpinLock.h>

/*
    FoundationMemoryDecorator.h

    The global allocator is built as a chain of decorators over the real one, assembled at startup
    from build flags and command-line switches. Each implements ReAllocator and forwards to the
    next, so a call site never knows or cares which of them are installed.

    Heap corruption and use-after-free are among the most expensive bugs in a large C codebase.
    Owning the allocator is what makes it possible to swap in poisoning, guard pages, or leak
    tracking without touching a single call site - and to compile all of it out for shipping.

    Every decorator here takes its storage from the caller rather than allocating it. These sit
    below the general allocator by definition, so anything they needed to allocate would recurse.

    @threadsafe Each decorator is as thread-safe as what it wraps, plus its own internal locking
                where it keeps state.
*/

/* Where reports go. Foundation cannot depend on the log service - the log service depends on
 * foundation - so the destination is installed by whoever is above both.
 *
 * Null by default, which silently discards. Wire it up early; a leak report nobody sees is worse
 * than no leak tracking at all, because it looks like it is working.
 */
typedef void ( *ReMemoryReportFn )( const char *message );

void RE_Memory_SetReportFn( ReMemoryReportFn report );
void RE_Memory_Report( const char *format, ... );

/* ------------------------------------------------------------------------------------------- */
/* Attribution                                                                                  */
/* ------------------------------------------------------------------------------------------- */

/*
    Scoped tags, so "we are 400 MB over" can become "the streaming pool is 400 MB over".

    Tags are string literals compared by pointer, not by content - registration is a pointer
    write, and lookup is a pointer compare. Pass literals, not constructed strings.

    C has no scope guards, so the begin and end are explicit and must be paired. Both compile to
    nothing in shipping.
*/
#define RE_MEMORY_MAX_TAGS       64
#define RE_MEMORY_TAG_STACK_DEPTH 16

#if RE_BUILD < RE_BUILD_SHIPPING
#define RE_MEMORY_SCOPE_BEGIN( tagLiteral ) RE_Memory_PushTag( tagLiteral )
#define RE_MEMORY_SCOPE_END()               RE_Memory_PopTag()
#else
#define RE_MEMORY_SCOPE_BEGIN( tagLiteral ) ( (void) 0 )
#define RE_MEMORY_SCOPE_END()               ( (void) 0 )
#endif

void        RE_Memory_PushTag( const char *tag );
void        RE_Memory_PopTag( void );
const char *RE_Memory_CurrentTag( void );

typedef struct ReMemoryTagUsage
{
    const char *tag;
    ReUint64    bytesInUse;
    ReUint64    allocationsInUse;
    ReUint64    peakBytes;
} ReMemoryTagUsage;

/* Copies up to maxTags entries and returns how many exist. */
ReUint32 RE_Memory_GetTagUsage( ReMemoryTagUsage *outTags, ReUint32 maxTags );

/* ------------------------------------------------------------------------------------------- */
/* Decorators                                                                                   */
/* ------------------------------------------------------------------------------------------- */

/* Shared record of one live allocation, used by the tracking decorators. */
typedef struct ReMemoryAllocationRecord
{
    void       *block;
    ReUint64    size;
    ReUint64    serial;
    const char *tag;
    ReUint32    frameCount;
    void       *frames[RE_CALLSTACK_MAX_FRAMES];
} ReMemoryAllocationRecord;

typedef struct ReMemoryRecordTable
{
    ReMemoryAllocationRecord *records; /* open addressing; block == 0 means empty */
    ReUint64                  capacity;
    ReUint64                  count;
    ReSpinLock                lock;
} ReMemoryRecordTable;

/*
    Fills fresh memory with one sentinel and freed memory with another, so reading uninitialised
    memory or memory that has already been freed produces obvious, greppable garbage rather than
    something that looks plausible.

    The values are chosen to be conspicuous in both interpretations: as a float they read as an
    enormous NaN-ish value, and as a pointer they are far outside any mapping.
*/
#define RE_MEMORY_POISON_ALLOCATED 0xCD
#define RE_MEMORY_POISON_FREED     0xDD

typedef struct ReMemoryPoisonDecorator
{
    ReAllocator inner;
} ReMemoryPoisonDecorator;

ReAllocator RE_MemoryDecorator_Poison( ReAllocator inner, ReMemoryPoisonDecorator *storage );

/* Records a callstack per live allocation and reports whatever is still outstanding on demand. */
typedef struct ReMemoryLeakDecorator
{
    ReAllocator         inner;
    ReMemoryRecordTable table;
    ReAtomicUint64      nextSerial;
    ReBool              captureCallstacks;
} ReMemoryLeakDecorator;

ReAllocator RE_MemoryDecorator_LeakTracker( ReAllocator inner, ReMemoryLeakDecorator *storage,
    ReUint64 capacity, ReBool captureCallstacks );

/* Reports outstanding allocations. Returns how many there were, so a caller can treat a non-zero
 * result as a failure without parsing the report.
 */
ReUint64 RE_MemoryDecorator_ReportLeaks( ReMemoryLeakDecorator *tracker );

/*
    Remembers recent frees, so freeing the same pointer twice names both sites rather than
    corrupting the free list and failing somewhere unrelated later.
*/
typedef struct ReMemoryDoubleFreeDecorator
{
    ReAllocator         inner;
    ReMemoryRecordTable table;
    ReUint64            historyCapacity;
    ReUint64            detected;
    ReBool              captureCallstacks;
} ReMemoryDoubleFreeDecorator;

ReAllocator RE_MemoryDecorator_DoubleFreeFinder( ReAllocator inner, ReMemoryDoubleFreeDecorator *storage,
    ReUint64 capacity, ReBool captureCallstacks );

/*
    Delays reuse of freed memory by a number of ticks, filling it with a canary and verifying that
    canary before releasing it.

    Catches the use-after-free that poisoning alone misses: poisoning only helps once the memory
    has actually been recycled, and until then a stale pointer reads perfectly plausible data.
*/
#define RE_MEMORY_QUARANTINE_BYTE 0xFB

typedef struct ReMemoryQuarantineEntry
{
    void    *block;
    ReUint64 size;
    ReUint64 tick;
} ReMemoryQuarantineEntry;

typedef struct ReMemoryQuarantineDecorator
{
    ReAllocator              inner;
    ReMemoryQuarantineEntry *entries;
    ReUint64                 capacity;
    ReUint64                 head;
    ReUint64                 count;
    ReUint64                 bytesHeld;
    ReUint64                 byteLimit;
    ReUint64                 delayTicks;
    ReUint64                 tick;
    ReUint64                 violations;
    ReSpinLock               lock;
} ReMemoryQuarantineDecorator;

ReAllocator RE_MemoryDecorator_Quarantine( ReAllocator inner, ReMemoryQuarantineDecorator *storage,
    ReUint64 capacity, ReUint64 delayTicks, ReUint64 byteLimit );

/* Advances the quarantine clock and releases anything that has served its delay. Call once a
 * frame. Also flushes early when the byte limit is exceeded, which a level load will do.
 */
void RE_MemoryDecorator_QuarantineTick( ReMemoryQuarantineDecorator *quarantine );

/*
    One or more whole pages per allocation, followed by an unmapped guard page, with the block
    placed so its last byte sits against the guard. An overrun faults on the instruction that
    caused it rather than corrupting a neighbour and failing somewhere else entirely.

    Enormously expensive in memory - a 16-byte allocation costs two pages - and unbeatable for
    pinning down corruption that nothing else will localise.

    @warning Depends on the caller passing an accurate oldSize to free, since that is what
             recovers the base of the region.
*/
typedef struct ReMemoryGuardPageDecorator
{
    ReAllocator inner;
    ReUint64    activeAllocations;
    ReUint64    bytesReserved;
} ReMemoryGuardPageDecorator;

ReAllocator RE_MemoryDecorator_GuardPage( ReAllocator inner, ReMemoryGuardPageDecorator *storage );

/*
    Writes a canary into the slack between what was requested and what the allocator actually
    handed over, and checks it on free.

    Cheaper than guard pages and catches the common small overrun, because a size class nearly
    always leaves a few bytes of slack past the request.
*/
#define RE_MEMORY_SLACK_CANARY 0xA5

typedef struct ReMemoryVerifierDecorator
{
    ReAllocator    inner;
    ReAtomicUint64 checked;
    ReAtomicUint64 violations;
} ReMemoryVerifierDecorator;

ReAllocator RE_MemoryDecorator_Verifier( ReAllocator inner, ReMemoryVerifierDecorator *storage );

/* Streams every event to the report function, for timeline analysis in an external profiler. */
typedef struct ReMemoryTraceDecorator
{
    ReAllocator    inner;
    ReAtomicUint64 events;
} ReMemoryTraceDecorator;

ReAllocator RE_MemoryDecorator_Trace( ReAllocator inner, ReMemoryTraceDecorator *storage );

/*
    Serialises every call.

    Only ever installed over an allocator that says it is not internally thread-safe. Wrapping one
    that is - the binned heap, for instance - would collapse its per-class locking down to a
    single global lock and undo the entire point of it.
*/
typedef struct ReMemoryLockingDecorator
{
    ReAllocator inner;
    ReSpinLock  lock;
} ReMemoryLockingDecorator;

ReAllocator RE_MemoryDecorator_Locking( ReAllocator inner, ReMemoryLockingDecorator *storage );

/*
    Holds a block of memory in reserve and releases it when an allocation first fails, so that
    reporting the failure does not itself fail for want of memory.

    Out of memory is where diagnostics are needed most and are least likely to work.
*/
typedef struct ReMemoryOutOfMemoryDecorator
{
    ReAllocator inner;
    void       *reserve;
    ReUint64    reserveSize;
    ReUint64    failures;
} ReMemoryOutOfMemoryDecorator;

ReAllocator RE_MemoryDecorator_OutOfMemory( ReAllocator inner, ReMemoryOutOfMemoryDecorator *storage,
    ReUint64 reserveSize );

/* ------------------------------------------------------------------------------------------- */
/* Chain assembly                                                                               */
/* ------------------------------------------------------------------------------------------- */

typedef struct ReMemoryDecoratorConfig
{
    ReBool poison;
    ReBool trackLeaks;
    ReBool findDoubleFree;
    ReBool guardPages;
    ReBool quarantine;
    ReBool verifySlack;
    ReBool trace;
    ReBool captureCallstacks;

    ReUint64 recordCapacity;    /* live allocations the trackers can hold */
    ReUint64 quarantineDelay;   /* ticks before freed memory is really released */
    ReUint64 quarantineBytes;   /* cap, so a level load cannot grow it without bound */
    ReUint64 outOfMemoryReserve;
} ReMemoryDecoratorConfig;

/* Sensible defaults for the current build level: everything off in shipping, poisoning and leak
 * tracking in development, and those plus double-free detection in debug.
 */
ReMemoryDecoratorConfig RE_Memory_DefaultDecoratorConfig( void );

/* Storage for the whole chain, owned by the caller. */
typedef struct ReMemoryDecoratorChain
{
    ReAllocator result;

    ReMemoryOutOfMemoryDecorator outOfMemory;
    ReMemoryTraceDecorator       trace;
    ReMemoryGuardPageDecorator   guardPage;
    ReMemoryLockingDecorator     locking;
    ReMemoryVerifierDecorator    verifier;
    ReMemoryLeakDecorator        leaks;
    ReMemoryPoisonDecorator      poison;
    ReMemoryDoubleFreeDecorator  doubleFree;
    ReMemoryQuarantineDecorator  quarantine;

    ReBool hasLeakTracker;
    ReBool hasQuarantine;
} ReMemoryDecoratorChain;

/* Assembles the chain over base and returns the outermost allocator.
 *
 * The locking proxy is installed only when base reports that it is not internally thread-safe.
 */
ReAllocator RE_Memory_BuildDecoratorChain( ReAllocator base, const ReMemoryDecoratorConfig *config,
    ReMemoryDecoratorChain *storage );

/* Per-frame upkeep for whatever in the chain needs it. */
void RE_Memory_DecoratorTick( ReMemoryDecoratorChain *chain );

/* Reports anything outstanding. Returns the number of problems found, so a caller can fail a
 * shutdown check on it.
 */
ReUint64 RE_Memory_ReportDecoratorFindings( ReMemoryDecoratorChain *chain );
