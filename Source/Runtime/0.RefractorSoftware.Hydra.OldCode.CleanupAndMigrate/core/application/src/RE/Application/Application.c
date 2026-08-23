/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Application/Application.h>

#include <RE/Foundation/FoundationContext.h>
#include <RE/Foundation/FoundationMemorySystem.h>
#include <RE/Log/Log.h>

typedef struct ReAppState
{
    ReUint64 tickCount;
} ReAppState;

/* Engine-owned, allocated from the engine's own heap rather than carved out of a block the kernel
 * handed over. One instance per process, which is why a file-scope pointer rather than something
 * the kernel has to carry for us.
 */
RE_GLOBAL ReAppState *gAppState;

/* Foundation cannot depend on the log service - the log service depends on foundation - so the
 * memory system reports through a function pointer and this is where it gets pointed.
 */
RE_INTERNAL void
Application_ReportMemory( const char *message )
{
    RE_LOG_INFO( "%s", message );
}

ReBool
RE_Application_Init( ReAppContext *context )
{
    RE_Memory_SetReportFn( Application_ReportMemory );

    ReMemorySystemConfig memoryConfig = RE_MemorySystem_DefaultConfig();

    if ( !RE_MemorySystem_Init( &memoryConfig ) )
    {
        RE_LOG_ERROR( "RE_Application_Init: memory system failed to initialise" );

        return RE_False;
    }

    RE_MEMORY_SCOPE_BEGIN( "Application" );

    gAppState = RE_MEMORY_ALLOC_TYPE( RE_Context_Allocator(), ReAppState );

    RE_MEMORY_SCOPE_END();

    if ( !gAppState )
    {
        RE_LOG_ERROR( "RE_Application_Init: could not allocate engine state" );

        return RE_False;
    }

    gAppState->tickCount = 0;

    RE_LOG_INFO( "RE_Application_Init: engine started" );

    for ( ReSint32 i = 0; i < context->argCount; i += 1 )
    {
        ReStringView arg = context->args[i];
        RE_LOG_INFO( "arg[%d]: %.*s", i, (int) arg.length, (const char *) arg.data );
    }

    return RE_True;
}

void
RE_Application_Tick( ReAppContext *context, ReFloat32 deltaTime )
{
    gAppState->tickCount += 1;

    /* Rotates the frame allocator onto this frame's buffer and does the decorators' per-frame
     * upkeep. Has to happen before anything allocates frame memory this tick.
     */
    RE_MemorySystem_BeginFrame( gAppState->tickCount );

    (void) deltaTime;
    (void) context->input;
}

void
RE_Application_Shutdown( ReAppContext *context )
{
    (void) context;

    if ( gAppState )
    {
        RE_MEMORY_FREE_TYPE( RE_Context_Allocator(), gAppState, ReAppState );
        gAppState = 0;
    }

    RE_MemorySystem_ReportStats();

    /* Anything still outstanding is reported here, by name and with a callstack where one was
     * captured. A non-zero count is a leak, not a statistic.
     */
    ReUint64 problems = RE_MemorySystem_Shutdown();

    if ( problems > 0 )
    {
        RE_LOG_ERROR( "RE_Application_Shutdown: %llu memory problem(s) reported", (unsigned long long) problems );
    }
    else
    {
        RE_LOG_INFO( "RE_Application_Shutdown: memory clean" );
    }
}
