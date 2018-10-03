/*
Copyright 2010, D. E. Shaw Research.
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
// BitMap.cpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 02/19/2010
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SimDefs.hpp"
#include "BitMap.hpp"
#include "PortTypes.hpp"

BEGIN_NAMESPACE_CASCADE

////////////////////////////////////////////////////////////////////////////////
//
// DefaultBitmap
//
////////////////////////////////////////////////////////////////////////////////
void DefaultBitmap::mapCtoV (IWriteWordArray &dst, const byte *src)
{
    union
    {
        uint32 dst32;
        byte dst8[4];
    };

    int isrc = 0;
    int idst = 0;
    dst32 = 0;
    int numBytes = m_sizeInBits / 8;
    for ( ; isrc < numBytes ; isrc++)
    {
        dst8[isrc&3] = src[isrc];
        if ((isrc&3) == 3)
        {
            dst.setWord(idst++, dst32);
            dst32 = 0;
        }
    }
    int remainingBits = m_sizeInBits & 7;
    if (remainingBits)
    {
        byte mask = (1 << remainingBits) - 1;
        dst8[isrc&3] = src[isrc] & mask;
        dst.setWord(idst, dst32);
    }
    else if (isrc&3)
        dst.setWord(idst, dst32);
}

void DefaultBitmap::mapVtoC (byte *dst, IReadWordArray &src)
{
    union
    {
        uint32 src32;
        byte src8[4];
    };

    int isrc = 0;
    int idst = 0;
    int numBytes = m_sizeInBits / 8;
    for ( ; idst < numBytes ; idst++)
    {
        if (!(idst&3))
            src32 = src.getWord(isrc++);
        dst[idst] = src8[idst&3];
    }
    int remainingBits = m_sizeInBits & 7;
    if (remainingBits)
    {
        if (!(idst&3))
            src32 = src.getWord(isrc);
        byte mask = (1 << remainingBits) - 1;
        dst[idst] ^= (dst[idst] ^ src8[idst&3]) & mask;
    }
}

END_NAMESPACE_CASCADE
