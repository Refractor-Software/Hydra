/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "foundation/string/foundation_string.h"

#include "foundation/memory/foundation_memory.h"

#define STRING_MIN_CAPACITY 16

b8
string_create (string *s, memory_allocator *a, string_view initial)
{
    usize capacity = initial.length > STRING_MIN_CAPACITY ? initial.length : STRING_MIN_CAPACITY;

    u8 *data = (u8 *) memory_allocate (a, capacity + 1);
    if (!data)
    {
        return 0;
    }

    memory_copy (data, initial.data, initial.length);
    data[initial.length] = 0;

    s->allocator = a;
    s->data      = data;
    s->length    = initial.length;
    s->capacity  = capacity;

    return 1;
}

void
string_destroy (string *s)
{
    memory_free (s->allocator, s->data);

    s->data     = 0;
    s->length   = 0;
    s->capacity = 0;
}

static b8
string_grow_to_fit (string *s, usize additionalLength)
{
    if (additionalLength > ((usize) -1) - s->length)
    {
        return 0; /* would overflow */
    }

    usize newLength = s->length + additionalLength;
    if (newLength <= s->capacity)
    {
        return 1;
    }

    usize doubledCapacity = (s->capacity > ((usize) -1) / 2) ? (usize) -1 : s->capacity * 2;
    usize newCapacity     = doubledCapacity > newLength ? doubledCapacity : newLength;

    u8 *newData = (u8 *) memory_allocate (s->allocator, newCapacity + 1);
    if (!newData)
    {
        return 0;
    }

    memory_copy (newData, s->data, s->length);
    memory_free (s->allocator, s->data);

    s->data     = newData;
    s->capacity = newCapacity;

    return 1;
}

b8
string_append (string *s, string_view more)
{
    if (!string_grow_to_fit (s, more.length))
    {
        return 0;
    }

    memory_copy (s->data + s->length, more.data, more.length);
    s->length += more.length;
    s->data[s->length] = 0;

    return 1;
}

b8
string_append_cstr (string *s, const char *cstr)
{
    return string_append (s, string_view_from_cstr (cstr));
}

void
string_clear (string *s)
{
    s->length = 0;

    if (s->capacity > 0)
    {
        s->data[0] = 0;
    }
}

string_view
string_as_view (const string *s)
{
    return string_view_from_bytes (s->data, s->length);
}

const char *
string_as_cstr (const string *s)
{
    return (const char *) s->data;
}
