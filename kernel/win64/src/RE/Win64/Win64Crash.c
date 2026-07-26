/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64Crash.h"

#include <dbghelp.h>
#include <stdio.h>
#include <wchar.h>

#include <RE/Foundation/FoundationPrimitiveTypes.h>

static b8
win64_crash_write_dump (EXCEPTION_POINTERS *exceptionPointers, wchar_t *dumpPath, u32 dumpPathCapacity)
{
    wchar_t moduleDirectory[MAX_PATH];
    GetModuleFileNameW (NULL, moduleDirectory, MAX_PATH);

    wchar_t *lastSlash = wcsrchr (moduleDirectory, L'\\');
    if (lastSlash)
    {
        *(lastSlash + 1) = L'\0';
    }
    else
    {
        moduleDirectory[0] = L'\0';
    }

    SYSTEMTIME localTime;
    GetLocalTime (&localTime);

    swprintf_s (dumpPath, dumpPathCapacity, L"%lsHydra_%04u%02u%02u_%02u%02u%02u.dmp",
                moduleDirectory, localTime.wYear, localTime.wMonth, localTime.wDay,
                localTime.wHour, localTime.wMinute, localTime.wSecond);

    HANDLE file = CreateFileW (dumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    MINIDUMP_EXCEPTION_INFORMATION dumpExceptionInfo;
    dumpExceptionInfo.ThreadId          = GetCurrentThreadId ();
    dumpExceptionInfo.ExceptionPointers = exceptionPointers;
    dumpExceptionInfo.ClientPointers    = FALSE;

    MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE) (
        MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory);

    b8 wrote = (b8) MiniDumpWriteDump (
        GetCurrentProcess (), GetCurrentProcessId (), file, dumpType, &dumpExceptionInfo, NULL, NULL);

    CloseHandle (file);

    return wrote;
}

void
win64_crash_init (void)
{
    SymSetOptions (SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    SymInitialize (GetCurrentProcess (), NULL, TRUE);
}

LONG WINAPI
win64_crash_exception_filter (EXCEPTION_POINTERS *exceptionPointers)
{
    wchar_t dumpPath[MAX_PATH];
    b8 wroteDump = win64_crash_write_dump (exceptionPointers, dumpPath, MAX_PATH);

    wchar_t message[MAX_PATH + 128];
    if (wroteDump)
    {
        swprintf_s (message, MAX_PATH + 128, L"Hydra has crashed.\n\nA crash dump was written to:\n%ls", dumpPath);
    }
    else
    {
        swprintf_s (message, MAX_PATH + 128, L"Hydra has crashed, and the crash dump could not be written.");
    }

    MessageBoxW (NULL, message, L"Hydra - Crash", MB_OK | MB_ICONERROR);

    return EXCEPTION_EXECUTE_HANDLER;
}
