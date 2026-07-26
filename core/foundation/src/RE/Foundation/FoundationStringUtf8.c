/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include <RE/Foundation/FoundationStringUtf8.h>

#include <assert.h>

static b8
utf8_is_continuation_byte (u8 b)
{
    return (b8) ((b & 0xC0) == 0x80);
}

/* Does the real decode work, additionally reporting whether the sequence was well-formed (no
 * substitution) - the public utf8_decode() discards that flag (it always substitutes and
 * proceeds regardless); utf8_is_valid() is the one caller that actually needs to know.
 */
static usize
utf8_decode_ex (const u8 *bytes, usize remainingLength, u32 *outCodepoint, b8 *outWasValid)
{
    assert (remainingLength > 0);

    u8 lead = bytes[0];

    if ((lead & 0x80) == 0)
    {
        *outCodepoint = lead;
        *outWasValid  = 1;
        return 1;
    }

    usize sequenceLength;
    u32   codepoint;
    u32   minCodepoint;

    if ((lead & 0xE0) == 0xC0)
    {
        sequenceLength = 2;
        codepoint      = lead & 0x1Fu;
        minCodepoint   = 0x80;
    }
    else if ((lead & 0xF0) == 0xE0)
    {
        sequenceLength = 3;
        codepoint      = lead & 0x0Fu;
        minCodepoint   = 0x800;
    }
    else if ((lead & 0xF8) == 0xF0)
    {
        sequenceLength = 4;
        codepoint      = lead & 0x07u;
        minCodepoint   = 0x10000;
    }
    else
    {
        /* Stray continuation byte, or an obsolete/invalid lead pattern (0xF8-0xFF). */
        *outCodepoint = UTF8_REPLACEMENT_CODEPOINT;
        *outWasValid  = 0;
        return 1;
    }

    if (remainingLength < sequenceLength)
    {
        *outCodepoint = UTF8_REPLACEMENT_CODEPOINT;
        *outWasValid  = 0;
        return 1;
    }

    for (usize i = 1; i < sequenceLength; i += 1)
    {
        if (!utf8_is_continuation_byte (bytes[i]))
        {
            /* Don't consume the bad continuation byte as part of this failed sequence - only the
             * lead byte is consumed, so the next decode call resyncs starting at bytes[i].
             */
            *outCodepoint = UTF8_REPLACEMENT_CODEPOINT;
            *outWasValid  = 0;
            return 1;
        }

        codepoint = (codepoint << 6) | (bytes[i] & 0x3Fu);
    }

    b8 overlong  = (b8) (codepoint < minCodepoint);
    b8 surrogate = (b8) (codepoint >= 0xD800u && codepoint <= 0xDFFFu);
    b8 tooLarge  = (b8) (codepoint > 0x10FFFFu);

    if (overlong || surrogate || tooLarge)
    {
        *outCodepoint = UTF8_REPLACEMENT_CODEPOINT;
        *outWasValid  = 0;
        return 1;
    }

    *outCodepoint = codepoint;
    *outWasValid  = 1;
    return sequenceLength;
}

usize
utf8_decode (const u8 *bytes, usize remainingLength, u32 *outCodepoint)
{
    b8 wasValid;
    return utf8_decode_ex (bytes, remainingLength, outCodepoint, &wasValid);
}

usize
utf8_encode (u32 codepoint, u8 outBytes[4])
{
    assert (codepoint <= 0x10FFFFu && !(codepoint >= 0xD800u && codepoint <= 0xDFFFu));

    if (codepoint <= 0x7Fu)
    {
        outBytes[0] = (u8) codepoint;
        return 1;
    }

    if (codepoint <= 0x7FFu)
    {
        outBytes[0] = (u8) (0xC0u | (codepoint >> 6));
        outBytes[1] = (u8) (0x80u | (codepoint & 0x3Fu));
        return 2;
    }

    if (codepoint <= 0xFFFFu)
    {
        outBytes[0] = (u8) (0xE0u | (codepoint >> 12));
        outBytes[1] = (u8) (0x80u | ((codepoint >> 6) & 0x3Fu));
        outBytes[2] = (u8) (0x80u | (codepoint & 0x3Fu));
        return 3;
    }

    outBytes[0] = (u8) (0xF0u | (codepoint >> 18));
    outBytes[1] = (u8) (0x80u | ((codepoint >> 12) & 0x3Fu));
    outBytes[2] = (u8) (0x80u | ((codepoint >> 6) & 0x3Fu));
    outBytes[3] = (u8) (0x80u | (codepoint & 0x3Fu));
    return 4;
}

b8
utf8_is_valid (string_view sv)
{
    usize offset = 0;

    while (offset < sv.length)
    {
        u32 codepoint;
        b8  wasValid;
        offset += utf8_decode_ex (sv.data + offset, sv.length - offset, &codepoint, &wasValid);

        if (!wasValid)
        {
            return 0;
        }
    }

    return 1;
}

usize
utf8_codepoint_count (string_view sv)
{
    usize offset = 0;
    usize count  = 0;

    while (offset < sv.length)
    {
        u32 codepoint;
        offset += utf8_decode (sv.data + offset, sv.length - offset, &codepoint);
        count  += 1;
    }

    return count;
}

utf8_iterator
utf8_iterator_create (string_view sv)
{
    utf8_iterator it;
    it.view   = sv;
    it.offset = 0;

    return it;
}

b8
utf8_iterator_next (utf8_iterator *it, u32 *outCodepoint)
{
    if (it->offset >= it->view.length)
    {
        return 0;
    }

    it->offset += utf8_decode (it->view.data + it->offset, it->view.length - it->offset, outCodepoint);

    return 1;
}
