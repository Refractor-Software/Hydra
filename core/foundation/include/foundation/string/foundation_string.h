/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include "foundation/primitive/foundation_primitive_types.h"

#include "foundation/memory/foundation_memory_allocator.h"
#include "foundation/string/foundation_string_view.h"

/*
    foundation_string.h

    An owning, growable UTF-8 string backed by a caller-supplied memory_allocator - never
    allocates from the OS itself. Unlike string_view, this maintains a trailing null terminator
    as a zero-copy string_as_cstr() convenience for CRT/OS interop; capacity excludes that byte.
*/

typedef struct string
{
    memory_allocator *allocator;
    u8    *data;
    usize  length;   /* content length, excludes the trailing terminator byte */
    usize  capacity; /* usable content capacity; the real allocation is capacity + 1 (to make space for the terminator) */
} string;

/* Returns 0 only if the initial allocation itself fails. */
b8   string_create  (string *s, memory_allocator *a, string_view initial);
void string_destroy (string *s);

/* Grow by doubling (per this project's own style guide). Returns 0 if growth was needed and the
 * allocator couldn't satisfy it - s is left unmodified in that case.
 */
b8 string_append      (string *s, string_view more);
b8 string_append_cstr (string *s, const char *cstr);

/* Sets length to 0, keeps the allocated capacity - cheap reuse, mirrors arena_reset(). */
void string_clear (string *s);

string_view string_as_view (const string *s);
const char *string_as_cstr (const string *s);
