/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    foundation:

    This module is the lowest-level of them all, mostly.
    Both platform kernel and application can use it; it's meant to be generally reusable for any purpose.
*/

#include "foundation/primitive/foundation_primitive_predef.h"
#include "foundation/primitive/foundation_primitive_types.h"

#include "foundation/memory/foundation_memory.h"
#include "foundation/memory/foundation_memory_arena.h"
#include "foundation/memory/foundation_memory_pool.h"

#include "foundation/string/foundation_string_view.h"
#include "foundation/string/foundation_string.h"
#include "foundation/string/foundation_string_utf8.h"

#include "foundation/reflection/foundation_reflection.h"

#include "foundation/container/foundation_container_array.h"
#include "foundation/container/foundation_container_hash_set.h"
#include "foundation/container/foundation_container_hash_table.h"
