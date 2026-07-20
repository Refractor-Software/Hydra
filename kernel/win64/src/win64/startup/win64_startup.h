/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    win64_startup.h

    One-shot process/environment setup: CPU feature gating, DPI awareness, COM, and timer
    resolution. Everything here runs once, early, around window/device creation.
*/

#include "win64/win64.h"

#include "foundation/primitive/foundation_primitive_types.h"

/* Must be the very first thing wWinMain does - before log/crash/anything else exists. Verifies
 * the CPU (and OS) actually support the ISA this build was compiled for (HYDRA_TARGET_ISA,
 * AVX or AVX2 - see the root CMakeLists.txt). Shows its own MessageBoxW and returns 0 on
 * failure, since no log system exists yet at this point.
 */
b8 win64_startup_check_cpu_features (void);

/* Opts into per-monitor DPI awareness. Call before the window is created. */
void win64_startup_set_dpi_awareness (void);

/* CoInitializeEx(COINIT_APARTMENTTHREADED) / CoUninitialize pair - call once, early/late. */
void win64_startup_init_com (void);
void win64_startup_shutdown_com (void);

/* Raises OS timer resolution and process priority for smoother frame pacing. Call once, right
 * before entering the main loop; pair with win64_startup_shutdown_timing() after it ends.
 */
void win64_startup_configure_timing (void);
void win64_startup_shutdown_timing (void);
