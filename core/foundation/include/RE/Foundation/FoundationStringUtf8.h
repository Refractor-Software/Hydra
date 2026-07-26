/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

#include <RE/Foundation/FoundationStringView.h>

/*
    FoundationStringUtf8.h

    Codepoint decode/iterate over UTF-8 byte data. Byte storage was chosen for ReStringView/ReString,
    but character-level (not just byte-level) reasoning is sometimes needed - this is that escape
    hatch. No case-folding/normalization/collation - byte operations plus codepoint iteration is
    the ceiling here.
*/

#define RE_UTF8_REPLACEMENT_CODEPOINT 0xFFFDu

/* Decodes one codepoint starting at bytes[0]. Always consumes and returns at least 1 byte, even
 * on malformed/truncated input (substitutes RE_UTF8_REPLACEMENT_CODEPOINT and resyncs at the next
 * byte) - never returns 0. Guaranteed forward progress means a naive `while (i < length) { i +=
 * RE_Utf8_Decode(...); }` loop can never infinite-loop on bad data, which is the whole point: keep
 * running on malformed input from a corrupted asset or command-line argument, don't halt.
 * remainingLength must be > 0 - decoding with nothing left is a caller contract violation.
 */
ReUint64 RE_Utf8_Decode (const ReUint8 *bytes, ReUint64 remainingLength, ReUint32 *outCodepoint);

/* Encodes one codepoint into outBytes (needs up to 4 bytes) and returns how many were written.
 * codepoint must be a valid Unicode scalar value (<= 0x10FFFF, not a surrogate) - unlike decode(),
 * this takes a caller-controlled value, so an invalid codepoint here is a programmer error and
 * asserts rather than substituting.
 */
ReUint64 RE_Utf8_Encode (ReUint32 codepoint, ReUint8 outBytes[4]);

/* True iff every codepoint in sv decoded cleanly - no malformed/truncated sequences anywhere. */
ReBool RE_Utf8_IsValid (ReStringView sv);

/* O(n) by construction - full-view iteration is unavoidable to count codepoints in UTF-8. */
ReUint64 RE_Utf8_CodepointCount (ReStringView sv);

typedef struct ReUtf8Iterator
{
    ReStringView view;
    ReUint64        offset;
} ReUtf8Iterator;

ReUtf8Iterator RE_Utf8_IteratorCreate (ReStringView sv);

/* False at end-of-view (outCodepoint untouched). Malformed sequences are substituted the same
 * way RE_Utf8_Decode() does, never stalling the iterator.
 */
ReBool RE_Utf8_IteratorNext (ReUtf8Iterator *it, ReUint32 *outCodepoint);
