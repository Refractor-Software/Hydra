/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "win64/thread/win64_thread.h"

typedef HRESULT (WINAPI *win64_pfn_set_thread_description) (HANDLE, PCWSTR);

static void
win64_thread_set_thread_description (HANDLE thread, const wchar_t *name)
{
    HMODULE kernel32 = GetModuleHandleW (L"kernel32.dll");
    if (!kernel32)
    {
        return;
    }

    win64_pfn_set_thread_description setThreadDescription =
        (win64_pfn_set_thread_description) (void *) GetProcAddress (kernel32, "SetThreadDescription");

    if (setThreadDescription)
    {
        setThreadDescription (thread, name);
    }
}

#pragma pack(push, 8)
typedef struct win64_thread_name_info
{
    DWORD  type;
    LPCSTR name;
    DWORD  threadId;
    DWORD  flags;
} win64_thread_name_info;
#pragma pack(pop)

/* The classic "magic exception" convention for naming threads, predating SetThreadDescription -
 * still recognized by some tooling that doesn't know about the modern API. Raising it with no
 * debugger attached to observe it can otherwise surface as a real (harmless) exception, so it's
 * wrapped in its own handler per Microsoft's documented idiom.
 */
static void
win64_thread_raise_legacy_name_exception (DWORD threadId, const wchar_t *name)
{
    char narrowName[64];
    WideCharToMultiByte (CP_UTF8, 0, name, -1, narrowName, sizeof (narrowName), NULL, NULL);

    win64_thread_name_info info;
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
win64_thread_set_thread_name (HANDLE thread, const wchar_t *name)
{
    win64_thread_set_thread_description (thread, name);
    win64_thread_raise_legacy_name_exception (GetThreadId (thread), name);
}

void
win64_thread_set_current_thread_name (const wchar_t *name)
{
    win64_thread_set_thread_name (GetCurrentThread (), name);
}
