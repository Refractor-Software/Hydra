/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

/*
    BeginAPI.h

    Opens an API declaration scope. Always paired with EndAPI.h:

        #include <RE/Foundation/BeginAPI.h>

        // declarations...

        #include <RE/Foundation/EndAPI.h>

    Deliberately has no include guard - it is meant to be included once per API block, many times
    per translation unit. What it pins down is everything about a declaration that a *consumer's*
    compiler settings could otherwise silently change: linkage and struct packing. An engine built
    with default packing and a game built with /Zp1 must still agree on where a struct's fields
    live, and that agreement can't be left to whoever happens to include the header.
*/

#include "RE/Foundation/FoundationCompiler.h"

/* Catches a missing EndAPI.h at the point of the *second* BeginAPI.h, which is far closer to the
 * mistake than the link-time or layout weirdness it would otherwise cause.
 */
#if defined( RE_API_SCOPE_OPEN )
#error "BeginAPI.h included while an API scope is already open - a matching EndAPI.h is missing."
#endif

#define RE_API_SCOPE_OPEN 1

/* C4103 fires whenever a header ends with different packing than it began with - which is the
 * entire job of a Begin/End pair, so it can only ever be noise here. The Windows SDK's own
 * pshpack8.h/poppack.h disable it exactly this way, for exactly this reason.
 */
#if defined( _MSC_VER )
#pragma warning( disable : 4103 )
#endif

/* 8 is the natural alignment ceiling on the 64-bit targets we ship to, and matches every
 * compiler's default here - the point is to be immune to a consumer overriding that default,
 * not to change it.
 */
#pragma pack( push, 8 )

#if RE_LANGUAGE_CPP
extern "C" {
#endif
