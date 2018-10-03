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
// BitMap.hpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 02/19/2010
//
// Support for getting/setting an arbitrary set of bits within a port.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef BitMap_hpp_100219062949
#define BitMap_hpp_100219062949

BEGIN_NAMESPACE_CASCADE

////////////////////////////////////////////////////////////////////////////////
//
// Generic read/write access to a word array.  Used to access the various
// flavours of Verilog data (2-valued or 4-valued arrays).
//
////////////////////////////////////////////////////////////////////////////////
struct IReadWordArray
{
    virtual uint32 getWord (int i) = 0;
};

struct IWriteWordArray
{
    virtual void setWord (int i, uint32 w) = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// Specialization for uint32 array
//
////////////////////////////////////////////////////////////////////////////////
struct ReadUint32Array : public IReadWordArray
{
    ReadUint32Array (const uint32 *p) : m_p(p) {}
    uint32 getWord (int i)
    {
        return m_p[i];
    }
    const uint32 *m_p;
};

struct WriteUint32Array : public IWriteWordArray
{
    WriteUint32Array (uint32 *p) : m_p(p) {}
    void setWord (int i, uint32 w)
    {
        m_p[i] = w;
    }
    uint32 *m_p;
};

////////////////////////////////////////////////////////////////////////////////
//
// Generic bitmap
//
////////////////////////////////////////////////////////////////////////////////
struct IBitmap
{
    // Form the Verilog value by writing directly to a uint32 array
    void mapCtoV (uint32 *dst, const byte *src)
    {
        WriteUint32Array w(dst);
        mapCtoV(w, src);
    }

    // Obtain the Verilog value by reading directly from a uint32 array
    void mapVtoC (byte *dst, const uint32 *src)
    {
        ReadUint32Array r(src);
        mapVtoC(dst, r);
    }

    // Copy to/from a generic array by supplying an IWriteWordArra/IReadWordArray.
    // These functions must be implemented for any custom bitmap.
    virtual void mapCtoV (IWriteWordArray &dst, const byte *src) = 0;
    virtual void mapVtoC (byte *dst, IReadWordArray &src) = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// Default bitmap (just copy all the bits)
//
////////////////////////////////////////////////////////////////////////////////
struct DefaultBitmap : public IBitmap
{
    DefaultBitmap (int sizeInBits) : m_sizeInBits(sizeInBits) {}

    void mapCtoV (IWriteWordArray &dst, const byte *src);
    void mapVtoC (byte *dst, IReadWordArray &src);

    int m_sizeInBits;
};

END_NAMESPACE_CASCADE

#endif // #ifndef BitMap_hpp_100219062949

