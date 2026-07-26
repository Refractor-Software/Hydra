/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationStringView.h>

#include <assert.h>
#include <string.h>

#include <RE/Foundation/FoundationMemory.h>

string_view
string_view_from_cstr (const char *cstr)
{
    string_view sv;
    sv.data   = (const u8 *) cstr;
    sv.length = strlen (cstr);

    return sv;
}

string_view
string_view_from_bytes (const u8 *data, usize length)
{
    string_view sv;
    sv.data   = data;
    sv.length = length;

    return sv;
}

b8
string_view_is_empty (string_view sv)
{
    return (b8) (sv.length == 0);
}

b8
string_view_equals (string_view a, string_view b)
{
    if (a.length != b.length)
    {
        return 0;
    }

    return (b8) (memory_compare (a.data, b.data, a.length) == 0);
}

s32
string_view_compare (string_view a, string_view b)
{
    usize minLength = a.length < b.length ? a.length : b.length;

    s32 result = memory_compare (a.data, b.data, minLength);
    if (result != 0)
    {
        return result;
    }

    if (a.length < b.length) { return -1; }
    if (a.length > b.length) { return 1; }

    return 0;
}

b8
string_view_starts_with (string_view sv, string_view prefix)
{
    if (prefix.length > sv.length)
    {
        return 0;
    }

    return (b8) (memory_compare (sv.data, prefix.data, prefix.length) == 0);
}

b8
string_view_ends_with (string_view sv, string_view suffix)
{
    if (suffix.length > sv.length)
    {
        return 0;
    }

    return (b8) (memory_compare (sv.data + (sv.length - suffix.length), suffix.data, suffix.length) == 0);
}

string_view
string_view_substring (string_view sv, usize start, usize length)
{
    assert (start <= sv.length && length <= sv.length - start);

    return string_view_from_bytes (sv.data + start, length);
}

b8
string_view_find_byte (string_view sv, u8 byte, usize *outIndex)
{
    for (usize i = 0; i < sv.length; i += 1)
    {
        if (sv.data[i] == byte)
        {
            *outIndex = i;
            return 1;
        }
    }

    return 0;
}

b8
string_view_find (string_view sv, string_view needle, usize *outIndex)
{
    if (needle.length == 0 || needle.length > sv.length)
    {
        return 0;
    }

    usize lastStart = sv.length - needle.length;
    for (usize i = 0; i <= lastStart; i += 1)
    {
        if (memory_compare (sv.data + i, needle.data, needle.length) == 0)
        {
            *outIndex = i;
            return 1;
        }
    }

    return 0;
}
