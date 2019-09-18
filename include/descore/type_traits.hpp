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
// type_traits.hpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 04/02/2010
//
// Provide some basic information about types that isn't easily obtainable
// through other means.
//
// 1. Check to see if a type is a pointer:
//
//      descore::type_traits<T>::is_pointer;
//      __is_pointer(T);
//
// 2. Check to see if a type is const:
//
//      descore::type_traits<T>::is_const;
//      __is_const(T);
//
// 3. Check to see if a type is a "bag of bits" (primitive, enum, or class/union
//    with default copy constructor + copy assignment + destructor):
//
//      descore::type_traits<T>::is_bag_of_bits;
//      __is_bag_of_bits(T);
//
// 4. Obtain a type's demangled name
//
//      descore::type_traits<T>::name();
//
// 5. Check to see if two types are the same:
//
//      __is_same_type(type1, type2);
//
// Also provide the following type operations:
//
// 1. Return a non-const version of T (or just T if T is already non-const)
//
//      __non_const_t(T)
//
////////////////////////////////////////////////////////////////////////////////

#ifndef type_traits_hpp_100402170306
#define type_traits_hpp_100402170306

#include "defines.hpp"
#include <typeinfo>

#ifndef _MSC_VER
#include <cxxabi.h>
// #include <malloc.h>
#endif

BEGIN_NAMESPACE_DESCORE

////////////////////////////////////////////////////////////////////////////////
//
// is_pointer
//
////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct is_pointer
{
    static const bool val = false;
};

template <typename T>
struct is_pointer <T *>
{
    static const bool val = true;
};

template <typename T>
struct is_pointer <T * const>
{
    static const bool val = true;
};

#define __is_pointer(...) descore::type_traits< __VA_ARGS__ >::is_pointer

////////////////////////////////////////////////////////////////////////////////
//
// is_const
//
////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct is_const
{
    static const bool val = false;
};

template <typename T>
struct is_const <const T>
{
    static const bool val = true;
};

#define __is_const(...) descore::type_traits< __VA_ARGS__ >::is_const

////////////////////////////////////////////////////////////////////////////////
//
// non_const
//
////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct non_const
{
    typedef T non_const_t;
};

template <typename T>
struct non_const <const T>
{
    typedef T non_const_t;
};

#define __non_const_t(...) descore::non_const< __VA_ARGS__ >::non_const_t

////////////////////////////////////////////////////////////////////////////////
//
// type_traits structure
//
////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct type_traits
{
    static const bool is_pointer = descore::is_pointer<T>::val;

    static const bool is_bag_of_bits = (__has_trivial_copy(T) && __has_trivial_assign(T) && __has_trivial_destructor(T)) || 
                                       (!__is_class(T) && !__is_union(T));

    static const bool is_const = descore::is_const<T>::val;

#ifndef _MSC_VER
    struct type_name
    {
        type_name (char *_name) : name(_name) {}
        ~type_name () { free(name); }
        char *name;
    };
#endif

    static const char *name ()
    {
#ifdef _MSC_VER
        return typeid(T).name();
#else
        static type_name name(abi::__cxa_demangle(typeid(T).name(), 0, 0, NULL));
        return name.name;
#endif
    }
};

#define __is_bag_of_bits(...) descore::type_traits< ... >::is_bag_of_bits

////////////////////////////////////////////////////////////////////////////////
//
// __is_same_type
//
////////////////////////////////////////////////////////////////////////////////
template <typename T1, typename T2>
struct same_type
{
    static const bool val = false;
};

template <typename T>
struct same_type<T,T>
{
    static const bool val = true;
};

#define __is_same_type(...) descore::same_type< __VA_ARGS__ >::val

END_NAMESPACE_DESCORE


#endif // #ifndef type_traits_hpp_100402170306

