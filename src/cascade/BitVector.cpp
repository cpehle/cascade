/*
Copyright 2007, D. E. Shaw Research.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions, and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions, and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of D. E. Shaw Research nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

////////////////////////////////////////////////////////////////////////////////
//
// BitVector.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 05/15/2007
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "BitVector.hpp"
#include <descore/hex.hpp>

////////////////////////////////////////////////////////////////////////////////
//
// bv_str_hex_internal()
//
////////////////////////////////////////////////////////////////////////////////
string bv_str_hex_internal (const byte *data, int len)
{
    int numDigits = (len + 3) / 4;
    char *sz = new char[numDigits + 3];
    sz[0] = '0';
    sz[1] = 'x';
    sz[numDigits + 2] = 0;
    char *dst = sz + 2;

    // First byte
    data += (len - 1) / 8;
    int len0 = 1 + ((len - 1) & 7);
    byte b = *data-- & ((1 << len0) - 1);
    if ((len - 1) & 4)
        *dst++ = descore::hex_to_ch[b >> 4];
    *dst++ = descore::hex_to_ch[b & 15];
    len -= len0;

    // Remaining bytes
    for ( ; len ; len -= 8)
    {
        b = *data--;
        *dst++ = descore::hex_to_ch[b >> 4];
        *dst++ = descore::hex_to_ch[b & 15];
    }

    string ret = sz;
    delete[] sz;
    return ret;
}


////////////////////////////////////////////////////////////////////////////////
//
// bv_str_bits()
//
////////////////////////////////////////////////////////////////////////////////
string bv_str_bits (const byte *data, int shift, int len)
{
    char *sz = new char[len + 1];
    sz[len] = 0;
    while (len--)
    {
        sz[len] = '0' + ((*data >> shift) & 1);
        if (shift++ == 7)
        {
            shift = 0;
            data++;
        }
    }
    string ret = sz;
    delete[] sz;
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//
// bv_parseHex()
//
////////////////////////////////////////////////////////////////////////////////
static int bv_parseHex (int c)
{
    int ret = descore::ch_to_hex[c];
    if (ret == -1)
        throw strcast_error("'%c' is not a valid hex digit", c);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//
// bv_fromStringInternal()
//
////////////////////////////////////////////////////////////////////////////////
void bv_fromStringInternal (byte *data, int len, const char *s)
{
    memset(data, 0, (len + 7) / 8);

    // Skip leading whitespace
    const char *ch = s;
    for ( ; *ch && *ch <= ' ' ; ch++);

    // Skip 0x
    if (ch[0] == '0' && ch[1] == 'x')
        ch += 2;

    // Can't be empty at this point
    if (!*ch)
        throw strcast_error("'%s' is not a valid hex string", s);

    // Skip leading zeros
    for ( ; *ch == '0' ; ch++);
    if (!*ch)
        return;

    // Validate and maybe parse the first nibble
    int numDigits = (int) strlen(ch);
    int maxDigits = (len + 3) / 4;
    int bound = 2 << ((len - 1) & 3);
    int b = bv_parseHex(*ch);
    if ((numDigits > maxDigits) || ((numDigits == maxDigits) && (b >= bound)))
        throw strcast_error("'%s' has more than %d bits", s, len);
    if (numDigits & 1)
    {
        data[numDigits-- / 2] = b;
        ch++;
    }

    // Parse the reset
    for (int i = numDigits / 2 - 1 ; i >= 0 ; i--)
    {
        b = bv_parseHex(*ch++) << 4;
        b |= bv_parseHex(*ch++);
        data[i] = b;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// bv_fromBitStringInternal()
//
////////////////////////////////////////////////////////////////////////////////
void bv_fromBitStringInternal (byte *data, int len, const char *s)
{
    memset(data, 0, (len + 7) / 8);

    // Skip leading whitespace
    const char *ch = s;
    for ( ; *ch && *ch <= ' ' ; ch++);

    // Can't be empty at this point
    if (!*ch)
        throw strcast_error("'%s' is not a valid binary string", s);

    // Skip leading zeros
    for ( ; *ch == '0' ; ch++);
    if (!*ch)
        return;

    // Validate
    int numDigits = (int) strlen(ch);
    if (numDigits > len)
        throw strcast_error("'%s' has more than %d bits", s, len);

    // Parse
    int shift = ((len - 1) & 7) - (len - numDigits);
    data += (len - 1) / 8;
    while (shift < 0)
    {
        shift += 8;
        data--;
    }
    while (numDigits--)
    {
        int currbit = *ch++ - '0';
        if (currbit)
        {
            if (currbit != 1)
                throw strcast_error("Could not parse bit string '%s'", s);
            *data |= 1 << shift;
        }
        if (!shift--)
        {
            shift = 7;
            data--;
        }
    }
}

