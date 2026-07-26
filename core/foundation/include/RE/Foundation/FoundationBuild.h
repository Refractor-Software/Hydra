/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    FoundationBuild.h

    Build configuration as an ordered scale rather than a set of independent booleans, so code can
    ask "is this build at least as stripped as shipping?" with a plain comparison:

        #if RE_BUILD < RE_BUILD_SHIPPING

    An ordering is what's actually wanted at nearly every use site - development wants most of
    what debug has, and shipping wants none of it. A pile of separate RE_DEBUG/RE_SHIPPING flags
    can't express that without every site restating the relationship (and eventually getting it
    wrong).
*/

#define RE_BUILD_DEBUG       0
#define RE_BUILD_DEVELOPMENT 1
#define RE_BUILD_SHIPPING    2

/* Normally supplied by the build system (see the root CMakeLists.txt). Defaulting to development
 * rather than shipping is deliberate: a build that somehow escapes the build system should show
 * up loud and instrumented, not silently stripped.
 */
#if !defined( RE_BUILD )
#define RE_BUILD RE_BUILD_DEVELOPMENT
#endif

#if RE_BUILD != RE_BUILD_DEBUG && RE_BUILD != RE_BUILD_DEVELOPMENT && RE_BUILD != RE_BUILD_SHIPPING
#error "RE_BUILD must be one of RE_BUILD_DEBUG, RE_BUILD_DEVELOPMENT, or RE_BUILD_SHIPPING."
#endif
