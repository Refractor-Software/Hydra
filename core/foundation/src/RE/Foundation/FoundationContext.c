/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationContext.h>

#include <assert.h>

#include <RE/Foundation/FoundationMemoryFrame.h>
#include <RE/Foundation/FoundationMemorySystem.h>
#include <RE/Foundation/FoundationMemoryThreadCache.h>
#include <RE/Foundation/FoundationMemoryUtility.h>
#include <RE/Foundation/FoundationThread.h>

RE_GLOBAL RE_THREAD_LOCAL ReContext gContext;

RE_INTERNAL void
Context_OnThreadExit( void *userData )
{
    (void) userData;

    RE_Context_ThreadShutdown();
}

ReBool
RE_Context_ThreadInit( ReUint32 threadIndex )
{
    if ( gContext.initialized )
    {
        return RE_True;
    }

    assert( RE_MemorySystem_IsInitialized() && "RE_Context_ThreadInit before RE_MemorySystem_Init" );

    if ( !RE_Memory_ThreadInit() )
    {
        return RE_False;
    }

    gContext.allocator   = RE_MemorySystem_GlobalAllocator();
    gContext.threadIndex = threadIndex;
    gContext.initialized = RE_True;

    /* Registered so a thread that forgets to shut down still releases its scratch arenas, which
     * are a reservation each and would otherwise accumulate for the life of the process.
     */
    RE_Thread_RegisterExitCallback( Context_OnThreadExit, &gContext );

    return RE_True;
}

void
RE_Context_ThreadShutdown( void )
{
    if ( !gContext.initialized )
    {
        return;
    }

    RE_Scratch_ThreadShutdown();
    RE_Memory_ThreadShutdown();

    RE_Memory_Zero( &gContext, sizeof( gContext ) );
}

ReContext *
RE_Context_Get( void )
{
    assert( gContext.initialized && "this thread never called RE_Context_ThreadInit" );

    return &gContext;
}

ReAllocator *
RE_Context_Allocator( void )
{
    return &RE_Context_Get()->allocator;
}

ReArena *
RE_Context_FrameArena( void )
{
    ReFrameAllocator *frames = RE_MemorySystem_FrameAllocator();

    if ( !frames )
    {
        return 0;
    }

    return RE_Frame_Arena( frames, RE_Context_Get()->threadIndex );
}

void *
RE_Context_FrameAlloc( ReUint64 size, ReUint64 alignment )
{
    ReFrameAllocator *frames = RE_MemorySystem_FrameAllocator();

    if ( !frames )
    {
        return 0;
    }

    return RE_Frame_Alloc( frames, RE_Context_Get()->threadIndex, size, alignment );
}
