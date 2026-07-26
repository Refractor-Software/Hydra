/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationStringUtf8.h>

#include <assert.h>

internal ReBool
Utf8_IsContinuationByte( ReUint8 b )
{
    return( ReBool ) ((b & 0xC0) == 0x80);
}

/* Does the real decode work, additionally reporting whether the sequence was well-formed (no
 * substitution) - the public RE_Utf8_Decode() discards that flag (it always substitutes and
 * proceeds regardless); RE_Utf8_IsValid() is the one caller that actually needs to know.
 */
internal ReUint64
Utf8_DecodeEx( const ReUint8 *bytes, ReUint64 remainingLength, ReUint32 *outCodepoint, ReBool *outWasValid )
{
    assert( remainingLength > 0 );

    ReUint8 lead = bytes[0];

    if ( (lead & 0x80) == 0 )
    {
        *outCodepoint = lead;
        *outWasValid  = RE_True;
        return 1;
    }

    ReUint64 sequenceLength;
    ReUint32   codepoint;
    ReUint32   minCodepoint;

    if ( (lead & 0xE0) == 0xC0 )
    {
        sequenceLength = 2;
        codepoint      = lead & 0x1Fu;
        minCodepoint   = 0x80;
    }
    else if ( (lead & 0xF0) == 0xE0 )
    {
        sequenceLength = 3;
        codepoint      = lead & 0x0Fu;
        minCodepoint   = 0x800;
    }
    else if ( (lead & 0xF8) == 0xF0 )
    {
        sequenceLength = 4;
        codepoint      = lead & 0x07u;
        minCodepoint   = 0x10000;
    }
    else
    {
        /* Stray continuation byte, or an obsolete/invalid lead pattern (0xF8-0xFF). */
        *outCodepoint = RE_UTF8_REPLACEMENT_CODEPOINT;
        *outWasValid  = RE_False;
        return 1;
    }

    if ( remainingLength < sequenceLength )
    {
        *outCodepoint = RE_UTF8_REPLACEMENT_CODEPOINT;
        *outWasValid  = RE_False;
        return 1;
    }

    for ( ReUint64 i = 1; i < sequenceLength; i += 1 )
    {
        if ( !Utf8_IsContinuationByte( bytes[i] ) )
        {
            /* Don't consume the bad continuation byte as part of this failed sequence - only the
             * lead byte is consumed, so the next decode call resyncs starting at bytes[i].
             */
            *outCodepoint = RE_UTF8_REPLACEMENT_CODEPOINT;
            *outWasValid  = RE_False;
            return 1;
        }

        codepoint = (codepoint << 6) | (bytes[i] & 0x3Fu);
    }

    ReBool overlong  = (ReBool) (codepoint < minCodepoint);
    ReBool surrogate = (ReBool) (codepoint >= 0xD800u && codepoint <= 0xDFFFu);
    ReBool tooLarge  = (ReBool) (codepoint > 0x10FFFFu);

    if ( overlong || surrogate || tooLarge )
    {
        *outCodepoint = RE_UTF8_REPLACEMENT_CODEPOINT;
        *outWasValid  = RE_False;
        return 1;
    }

    *outCodepoint = codepoint;
    *outWasValid  = RE_True;
    return sequenceLength;
}

ReUint64
RE_Utf8_Decode( const ReUint8 *bytes, ReUint64 remainingLength, ReUint32 *outCodepoint )
{
    ReBool wasValid;
    return Utf8_DecodeEx( bytes, remainingLength, outCodepoint, &wasValid );
}

ReUint64
RE_Utf8_Encode( ReUint32 codepoint, ReUint8 outBytes[4] )
{
    assert( codepoint <= 0x10FFFFu && !(codepoint >= 0xD800u && codepoint <= 0xDFFFu) );

    if ( codepoint <= 0x7Fu )
    {
        outBytes[0] = (ReUint8) codepoint;
        return 1;
    }

    if ( codepoint <= 0x7FFu )
    {
        outBytes[0] = (ReUint8) (0xC0u | (codepoint >> 6));
        outBytes[1] = (ReUint8) (0x80u | (codepoint & 0x3Fu));
        return 2;
    }

    if ( codepoint <= 0xFFFFu )
    {
        outBytes[0] = (ReUint8) (0xE0u | (codepoint >> 12));
        outBytes[1] = (ReUint8) (0x80u | ((codepoint >> 6) & 0x3Fu));
        outBytes[2] = (ReUint8) (0x80u | (codepoint & 0x3Fu));
        return 3;
    }

    outBytes[0] = (ReUint8) (0xF0u | (codepoint >> 18));
    outBytes[1] = (ReUint8) (0x80u | ((codepoint >> 12) & 0x3Fu));
    outBytes[2] = (ReUint8) (0x80u | ((codepoint >> 6) & 0x3Fu));
    outBytes[3] = (ReUint8) (0x80u | (codepoint & 0x3Fu));
    return 4;
}

ReBool
RE_Utf8_IsValid( ReStringView sv )
{
    ReUint64 offset = 0;

    while ( offset < sv.length )
    {
        ReUint32 codepoint;
        ReBool  wasValid;
        offset += Utf8_DecodeEx( sv.data + offset, sv.length - offset, &codepoint, &wasValid );

        if ( !wasValid )
        {
            return RE_False;
        }
    }

    return RE_True;
}

ReUint64
RE_Utf8_CodepointCount( ReStringView sv )
{
    ReUint64 offset = 0;
    ReUint64 count  = 0;

    while ( offset < sv.length )
    {
        ReUint32 codepoint;
        offset += RE_Utf8_Decode( sv.data + offset, sv.length - offset, &codepoint );
        count  += 1;
    }

    return count;
}

ReUtf8Iterator
RE_Utf8_IteratorCreate( ReStringView sv )
{
    ReUtf8Iterator it;
    it.view   = sv;
    it.offset = 0;

    return it;
}

ReBool
RE_Utf8_IteratorNext( ReUtf8Iterator *it, ReUint32 *outCodepoint )
{
    if ( it->offset >= it->view.length )
    {
        return RE_False;
    }

    it->offset += RE_Utf8_Decode( it->view.data + it->offset, it->view.length - it->offset, outCodepoint );

    return RE_True;
}
