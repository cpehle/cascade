/*
Copyright 2009, D. E. Shaw Research.
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

//////////////////////////////////////////////////////////////////////
//
// utils.hpp
//
// Copyright (C) 2009 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 09/15/2009
//
// Fast inline implementations for basic functions
//
//////////////////////////////////////////////////////////////////////

#ifndef utils_hpp_090915121628
#define utils_hpp_090915121628

#include "defines.hpp"

/////////////////////////////////////////////////////////////////
//
// popcount()
//
/////////////////////////////////////////////////////////////////
inline int popcount (uint8 x)
{
    // Convert x from 8 1-bit counts in the range [0-1] 
    // to 4 2-bit counts in the range [0-2]
    x -= (x >> 1) & 0x55;

    // Convert from 4 2-bit counts in the range [0-2] 
    // to 2 4-bit counts in the range [0-4]
    x = ((x >> 2) & 0x33) + (x & 0x33);

    // Convert from 2 4-bit counts in the range [0-4]
    // to a single 8-bit count in the range [0-8].
    return ((x >> 4) + x) & 0xf;
}
inline int popcount (int8 x)
{
    return popcount((uint8) x);
}
inline int popcount (char x)
{
    return popcount((uint8) x);
}

inline int popcount (uint16 x)
{
    // Convert x from 16 1-bit counts in the range [0-1] 
    // to 8 2-bit counts in the range [0-2]
    x -= (x >> 1) & 0x5555;

    // Convert from 8 2-bit counts in the range [0-2] 
    // to 4 4-bit counts in the range [0-4]
    x = ((x >> 2) & 0x3333) + (x & 0x3333);

    // Convert from 4 4-bit counts in the range [0-4]
    // to 2 8-bit counts in the range [0-8].  Note that
    // because the range [0-8] fits within 4 bits, it's
    // safe to just mask the sum instead of masking each
    // term, because the intermediate sum can't get a carry 
    // from the garbage bits [8k-1:8k-4] to the actual
    // value bits [8k+3:8k].
    x = ((x >> 4) + x) & 0x0f0f;

    // Convert from 2 8-bit counts in the range [0-8]
    // to a single 16-bit count in the range [0-32].
    return ((x >> 8) + x) & 0xff;
}
inline int popcount (int16 x)
{
    return popcount((uint16) x);
}

inline int popcount (uint32 x)
{
    // Convert x from 32 1-bit counts in the range [0-1] 
    // to 16 2-bit counts in the range [0-2]
    x -= (x >> 1) & 0x55555555;

    // Convert from 16 2-bit counts in the range [0-2] 
    // to 8 4-bit counts in the range [0-4]
    x = ((x >> 2) & 0x33333333) + (x & 0x33333333);

    // Convert from 8 4-bit counts in the range [0-4]
    // to 4 8-bit counts in the range [0-8].  Note that
    // because the range [0-8] fits within 4 bits, it's
    // safe to just mask the sum instead of masking each
    // term, because the intermediate sum can't get a carry 
    // from the garbage bits [8k-1:8k-4] to the actual
    // value bits [8k+3:8k].
    x = ((x >> 4) + x) & 0x0f0f0f0f;

    // Convert from 4 8-bit counts in the range [0-8]
    // to a single 8-bit count in the range [0-32] by
    // multiplying by 0x01010101, which causes the desired
    // sum to appear in the upper 8 bits.  Again, we don't
    // need to worry about evil carries from the right.
    return (x * 0x01010101) >> 24;
}
inline int popcount (int32 x)
{
    return popcount((uint32) x);
}

inline int popcount (uint64 x)
{
    // Convert x from 64 1-bit counts in the range [0-1] 
    // to 32 2-bit counts in the range [0-2]
    x -= (x >> 1) & 0x5555555555555555;

    // Convert from 32 2-bit counts in the range [0-2] 
    // to 16 4-bit counts in the range [0-4]
    x = ((x >> 2) & 0x3333333333333333) + (x & 0x3333333333333333);

    // Convert from 16 4-bit counts in the range [0-4]
    // to 8 8-bit counts in the range [0-8].  Note that
    // because the range [0-8] fits within 4 bits, it's
    // safe to just mask the sum instead of masking each
    // term, because the intermediate sum can't get a carry 
    // from the garbage bits [8k-1:8k-4] to the actual
    // value bits [8k+3:8k].
    x = ((x >> 4) + x) & 0x0f0f0f0f0f0f0f0f;

    // Convert from 8 8-bit counts in the range [0-8]
    // to a single 8-bit count in the range [0-64] by
    // multiplying by 0x0101010101010101, which causes the desired
    // sum to appear in the upper 8 bits.  Again, we don't
    // need to worry about evil carries from the right.
    return (int) ((x * 0x0101010101010101) >> 56);
}
inline int popcount (int64 x)
{
    return popcount((uint64) x);
}

/////////////////////////////////////////////////////////////////
//
// lsb() - lsb((uint32) 0) = 32; lsb((uint64) 0) = 64
//
/////////////////////////////////////////////////////////////////
inline int lsb (uint8 x)
{
    const uint8 lsbTable[16] = { 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 };
    int ret = 0;
    if (!(x & 0xf))
    {
        x >>= 4;
        ret = 4;
    }
    return ret + lsbTable[x & 0xf];
}
inline int lsb (int8 x)
{
    return lsb((uint8) x);
}
inline int lsb (char x)
{
    return lsb((uint8) x);
}

inline int lsb (uint16 x)
{
    int ret = 0;
    if (!(x & 0xff))
    {
        x >>= 8;
        ret = 8;
    }
    return ret + lsb((uint8) x);
}
inline int lsb (int16 x)
{
    return lsb((uint16) x);
}

inline int lsb (uint32 x)
{
    int ret = 0;
    if (!(x & 0xffff))
    {
        x >>= 16;
        ret = 16;
    }
    return ret + lsb((uint16) x);
}
inline int lsb (int32 x)
{
    return lsb((uint32) x);
}

inline int lsb (uint64 x)
{
    int ret = 0;
    uint32 y = (uint32) x;
    if (!y)
    {
        y = (uint32) (x >> 32);
        ret += 32;
    }
    return ret + lsb(y);
}
inline int lsb (int64 x)
{
    return lsb((uint64) x);
}


#endif // #ifndef utils_hpp_090915121628

