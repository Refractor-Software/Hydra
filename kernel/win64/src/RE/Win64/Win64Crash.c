/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64Crash.h"

#include <dbghelp.h>
#include <stdio.h>
#include <wchar.h>

#include <RE/Foundation/FoundationPrimitiveTypes.h>

internal ReBool
Win64_Crash_WriteDump (EXCEPTION_POINTERS *exceptionPointers, wchar_t *dumpPath, ReUint32 dumpPathCapacity)
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
        return RE_False;
    }

    MINIDUMP_EXCEPTION_INFORMATION dumpExceptionInfo;
    dumpExceptionInfo.ThreadId          = GetCurrentThreadId ();
    dumpExceptionInfo.ExceptionPointers = exceptionPointers;
    dumpExceptionInfo.ClientPointers    = FALSE;

    MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE) (
        MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory);

    ReBool wrote = (ReBool) MiniDumpWriteDump (
        GetCurrentProcess (), GetCurrentProcessId (), file, dumpType, &dumpExceptionInfo, NULL, NULL);

    CloseHandle (file);

    return wrote;
}

void
Win64_Crash_Init (void)
{
    SymSetOptions (SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    SymInitialize (GetCurrentProcess (), NULL, TRUE);
}

LONG WINAPI
Win64_Crash_ExceptionFilter (EXCEPTION_POINTERS *exceptionPointers)
{
    wchar_t dumpPath[MAX_PATH];
    ReBool wroteDump = Win64_Crash_WriteDump (exceptionPointers, dumpPath, MAX_PATH);

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
