/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    win64_commandline.h

    Parses the real process command line into UTF-8 string_views, backed by process-lifetime
    kernel-static storage. This is the only place in the whole project that knows
    CommandLineToArgvW, GetCommandLineW, or wide-to-UTF-8 command-line conversion exist.

    Deliberately does NOT use wWinMain's own commandLine parameter - that's documented to exclude
    argv[0] (a legacy artifact predating CommandLineToArgvW). GetCommandLineW() +
    CommandLineToArgvW() is the correct, MSDN-recommended pairing for real argv/argc on Windows.
*/

#include "RE/Win64/Win64.h"

#include <RE/Foundation/FoundationStringView.h>

/* Call once, early in wWinMain, after log/crash init. */
void win64_command_line_init (void);

s32          win64_command_line_get_arg_count (void);
string_view *win64_command_line_get_args      (void);
