/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationDebug.h

    Callstack capture and symbolisation. Boundary point: declared here, defined by the platform
    kernel.

    Exists for the memory decorators. "This allocation leaked" is nearly useless on its own;
    "this allocation leaked, and here is where it came from" is actionable, and the allocator is
    the only place that sees every allocation.
*/

#define RE_CALLSTACK_MAX_FRAMES 24

/* Fills frames with return addresses, innermost first, and returns how many were captured.
 *
 * skipFrames drops the innermost entries, so a capture helper can leave itself and its caller out
 * of the result rather than making every reader mentally skip them.
 *
 * Cheap enough to do per allocation in a debug build, and far too expensive to do in shipping.
 */
ReUint32 RE_Debug_CaptureCallstack( void **frames, ReUint32 maxFrames, ReUint32 skipFrames );

/* Formats a captured stack into buffer as one line per frame, always null-terminated and never
 * overrunning. Symbol names are included when the platform can resolve them and addresses alone
 * when it cannot, so this never fails outright - a stack of bare addresses is still a stack.
 */
void RE_Debug_FormatCallstack( void *const *frames, ReUint32 frameCount, char *buffer, ReUint64 bufferSize );
