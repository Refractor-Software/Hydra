/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

/*
    EndAPI.h

    Closes the API declaration scope opened by BeginAPI.h, restoring whatever linkage, packing,
    and warning state the consumer had before it. See BeginAPI.h for the rationale.

    Deliberately has no include guard, for the same reason BeginAPI.h doesn't.
*/

#include "RE/Foundation/FoundationCompiler.h"

#if !defined( RE_API_SCOPE_OPEN )
#error "EndAPI.h included without a matching BeginAPI.h."
#endif

#if RE_LANGUAGE_CPP
}
#endif

/* See BeginAPI.h - same reason, and it has to be repeated because this header changes packing
 * across its own boundary too.
 */
#if defined( _MSC_VER )
#pragma warning( disable : 4103 )
#endif

#pragma pack( pop )

#undef RE_API_SCOPE_OPEN
