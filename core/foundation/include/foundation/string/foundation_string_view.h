/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include "foundation/primitive/foundation_primitive_types.h"

/*
    foundation_string_view.h

    A non-owning view over UTF-8 bytes. Deliberately not null-terminated (length-prefixed) -
    avoids null-terminator scans anywhere they can be avoided. Never allocates; needs no
    memory_allocator at all.
*/

typedef struct string_view
{
    const u8 *data;
    usize     length;
} string_view;

/* The one acceptable strlen()-equivalent scan - C string literals/interop are an unavoidable
 * entry point for null-terminated data.
 */
string_view string_view_from_cstr  (const char *cstr);
string_view string_view_from_bytes (const u8 *data, usize length);

/* Zero-scan for the common case of a compile-time string literal - strictly better than
 * string_view_from_cstr() when the literal (and its length) are already known at compile time.
 */
#define STRING_VIEW_FROM_LITERAL(strLiteral) \
    ((string_view) {(const u8 *) (strLiteral), sizeof (strLiteral) - 1})

b8 string_view_is_empty (string_view sv);
b8 string_view_equals   (string_view a, string_view b);

/* Deterministic sign+order at the first differing byte - not a raw memcmp() passthrough, whose
 * return magnitude beyond its sign is implementation-defined.
 */
s32 string_view_compare (string_view a, string_view b);

b8 string_view_starts_with (string_view sv, string_view prefix);
b8 string_view_ends_with   (string_view sv, string_view suffix);

/* Asserts if start+length exceeds sv.length - an out-of-bounds slice is a programmer error, not
 * a runtime condition.
 */
string_view string_view_substring (string_view sv, usize start, usize length);

/* False (not found) is a normal outcome, not an error. */
b8 string_view_find_byte (string_view sv, u8 byte, usize *outIndex);
b8 string_view_find      (string_view sv, string_view needle, usize *outIndex);
