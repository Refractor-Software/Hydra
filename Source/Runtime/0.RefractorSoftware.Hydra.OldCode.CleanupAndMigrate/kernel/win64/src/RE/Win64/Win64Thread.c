/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64Thread.h"

#include <RE/Foundation/FoundationSpinLock.h>
#include <RE/Foundation/FoundationThread.h>

/* Windows side of the RE_Thread_* boundary declared in FoundationThread.h. */

void
RE_Thread_Yield( void )
{
    /* SwitchToThread only yields to a thread on the *same* processor and reports whether it found
     * one. Sleep(0) is the documented follow-up when it did not: together they cover both the
     * "another thread is ready here" and "everything on this core is blocked" cases.
     */
    if ( !SwitchToThread() )
    {
        Sleep( 0 );
    }
}

ReUint64
RE_Thread_CurrentId( void )
{
    return (ReUint64) GetCurrentThreadId();
}

/*
    Thread-exit notification, via fiber-local storage.

    FLS rather than TLS because only FLS takes a destructor callback. Windows runs it as the
    thread exits, for every thread that ever set a value - including threads the engine did not
    create, which is exactly the case an explicit shutdown call would miss.

    One index for the whole process, allocated on first use. Only one subsystem needs this, so
    the callback is a single global rather than a table.
*/

RE_GLOBAL DWORD          gWin64ThreadExitFlsIndex = FLS_OUT_OF_INDEXES;
RE_GLOBAL ReThreadExitFn gWin64ThreadExitCallback;
RE_GLOBAL ReSpinLock     gWin64ThreadExitLock;

RE_INTERNAL void WINAPI
Win64_Thread_OnFlsDestroy( PVOID userData )
{
    if ( gWin64ThreadExitCallback && userData )
    {
        gWin64ThreadExitCallback( userData );
    }
}

ReBool
RE_Thread_RegisterExitCallback( ReThreadExitFn callback, void *userData )
{
    if ( !callback || !userData )
    {
        return RE_False;
    }

    if ( gWin64ThreadExitFlsIndex == FLS_OUT_OF_INDEXES )
    {
        RE_SpinLock_Acquire( &gWin64ThreadExitLock );

        /* Re-checked under the lock: two threads can both find it unallocated, and allocating
         * twice would leak an index and leave one of them writing to a slot nothing reads.
         */
        if ( gWin64ThreadExitFlsIndex == FLS_OUT_OF_INDEXES )
        {
            gWin64ThreadExitCallback = callback;

            DWORD index = FlsAlloc( Win64_Thread_OnFlsDestroy );
            if ( index == FLS_OUT_OF_INDEXES )
            {
                RE_SpinLock_Release( &gWin64ThreadExitLock );

                return RE_False;
            }

            gWin64ThreadExitFlsIndex = index;
        }

        RE_SpinLock_Release( &gWin64ThreadExitLock );
    }

    return (ReBool) FlsSetValue( gWin64ThreadExitFlsIndex, userData );
}

typedef HRESULT( WINAPI *ReWin64PfnSetThreadDescription ) (HANDLE, PCWSTR);

RE_INTERNAL void
Win64_Thread_SetThreadDescription( HANDLE thread, const wchar_t *name )
{
    HMODULE kernel32 = GetModuleHandleW( L"kernel32.dll" );
    if ( !kernel32 )
    {
        return;
    }

    ReWin64PfnSetThreadDescription setThreadDescription =
        (ReWin64PfnSetThreadDescription) (void *) GetProcAddress( kernel32, "SetThreadDescription" );

    if ( setThreadDescription )
    {
        setThreadDescription( thread, name );
    }
}

#pragma pack( push, 8 )
typedef struct ReWin64ThreadNameInfo
{
    DWORD  type;
    LPCSTR name;
    DWORD  threadId;
    DWORD  flags;
} ReWin64ThreadNameInfo;
#pragma pack( pop )

/* The classic "magic exception" convention for naming threads, predating SetThreadDescription -
 * still recognized by some tooling that doesn't know about the modern API. Raising it with no
 * debugger attached to observe it can otherwise surface as a real (harmless) exception, so it's
 * wrapped in its own handler per Microsoft's documented idiom.
 */
RE_INTERNAL void
Win64_Thread_RaiseLegacyNameException( DWORD threadId, const wchar_t *name )
{
    char narrowName[64];
    WideCharToMultiByte( CP_UTF8, 0, name, -1, narrowName, sizeof( narrowName ), NULL, NULL );

    ReWin64ThreadNameInfo info;
    info.type     = 0x1000;
    info.name     = narrowName;
    info.threadId = threadId;
    info.flags    = 0;

    __try
    {
        RaiseException( 0x406D1388, 0, sizeof( info ) / sizeof( ULONG_PTR ), (ULONG_PTR *) &info );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
    }
}

void
Win64_Thread_SetThreadName( HANDLE thread, const wchar_t *name )
{
    Win64_Thread_SetThreadDescription( thread, name );
    Win64_Thread_RaiseLegacyNameException( GetThreadId( thread ), name );
}

void
Win64_Thread_SetCurrentThreadName( const wchar_t *name )
{
    Win64_Thread_SetThreadName( GetCurrentThread(), name );
}
