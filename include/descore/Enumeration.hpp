/*
Copyright 2011, D. E. Shaw Research.
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
// Enumeration.hpp
//
// Copyright (C) 2011 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 12/15/2011
//
// Enhance standard enumerations with automatic string conversion, minimal
// size, and values scoped by enumeration name.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef Enumeration_hpp_111215173923
#define Enumeration_hpp_111215173923

#include "strcast.hpp"

BEGIN_NAMESPACE_DESCORE

////////////////////////////////////////////////////////////////////////////////
//
// IEnumeration
//
////////////////////////////////////////////////////////////////////////////////
struct IEnumeration
{
    virtual ~IEnumeration () {}

    // Enumeration name
    virtual string getName () const = 0;

    // Convert a string to a value.  Should return -1 for an invalid string.
    virtual int getValue (const string &symbol) const = 0;

    // Convert a value to a string.
    virtual string getSymbol (int value) const = 0;

    // Return the maximum value in the enumeration
    virtual int maxValue () const = 0;

    // Return true if a value is included in the enumeration
    virtual bool isValid (int value) const = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// EnumerationType
//
// Describe a generic enumeration
//
////////////////////////////////////////////////////////////////////////////////
class EnumerationType : public IEnumeration
{
public:
    EnumerationType (const char *name, const char *values, int numValues);

    // Enumeration name
    string getName () const;

    // Convert a string to a value.  Return -1 for an invalid string.
    int getValue (const string &symbol) const;

    // Convert a value to a string.  Return "name::???" for an invalid value.
    string getSymbol (int value) const;

    // Return the maximum value in the enumeration
    int maxValue () const;

    // Return true if a value is included in the enumeration
    bool isValid (int value) const;

    // Return the set of values as a comma-delimited string
    string getValuesAsString () const;

    // Parse the value from a stream and throw a strcast_error if unsuccessful
    int parseValue (istrcaststream &is) const;

private:
    string m_name;
    string *m_values;
    int    m_numValues;
};

////////////////////////////////////////////////////////////////////////////////
//
// EnumerationValue
//
////////////////////////////////////////////////////////////////////////////////
template <int N>
struct EnumerationValue
{
    static const int Nmax = (N < 0x80) ? 0x80 : (N < 0x8000) ? 0x8000 : 0x80001;
    typedef typename EnumerationValue<Nmax>::value_t value_t;
};

template <>
struct EnumerationValue<0x80>
{
    typedef int8 value_t;
};
template<>
struct EnumerationValue<0x8000>
{
    typedef int16 value_t;
};
template<>
struct EnumerationValue<0x8001>
{
    typedef int32 value_t;
};

////////////////////////////////////////////////////////////////////////////////
//
// __is_descore_enumeration
//
////////////////////////////////////////////////////////////////////////////////
template <typename T> int __is_enumeration (typename T::is_descore_enumeration_t *);
template <typename T> char __is_enumeration (...);

#define __is_descore_enumeration(T) (sizeof(descore::__is_enumeration<T>(0)) > 1)

////////////////////////////////////////////////////////////////////////////////
//
// getEnumerationType
//
////////////////////////////////////////////////////////////////////////////////
template <typename T>
const EnumerationType *_getEnumerationType (typename T::is_descore_enumeration_t *)
{
    return T::getType();
}

template <typename T>
const EnumerationType *_getEnumerationType (...)
{
    return NULL;
}

template <typename T>
const EnumerationType *getEnumerationType ()
{
    return _getEnumerationType<T>(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// EnumerationValueType
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, bool _descore_enumeration>
struct EnumerationValueTypeInternal
{
    typedef int value_t;
};

template <typename T>
struct EnumerationValueTypeInternal<T, true>
{
    typedef typename T::value_t value_t;
};

template <typename T>
struct EnumerationValueType
{
    typedef typename EnumerationValueTypeInternal<T, __is_descore_enumeration(T)>::value_t value_t;
};

////////////////////////////////////////////////////////////////////////////////
//
// DECLARE_ENUMERATION()
//
////////////////////////////////////////////////////////////////////////////////

// GCC requires typename where MSVC prohibits it
#ifdef _MSC_VER
#define __TYPENAME_IF_GCC
#else
#define __TYPENAME_IF_GCC typename
#endif

#define DECLARE_ENUMERATION(name, ...) \
    struct name \
    { \
        enum enum_t { __VA_ARGS__ };                 \
        DECLARE_ENUMERATION_BODY(name, __VA_ARGS__); \
    }

// This macro assumes that enum_t is already defined
#define DECLARE_ENUMERATION_BODY(name, ...) \
    typedef int is_descore_enumeration_t;                               \
    struct __enum { enum { __VA_ARGS__, __size }; };                    \
    typedef __TYPENAME_IF_GCC descore::EnumerationValue<__enum::__size>::value_t value_t; \
    name () {}                                                          \
    name (enum_t _val) : val(_val) {}                                   \
    explicit name (int _val) : val(_val) {}                             \
    enum_t operator= (enum_t _val) { val = _val; return _val; }         \
    inline operator enum_t () const { return (enum_t) val; }            \
    static const descore::EnumerationType *getType ()                   \
    {                                                                   \
        static descore::EnumerationType type(#name, #__VA_ARGS__, __enum::__size); \
        return &type;                                                   \
    }                                                                   \
    void operator<< (istrcaststream &iss) { val = getType()->parseValue(iss); }       \
    void operator>> (ostrcaststream &oss) const { oss << getType()->getSymbol(val); } \
    value_t val

END_NAMESPACE_DESCORE

#endif // #ifndef Enumeration_hpp_111215173923


