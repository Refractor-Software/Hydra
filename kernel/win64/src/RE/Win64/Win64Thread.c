/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64Thread.h"

typedef HRESULT (WINAPI *ReWin64PfnSetThreadDescription) (HANDLE, PCWSTR);

internal void
Win64_Thread_SetThreadDescription (HANDLE thread, const wchar_t *name)
{
    HMODULE kernel32 = GetModuleHandleW (L"kernel32.dll");
    if (!kernel32)
    {
        return;
    }

    ReWin64PfnSetThreadDescription setThreadDescription =
        (ReWin64PfnSetThreadDescription) (void *) GetProcAddress (kernel32, "SetThreadDescription");

    if (setThreadDescription)
    {
        setThreadDescription (thread, name);
    }
}

#pragma pack(push, 8)
typedef struct ReWin64ThreadNameInfo
{
    DWORD  type;
    LPCSTR name;
    DWORD  threadId;
    DWORD  flags;
} ReWin64ThreadNameInfo;
#pragma pack(pop)

/* The classic "magic exception" convention for naming threads, predating SetThreadDescription -
 * still recognized by some tooling that doesn't know about the modern API. Raising it with no
 * debugger attached to observe it can otherwise surface as a real (harmless) exception, so it's
 * wrapped in its own handler per Microsoft's documented idiom.
 */
internal void
Win64_Thread_RaiseLegacyNameException (DWORD threadId, const wchar_t *name)
{
    char narrowName[64];
    WideCharToMultiByte (CP_UTF8, 0, name, -1, narrowName, sizeof (narrowName), NULL, NULL);

    ReWin64ThreadNameInfo info;
    info.type     = 0x1000;
    info.name     = narrowName;
    info.threadId = threadId;
    info.flags    = 0;

    __try
    {
        RaiseException (0x406D1388, 0, sizeof (info) / sizeof (ULONG_PTR), (ULONG_PTR *) &info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void
Win64_Thread_SetThreadName (HANDLE thread, const wchar_t *name)
{
    Win64_Thread_SetThreadDescription (thread, name);
    Win64_Thread_RaiseLegacyNameException (GetThreadId (thread), name);
}

void
Win64_Thread_SetCurrentThreadName (const wchar_t *name)
{
    Win64_Thread_SetThreadName (GetCurrentThread (), name);
}
