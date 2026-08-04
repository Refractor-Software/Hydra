/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    Win64CommandLine.h

    Parses the real process command line into UTF-8 ReStringViews, backed by process-lifetime
    kernel-static storage. This is the only place in the whole project that knows
    CommandLineToArgvW, GetCommandLineW, or wide-to-UTF-8 command-line conversion exist.

    Deliberately does NOT use wWinMain's own commandLine parameter - that's documented to exclude
    argv[0] (a legacy artifact predating CommandLineToArgvW). GetCommandLineW() +
    CommandLineToArgvW() is the correct, MSDN-recommended pairing for real argv/argc on Windows.
*/

#include "RE/Win64/Win64.h"

#include <RE/Foundation/FoundationStringView.h>

/* Call once, early in wWinMain, after log/crash init. */
void Win64_CommandLine_Init( void );

ReSint32          Win64_CommandLine_GetArgCount( void );
ReStringView *Win64_CommandLine_GetArgs( void );
