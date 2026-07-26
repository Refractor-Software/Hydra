/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    foundation:

    This module is the lowest-level of them all, mostly.
    Both platform kernel and application can use it; it's meant to be generally reusable for any purpose.
*/

#include <RE/Foundation/FoundationPrimitivePredef.h>
#include <RE/Foundation/FoundationPrimitiveTypes.h>

#include <RE/Foundation/FoundationMemory.h>
#include <RE/Foundation/FoundationMemoryArena.h>
#include <RE/Foundation/FoundationMemoryPool.h>

#include <RE/Foundation/FoundationStringView.h>
#include <RE/Foundation/FoundationString.h>
#include <RE/Foundation/FoundationStringUtf8.h>

#include <RE/Foundation/FoundationReflection.h>

#include <RE/Foundation/FoundationContainerArray.h>
#include <RE/Foundation/FoundationContainerHashSet.h>
#include <RE/Foundation/FoundationContainerHashTable.h>
