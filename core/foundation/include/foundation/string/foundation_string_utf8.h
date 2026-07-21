/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include "foundation/primitive/foundation_primitive_types.h"

#include "foundation/string/foundation_string_view.h"

/*
    foundation_string_utf8.h

    Codepoint decode/iterate over UTF-8 byte data. Byte storage was chosen for string_view/string,
    but character-level (not just byte-level) reasoning is sometimes needed - this is that escape
    hatch. No case-folding/normalization/collation - byte operations plus codepoint iteration is
    the ceiling here.
*/

#define UTF8_REPLACEMENT_CODEPOINT 0xFFFDu

/* Decodes one codepoint starting at bytes[0]. Always consumes and returns at least 1 byte, even
 * on malformed/truncated input (substitutes UTF8_REPLACEMENT_CODEPOINT and resyncs at the next
 * byte) - never returns 0. Guaranteed forward progress means a naive `while (i < length) { i +=
 * utf8_decode(...); }` loop can never infinite-loop on bad data, which is the whole point: keep
 * running on malformed input from a corrupted asset or command-line argument, don't halt.
 * remainingLength must be > 0 - decoding with nothing left is a caller contract violation.
 */
usize utf8_decode (const u8 *bytes, usize remainingLength, u32 *outCodepoint);

/* Encodes one codepoint into outBytes (needs up to 4 bytes) and returns how many were written.
 * codepoint must be a valid Unicode scalar value (<= 0x10FFFF, not a surrogate) - unlike decode(),
 * this takes a caller-controlled value, so an invalid codepoint here is a programmer error and
 * asserts rather than substituting.
 */
usize utf8_encode (u32 codepoint, u8 outBytes[4]);

/* True iff every codepoint in sv decoded cleanly - no malformed/truncated sequences anywhere. */
b8 utf8_is_valid (string_view sv);

/* O(n) by construction - full-view iteration is unavoidable to count codepoints in UTF-8. */
usize utf8_codepoint_count (string_view sv);

typedef struct utf8_iterator
{
    string_view view;
    usize        offset;
} utf8_iterator;

utf8_iterator utf8_iterator_create (string_view sv);

/* False at end-of-view (outCodepoint untouched). Malformed sequences are substituted the same
 * way utf8_decode() does, never stalling the iterator.
 */
b8 utf8_iterator_next (utf8_iterator *it, u32 *outCodepoint);
