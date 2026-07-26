/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationString.h>

#include <RE/Foundation/FoundationMemory.h>

#define STRING_MIN_CAPACITY 16

ReBool
RE_String_Create( ReString *s, ReAllocator *a, ReStringView initial )
{
    ReUint64 capacity = initial.length > STRING_MIN_CAPACITY ? initial.length : STRING_MIN_CAPACITY;

    ReUint8 *data = (ReUint8 *) RE_Memory_Allocate( a, capacity + 1 );
    if ( !data )
    {
        return RE_False;
    }

    RE_Memory_Copy( data, initial.data, initial.length );
    data[initial.length] = 0;

    s->allocator = a;
    s->data      = data;
    s->length    = initial.length;
    s->capacity  = capacity;

    return RE_True;
}

void
RE_String_Destroy( ReString *s )
{
    RE_Memory_Free( s->allocator, s->data );

    s->data     = 0;
    s->length   = 0;
    s->capacity = 0;
}

internal ReBool
String_GrowToFit( ReString *s, ReUint64 additionalLength )
{
    if ( additionalLength > ((ReUint64) -1) - s->length )
    {
        return 0; /* would overflow */
    }

    ReUint64 newLength = s->length + additionalLength;
    if ( newLength <= s->capacity )
    {
        return RE_True;
    }

    ReUint64 doubledCapacity = (s->capacity > ((ReUint64) -1) / 2) ? (ReUint64) -1 : s->capacity * 2;
    ReUint64 newCapacity     = doubledCapacity > newLength ? doubledCapacity : newLength;

    ReUint8 *newData = (ReUint8 *) RE_Memory_Allocate( s->allocator, newCapacity + 1 );
    if ( !newData )
    {
        return RE_False;
    }

    RE_Memory_Copy( newData, s->data, s->length );
    RE_Memory_Free( s->allocator, s->data );

    s->data     = newData;
    s->capacity = newCapacity;

    return RE_True;
}

ReBool
RE_String_Append( ReString *s, ReStringView more )
{
    if ( !String_GrowToFit( s, more.length ) )
    {
        return RE_False;
    }

    RE_Memory_Copy( s->data + s->length, more.data, more.length );
    s->length += more.length;
    s->data[s->length] = 0;

    return RE_True;
}

ReBool
RE_String_AppendCStr( ReString *s, const char *cstr )
{
    return RE_String_Append( s, RE_StringView_FromCStr( cstr ) );
}

void
RE_String_Clear( ReString *s )
{
    s->length = 0;

    if ( s->capacity > 0 )
    {
        s->data[0] = 0;
    }
}

ReStringView
RE_String_AsView( const ReString *s )
{
    return RE_StringView_FromBytes( s->data, s->length );
}

const char *
RE_String_AsCStr( const ReString *s )
{
    return( const char * ) s->data;
}
