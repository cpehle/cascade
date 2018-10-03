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

//////////////////////////////////////////////////////////////////////
//
// PortTypes.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// Keep track of basic information about the bits in a type.  This is needed
// to make the Verilog ports work.
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_PortTypes_hpp_
#define Cascade_PortTypes_hpp_

#include "BitMap.hpp"

BEGIN_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// sizeInBits
//
// Actual number of bits represented by this type.  When binding
// with a Verilog port, the verilog port must match this size.
//
/////////////////////////////////////////////////////////////////
template <typename T>
struct PortSizeInBits
{
    // Size of port in bits (by default rounded up to 8 * sizeof(T), since 
    // that's all the information we get from C++).
    static const uint16 sizeInBits = 8 * sizeof(T);

    // Flag indicating whether sizeInBits is exact (true) or rounded up to 
    // 8 * sizeof(T) (false).
    static const bool exact = false;
};

#define DECLARE_PORT_SIZE(T, _sizeInBits) \
    BEGIN_NAMESPACE_CASCADE \
    template <> struct PortSizeInBits<T> \
    { \
        static const uint16 sizeInBits = _sizeInBits; \
        static const bool exact = true; \
    }; \
    END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// bitmap
//
// When dumping waves or binding to a Verilog port, this specifies 
// the mapping between C++ and Verilog.
//
/////////////////////////////////////////////////////////////////

// Default port-specific bitmap just copies the bits (DefaultBitmap is defined
// in BitMap.hpp/.cpp)
template <typename T>
struct PortBitmap
{
    static IBitmap *getBitmap ()
    {
        static DefaultBitmap s_bitmap(PortSizeInBits<T>::sizeInBits);
        return &s_bitmap;
    }
};

// Associate a port type with a bitmap type, where the bitmap type inherits
// from Cascade::IBitmap and implements the virtual functions mapCtoV and mapVtoC.
#define DECLARE_PORT_BITMAP(T, _bitmap_t) \
    BEGIN_NAMESPACE_CASCADE \
    template <> struct PortBitmap<T> \
    { \
        typedef _bitmap_t bitmap_t; \
        static IBitmap *getBitmap () \
        { \
            static bitmap_t s_bitmap; \
            return &s_bitmap; \
        } \
    }; \
    END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// value_t
//
// Actual types used to store and read port value.  The level of 
// indirection between the declared port type and the actual value
// allows different port traits to be associated with ports that are
// actually the same type.  For example, suppose a 'tag' port is 
// being modeled as an integer, but the actual width is a compile-time
// parameter in both C++ and Verilog.  Then a dummy type can be
// declared along with the appropriate traits:
//
//  struct tag_t {};
//
//  DECLARE_PORT_SIZE(tag_t, TAG_WIDTH);
//  DECLARE_PORT_VALUE_TYPE(tag_t, int);
//
// Then ports can be declared using the dummy tag_t type:
//
//  Input(tag_t,    in_tag);
//
// The additional level of indirection between the port storage type
// and the type returned by the conversion read accessor is primarily to allow
// bitvector ports to be read as their underlying integer representation.
// This allows bitvector ports to be used directly in expressions that would
// otherwise require an explicit accessor operator because the C++ compiler
// cannot automatically do two levels of conversion (bitvector port to
// bitvector to integer).
//
/////////////////////////////////////////////////////////////////
template <typename T>
struct PortValueType
{
    typedef T value_t;  // Used for port implementation and trigger type
    typedef T read_t;   // Used for implicit type conversion operator only
};

#define DECLARE_PORT_VALUE_TYPE(T, _value_t) \
    BEGIN_NAMESPACE_CASCADE \
    template <> struct PortValueType<T> \
    { \
        typedef _value_t value_t; \
        typedef _value_t read_t; \
    }; \
    END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// Bag of bits check
//
// Normally only "bag of bits" values are allowed to be ports
// because the port values are copied around as raw bits.
// This macro is required in order to use a non-bag-of-bits type
// as a port.
//
/////////////////////////////////////////////////////////////////
template <typename T>
struct PortAllowType
{
    static const bool allowed = descore::type_traits<T>::is_bag_of_bits;
};

#define ALLOW_PORT_TYPE(T) \
    BEGIN_NAMESPACE_CASCADE \
    template <> struct PortAllowType<T> \
    { \
        static const bool allowed = true; \
    }; \
    END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// Run-time structure used to store all of the above except value_t
//
/////////////////////////////////////////////////////////////////
struct PortInfo
{
    uint16  sizeInBytes;
    uint16  sizeInBits;
    bool    exact;
    IBitmap *bitmap;
    string  typeName;
};

/////////////////////////////////////////////////////////////////
//
// PortTraits
//
// Collect all of the above into a single templated structure
//
/////////////////////////////////////////////////////////////////
template <typename T> 
struct PortTraits
{
    // Use the ALLOW_PORT_TYPE() macro to suppress this assertion for
    // non-bag-of-bits port types (but make sure you really want to use
    // a non-bag-of-bits type for a port!).
    STATIC_ASSERT(PortAllowType<T>::allowed);

    typedef typename PortValueType<T>::value_t value_t;

    static const uint16 sizeInBytes   = sizeof(value_t);
    static const uint16 sizeInBits    = PortSizeInBits<T>::sizeInBits;
    static const bool   exact         = PortSizeInBits<T>::exact;
    static IBitmap *bitmap ()
    {
        return PortBitmap<T>::getBitmap();
    }

    static PortInfo getPortInfo ()
    {
        PortInfo info;
        info.sizeInBytes = sizeInBytes;
        info.sizeInBits = sizeInBits;
        info.exact = exact;
        info.bitmap = bitmap();
        info.typeName = descore::type_traits<T>::name();
        return info;
    }
};

/////////////////////////////////////////////////////////////////
//
// Obtain port info at run time
//
/////////////////////////////////////////////////////////////////

// Convert compile-time information into run-time information.
// Return a pointer to a static PortInfo object so that this pointer
// can also be used as a unique type ID for run-time type checking.
template <typename T>
inline const PortInfo *getPortInfo ()
{
    static PortInfo info = PortTraits<T>::getPortInfo();
    return &info;
};

END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// Set the port traits for FxPs
//
/////////////////////////////////////////////////////////////////

template <int L, int R> struct uFxP;
template <int L, int R> struct sFxP;

BEGIN_NAMESPACE_CASCADE

template <int L, int R>
struct PortSizeInBits<uFxP<L,R> >
{
    static const uint16 sizeInBits = L + R;
    static const bool exact = true;
};

template <int L, int R>
struct PortSizeInBits<sFxP<L,R> >
{
    static const uint16 sizeInBits = L + R;
    static const bool exact = true;
};

// Signed FxPs need custom bitmaps to do sign-extension when copying
// from Verilog to C++
template <int L, int R>
struct sFxPBitmap : public DefaultBitmap
{
    sFxPBitmap () : DefaultBitmap(L+R) {}

    void mapVtoC (byte *dst, IReadWordArray &src)
    {
        DefaultBitmap::mapVtoC(dst, src);
        sFxP<L,R> *sfxp = (sFxP<L,R> *) dst;
        sfxp->sign_extend();
    }
};

template <int L, int R>
struct PortBitmap<sFxP<L,R> >
{
    static IBitmap *getBitmap ()
    {
        static sFxPBitmap<L,R> s_bitmap;
        return &s_bitmap;
    }
};

END_NAMESPACE_CASCADE

#endif

