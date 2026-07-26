/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    Win64Startup.h

    One-shot process/environment setup: CPU feature gating, DPI awareness, COM, and timer
    resolution. Everything here runs once, early, around window/device creation.
*/

#include "RE/Win64/Win64.h"

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/* Must be the very first thing wWinMain does - before log/crash/anything else exists. Verifies
 * the CPU (and OS) actually support the ISA this build was compiled for (RE_TARGET_ISA,
 * AVX or AVX2 - see the root CMakeLists.txt). Shows its own MessageBoxW and returns 0 on
 * failure, since no log system exists yet at this point.
 */
ReBool Win64_Startup_CheckCpuFeatures (void);

/* Opts into per-monitor DPI awareness. Call before the window is created. */
void Win64_Startup_SetDpiAwareness (void);

/* CoInitializeEx(COINIT_APARTMENTTHREADED) / CoUninitialize pair - call once, early/late. */
void Win64_Startup_InitCom (void);
void Win64_Startup_ShutdownCom (void);

/* Raises OS timer resolution and process priority for smoother frame pacing. Call once, right
 * before entering the main loop; pair with Win64_Startup_ShutdownTiming() after it ends.
 */
void Win64_Startup_ConfigureTiming (void);
void Win64_Startup_ShutdownTiming (void);
