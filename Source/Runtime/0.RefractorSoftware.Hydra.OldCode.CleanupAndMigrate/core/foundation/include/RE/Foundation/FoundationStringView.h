/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/*
    FoundationStringView.h

    A non-owning view over UTF-8 bytes. Deliberately not null-terminated (length-prefixed) -
    avoids null-terminator scans anywhere they can be avoided. Never allocates; needs no
    ReAllocator at all.
*/

typedef struct ReStringView
{
    const ReUint8 *data;
    ReUint64     length;
} ReStringView;

/* The one acceptable strlen()-equivalent scan - C string literals/interop are an unavoidable
 * entry point for null-terminated data.
 */
ReStringView RE_StringView_FromCStr( const char *cstr );
ReStringView RE_StringView_FromBytes( const ReUint8 *data, ReUint64 length );

/* Zero-scan for the common case of a compile-time string literal - strictly better than
 * RE_StringView_FromCStr() when the literal (and its length) are already known at compile time.
 */
#define RE_STRING_VIEW_FROM_LITERAL( strLiteral ) \
    ((ReStringView) {(const ReUint8 *) (strLiteral), sizeof( strLiteral ) - 1})

ReBool RE_StringView_IsEmpty( ReStringView sv );
ReBool RE_StringView_Equals( ReStringView a, ReStringView b );

/* Deterministic sign+order at the first differing byte - not a raw memcmp() passthrough, whose
 * return magnitude beyond its sign is implementation-defined.
 */
ReSint32 RE_StringView_Compare( ReStringView a, ReStringView b );

ReBool RE_StringView_StartsWith( ReStringView sv, ReStringView prefix );
ReBool RE_StringView_EndsWith( ReStringView sv, ReStringView suffix );

/* Asserts if start+length exceeds sv.length - an out-of-bounds slice is a programmer error, not
 * a runtime condition.
 */
ReStringView RE_StringView_Substring( ReStringView sv, ReUint64 start, ReUint64 length );

/* False (not found) is a normal outcome, not an error. */
ReBool RE_StringView_FindByte( ReStringView sv, ReUint8 byte, ReUint64 *outIndex );
ReBool RE_StringView_Find( ReStringView sv, ReStringView needle, ReUint64 *outIndex );
