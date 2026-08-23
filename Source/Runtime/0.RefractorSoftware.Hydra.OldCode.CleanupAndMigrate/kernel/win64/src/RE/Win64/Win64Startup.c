/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64Startup.h"

#include <intrin.h>

RE_INTERNAL ReBool
Win64_Startup_CpuHasAvx( void )
{
    int cpuInfo[4];
    __cpuid( cpuInfo, 1 );

    ReBool hasAvxBit  = (ReBool) ((cpuInfo[2] & (1 << 28)) != 0);
    ReBool hasOsxsave = (ReBool) ((cpuInfo[2] & (1 << 27)) != 0);

    if ( !hasAvxBit || !hasOsxsave )
    {
        return RE_False;
    }

    /* CPUID reporting the AVX bit isn't enough on its own - the OS also has to have opted the
     * extended (YMM) register state into its context-save set, or using AVX instructions will
     * fault regardless of what the CPU itself supports.
     */
    unsigned __int64 xcr0 = _xgetbv( 0 );

    return (ReBool) ((xcr0 & 0x6) == 0x6);
}

#if RE_TARGET_ISA_AVX2
RE_INTERNAL ReBool
Win64_Startup_CpuHasAvx2( void )
{
    int maxLeafInfo[4];
    __cpuid( maxLeafInfo, 0 );
    if ( maxLeafInfo[0] < 7 )
    {
        return RE_False;
    }

    int extInfo[4];
    __cpuidex( extInfo, 7, 0 );

    return (ReBool) ((extInfo[1] & (1 << 5)) != 0);
}
#endif

ReBool
Win64_Startup_CheckCpuFeatures( void )
{
    ReBool supported = Win64_Startup_CpuHasAvx();

#if RE_TARGET_ISA_AVX2
    supported = (ReBool) (supported && Win64_Startup_CpuHasAvx2());
#endif

    if ( !supported )
    {
#if RE_TARGET_ISA_AVX2
        const wchar_t *message = L"This CPU (or OS configuration) does not support AVX2, which this build requires.";
#else
        const wchar_t *message = L"This CPU (or OS configuration) does not support AVX, which this build requires.";
#endif
        MessageBoxW( NULL, message, L"Hydra - Unsupported CPU", MB_OK | MB_ICONERROR );

        return RE_False;
    }

    return RE_True;
}

void
Win64_Startup_SetDpiAwareness( void )
{
    SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
}

void
Win64_Startup_InitCom( void )
{
    CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
}

void
Win64_Startup_ShutdownCom( void )
{
    CoUninitialize();
}

void
Win64_Startup_ConfigureTiming( void )
{
    timeBeginPeriod( 1 );
    SetPriorityClass( GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS );
}

void
Win64_Startup_ShutdownTiming( void )
{
    timeEndPeriod( 1 );
}
