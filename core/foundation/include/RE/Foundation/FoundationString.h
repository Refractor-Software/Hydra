/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

#include <RE/Foundation/FoundationPrimitiveTypes.h>

#include <RE/Foundation/FoundationMemoryAllocator.h>
#include <RE/Foundation/FoundationStringView.h>

/*
    FoundationString.h

    An owning, growable UTF-8 string backed by a caller-supplied ReAllocator - never
    allocates from the OS itself. Unlike ReStringView, this maintains a trailing null terminator
    as a zero-copy RE_String_AsCStr() convenience for CRT/OS interop; capacity excludes that byte.
*/

typedef struct ReString
{
    ReAllocator *allocator;
    ReUint8    *data;
    ReUint64  length;   /* content length, excludes the trailing terminator byte */
    ReUint64  capacity; /* usable content capacity; the real allocation is capacity + 1 (to make space for the terminator) */
} ReString;

/* Returns 0 only if the initial allocation itself fails. */
ReBool   RE_String_Create  (ReString *s, ReAllocator *a, ReStringView initial);
void RE_String_Destroy (ReString *s);

/* Grow by doubling (per this project's own style guide). Returns 0 if growth was needed and the
 * allocator couldn't satisfy it - s is left unmodified in that case.
 */
ReBool RE_String_Append      (ReString *s, ReStringView more);
ReBool RE_String_AppendCStr (ReString *s, const char *cstr);

/* Sets length to 0, keeps the allocated capacity - cheap reuse, mirrors RE_Arena_Reset(). */
void RE_String_Clear (ReString *s);

ReStringView RE_String_AsView (const ReString *s);
const char *RE_String_AsCStr (const ReString *s);
