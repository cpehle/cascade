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
// defines.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
// Basic types and macros
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_defines_hpp_
#define descore_defines_hpp_

/////////////////////////////////////////////////////////////////
//
// Types
//
/////////////////////////////////////////////////////////////////

typedef unsigned char uint8;
typedef signed char   int8;

#if defined _MSC_VER
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;
typedef __int16 int16;
typedef __int32 int32;
typedef __int64 int64;
typedef unsigned __int8 byte;

// Hacked together inttypes for MSVC
#define PRId8  "d"
#define PRId16 "hd"
#define PRId32 "I32d"
#define PRId64 "I64d"

#define PRIu8  "u"
#define PRIu16 "hu"
#define PRIu32 "I32u"
#define PRIu64 "I64u"

#define PRIo8  "o"
#define PRIo16 "ho"
#define PRIo32 "I32o"
#define PRIo64 "I64o"

#define PRIx8  "x"
#define PRIx16 "hx"
#define PRIx32 "I32x"
#define PRIx64 "I64x"

#define SCNd8  "d"
#define SCNd16 "hd"
#define SCNd32 "I32d"
#define SCNd64 "I64d"

#define SCNu8  "u"
#define SCNu16 "hu"
#define SCNu32 "I32u"
#define SCNu64 "I64u"

#define SCNx8  "x"
#define SCNx16 "hx"
#define SCNx32 "I32x"
#define SCNx64 "I64x"

#else
#include <stdint.h>

typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t byte;

// So we can use the C99 PRNxx and SCNxx format macros
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#endif


/////////////////////////////////////////////////////////////////
//
// Macros
//
/////////////////////////////////////////////////////////////////

//----------------------------------
// descore namespace
//----------------------------------
#define BEGIN_NAMESPACE_DESCORE namespace descore {
#define END_NAMESPACE_DESCORE }

//----------------------------------
// Anonymous namespace
//----------------------------------
#define BEGIN_NAMESPACE namespace {
#define END_NAMESPACE }

//----------------------------------
// DECLARE_NOCOPY
//----------------------------------
#define DECLARE_NOCOPY(Class) \
    private: \
    Class (const Class &rhs); \
    Class &operator= (const Class &rhs)


//----------------------------------
// STATIC_ASSERT
//----------------------------------
#define DESCORE_CONCAT2(x,y) x##y
#define CONCAT(x,y) DESCORE_CONCAT2(x, y)

#define STATIC_ASSERT(expr) static_assert((expr), #expr)

//----------------------------------
// function attributes
//----------------------------------
#ifdef _MSC_VER

#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED
#endif

#ifndef ATTRIBUTE_PRINTF
#define ATTRIBUTE_PRINTF(m)
#endif

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE __forceinline
#endif

#ifndef NEVER_INLINE
#define NEVER_INLINE __declspec(noinline)
#endif

#else

#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED  __attribute__ ((unused))
#endif

#ifndef ATTRIBUTE_PRINTF
#define ATTRIBUTE_PRINTF(m) __attribute__ ((format (printf, m, m+1)))
#endif

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE inline __attribute__ ((always_inline))
#endif

#ifndef NEVER_INLINE
#define NEVER_INLINE __attribute__ ((noinline))
#endif

#define __FUNCSIG__  __PRETTY_FUNCTION__

#endif

//----------------------------------
// __COUNTER__
//----------------------------------
#ifndef __COUNTER__
#define __COUNTER__ 0
#endif

//----------------------------------
// offsetof
//----------------------------------

// The default gcc implementation of offsetof doesn't allow the macro to be used for
// class members (only POD members) and gives a compiler error.  This alternate
// implementation will always work (as long as you steer clear of virtual inheritance),
// and is guaranteed not to collide with any other no matter what order files are included.
#define descore_offsetof(TYPE, MEMBER) ((size_t) ((&(char&)((TYPE *)8)->MEMBER)-((char*)8)))

//----------------------------------
// va_copy
//----------------------------------
#ifdef _MSC_VER
#define va_copy(dst, src) ((void)((dst) = (src)))
#endif

//----------------------------------
// Thread-local storage
//----------------------------------
#ifdef _MSC_VER
#define __thread __declspec(thread)
#endif

//----------------------------------
// string functions
//----------------------------------
#ifdef _MSC_VER
#define strtoll    _strtoi64
#define strtoull   _strtoui64
#define strcasecmp _stricmp
#define strdup     _strdup
#endif

//----------------------------------
// branch hints
//----------------------------------
#ifdef _MSC_VER
#define ASSUME_TRUE(x)  x
#define ASSUME_FALSE(x) x
#else
#define ASSUME_TRUE(x)  __builtin_expect(x, 1)
#define ASSUME_FALSE(x) __builtin_expect(x, 0)
#endif



//----------------------------------
// constants
//----------------------------------
#ifndef M_PI
#define M_PI 3.141592653589793238462643 
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.4142135623730950488016887 
#endif

#define MACRO_TO_STRING(x) MACRO_TO_STRING_(x)
#define MACRO_TO_STRING_(x) #x

#endif
