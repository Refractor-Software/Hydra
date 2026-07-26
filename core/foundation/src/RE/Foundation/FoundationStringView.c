/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationStringView.h>

#include <assert.h>
#include <string.h>

#include <RE/Foundation/FoundationMemory.h>

ReStringView
RE_StringView_FromCStr (const char *cstr)
{
    ReStringView sv;
    sv.data   = (const ReUint8 *) cstr;
    sv.length = strlen (cstr);

    return sv;
}

ReStringView
RE_StringView_FromBytes (const ReUint8 *data, ReUint64 length)
{
    ReStringView sv;
    sv.data   = data;
    sv.length = length;

    return sv;
}

ReBool
RE_StringView_IsEmpty (ReStringView sv)
{
    return (ReBool) (sv.length == 0);
}

ReBool
RE_StringView_Equals (ReStringView a, ReStringView b)
{
    if (a.length != b.length)
    {
        return RE_False;
    }

    return (ReBool) (RE_Memory_Compare (a.data, b.data, a.length) == 0);
}

ReSint32
RE_StringView_Compare (ReStringView a, ReStringView b)
{
    ReUint64 minLength = a.length < b.length ? a.length : b.length;

    ReSint32 result = RE_Memory_Compare (a.data, b.data, minLength);
    if (result != 0)
    {
        return result;
    }

    if (a.length < b.length) { return -1; }
    if (a.length > b.length) { return 1; }

    return 0;
}

ReBool
RE_StringView_StartsWith (ReStringView sv, ReStringView prefix)
{
    if (prefix.length > sv.length)
    {
        return RE_False;
    }

    return (ReBool) (RE_Memory_Compare (sv.data, prefix.data, prefix.length) == 0);
}

ReBool
RE_StringView_EndsWith (ReStringView sv, ReStringView suffix)
{
    if (suffix.length > sv.length)
    {
        return RE_False;
    }

    return (ReBool) (RE_Memory_Compare (sv.data + (sv.length - suffix.length), suffix.data, suffix.length) == 0);
}

ReStringView
RE_StringView_Substring (ReStringView sv, ReUint64 start, ReUint64 length)
{
    assert (start <= sv.length && length <= sv.length - start);

    return RE_StringView_FromBytes (sv.data + start, length);
}

ReBool
RE_StringView_FindByte (ReStringView sv, ReUint8 byte, ReUint64 *outIndex)
{
    for (ReUint64 i = 0; i < sv.length; i += 1)
    {
        if (sv.data[i] == byte)
        {
            *outIndex = i;
            return RE_True;
        }
    }

    return RE_False;
}

ReBool
RE_StringView_Find (ReStringView sv, ReStringView needle, ReUint64 *outIndex)
{
    if (needle.length == 0 || needle.length > sv.length)
    {
        return RE_False;
    }

    ReUint64 lastStart = sv.length - needle.length;
    for (ReUint64 i = 0; i <= lastStart; i += 1)
    {
        if (RE_Memory_Compare (sv.data + i, needle.data, needle.length) == 0)
        {
            *outIndex = i;
            return RE_True;
        }
    }

    return RE_False;
}
