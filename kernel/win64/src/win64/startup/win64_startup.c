/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "win64/startup/win64_startup.h"

#include <intrin.h>

static b8
win64_startup_cpu_has_avx (void)
{
    int cpuInfo[4];
    __cpuid (cpuInfo, 1);

    b8 hasAvxBit  = (b8) ((cpuInfo[2] & (1 << 28)) != 0);
    b8 hasOsxsave = (b8) ((cpuInfo[2] & (1 << 27)) != 0);

    if (!hasAvxBit || !hasOsxsave)
    {
        return 0;
    }

    /* CPUID reporting the AVX bit isn't enough on its own - the OS also has to have opted the
     * extended (YMM) register state into its context-save set, or using AVX instructions will
     * fault regardless of what the CPU itself supports.
     */
    unsigned __int64 xcr0 = _xgetbv (0);

    return (b8) ((xcr0 & 0x6) == 0x6);
}

#if HYDRA_TARGET_ISA_AVX2
static b8
win64_startup_cpu_has_avx2 (void)
{
    int maxLeafInfo[4];
    __cpuid (maxLeafInfo, 0);
    if (maxLeafInfo[0] < 7)
    {
        return 0;
    }

    int extInfo[4];
    __cpuidex (extInfo, 7, 0);

    return (b8) ((extInfo[1] & (1 << 5)) != 0);
}
#endif

b8
win64_startup_check_cpu_features (void)
{
    b8 supported = win64_startup_cpu_has_avx ();

#if HYDRA_TARGET_ISA_AVX2
    supported = (b8) (supported && win64_startup_cpu_has_avx2 ());
#endif

    if (!supported)
    {
#if HYDRA_TARGET_ISA_AVX2
        const wchar_t *message = L"This CPU (or OS configuration) does not support AVX2, which this build requires.";
#else
        const wchar_t *message = L"This CPU (or OS configuration) does not support AVX, which this build requires.";
#endif
        MessageBoxW (NULL, message, L"Hydra - Unsupported CPU", MB_OK | MB_ICONERROR);

        return 0;
    }

    return 1;
}

void
win64_startup_set_dpi_awareness (void)
{
    SetProcessDpiAwarenessContext (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

void
win64_startup_init_com (void)
{
    CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
}

void
win64_startup_shutdown_com (void)
{
    CoUninitialize ();
}

void
win64_startup_configure_timing (void)
{
    timeBeginPeriod (1);
    SetPriorityClass (GetCurrentProcess (), ABOVE_NORMAL_PRIORITY_CLASS);
}

void
win64_startup_shutdown_timing (void)
{
    timeEndPeriod (1);
}
