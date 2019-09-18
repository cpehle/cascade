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
// Iterators.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
// Encapsulate stl iterators.
//
//
// Sample usage for non-map containers:
//
//    typedef std::set<int> IntSet;
//    IntSet ids;
//    ...
//    for (Iterator<IntSet> it = ids ; it ; it++)
//        log("id = %d\n", *ids);
//
//
// Sample usage for map containers:
//
//    typedef std::map<int, string> NameMap;
//    NameMap names;
//    ...
//    for (Iterator<NameMap> it = names ; it ; it++)
//        log("names[%d] = %s\n", it.key(), it->c_str());
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_Iterators_hpp_
#define descore_Iterators_hpp_

#include "defines.hpp"
#include <map>
#include <set>

/////////////////////////////////////////////////////////////////
//
// Compile time functions to retrieve KeyType/ValueType
//
/////////////////////////////////////////////////////////////////
template <typename Container>
struct ContainerTypes
{
    typedef typename Container::value_type KeyType;
    typedef typename Container::value_type ValueType;
};

template <class _Kty, class _Ty, class _Pr, class _Alloc>
struct ContainerTypes<std::map<_Kty, _Ty, _Pr, _Alloc> >
{
    typedef _Kty KeyType;
    typedef _Ty  ValueType;
};

template <class _Kty, class _Ty, class _Pr, class _Alloc>
struct ContainerTypes<std::multimap<_Kty, _Ty, _Pr, _Alloc> >
{
    typedef _Kty KeyType;
    typedef _Ty  ValueType;
};

template <typename T, int n>
struct ContainerTypes<T[n]>
{
    typedef int KeyType;
    typedef T   ValueType;
};

template <>
struct ContainerTypes<const char *>
{
    typedef void        KeyType;
    typedef const char *ValueType;
};

template <>
struct ContainerTypes<char *>
{
    typedef void  KeyType;
    typedef char *ValueType;
};

////////////////////////////////////////////////////////////////////////////////
//
// Base struct for all iterators used to implement descore_foreach
//
////////////////////////////////////////////////////////////////////////////////
struct IteratorBase {};

/////////////////////////////////////////////////////////////////
//
// Iterator
//
/////////////////////////////////////////////////////////////////
template <typename Container, 
    typename ValueType = typename ContainerTypes<Container>::ValueType,
    typename KeyType = typename ContainerTypes<Container>::KeyType>
class Iterator : public IteratorBase
{
public:

    // Construction
    Iterator ()
    {
        m_curr = m_end;
    }
    Iterator (Container &container)
    {
        m_curr = container.begin();
        m_end = container.end();
    }

    // Assignment
    Iterator &operator= (Container &rhs)
    {
        m_curr = rhs.begin();
        m_end = rhs.end();
        return *this;
    }

    // Iterator accessors
    inline ValueType &operator* () const
    {
        return *m_curr;
    }
    inline ValueType *operator-> () const
    {
        return m_curr.operator->();
    }
    inline void operator++ ()
    {
        m_curr++;
    }
    inline void operator++ (int)
    {
        m_curr++;
    }
    inline operator bool () const
    {
        return m_curr != m_end;
    }

private:
    typename Container::iterator m_curr;
    typename Container::iterator m_end;
};

/////////////////////////////////////////////////////////////////
//
// ConstIterator
//
/////////////////////////////////////////////////////////////////
template <typename Container, 
    typename ValueType = typename ContainerTypes<Container>::ValueType,
    typename KeyType = typename ContainerTypes<Container>::KeyType>
class ConstIterator : public IteratorBase
{
public:

    // Construction
    ConstIterator ()
    {
        m_curr = m_end;
    }
    ConstIterator (const Container &container)
    {
        m_curr = container.begin();
        m_end = container.end();
    }
    ConstIterator (const Container &container, const ValueType &lower, const ValueType &upper)
    {
        m_curr = container.lower_bound(lower);
        m_end = container.upper_bound(upper);
    }

    // Assignment
    ConstIterator &operator= (const Container &rhs)
    {
        m_curr = rhs.begin();
        m_end = rhs.end();
        return *this;
    }

    // Iterator accessors
    inline const ValueType &operator* () const
    {
        return *m_curr;
    }
    inline const ValueType *operator-> () const
    {
        return m_curr.operator->();
    }
    inline void operator++ ()
    {
        m_curr++;
    }
    inline void operator++ (int)
    {
        m_curr++;
    }
    inline operator bool () const
    {
        return m_curr != m_end;
    }

private:
    typename Container::const_iterator m_curr;
    typename Container::const_iterator m_end;
};

/////////////////////////////////////////////////////////////////
//
// Need to special case Iterator<set> and Iterator<multiset>
// because these iterators must always be const.
//
/////////////////////////////////////////////////////////////////
template <class _Kty, class _Pr, class _Alloc, typename ValueType, typename KeyType>
class Iterator<std::set<_Kty, _Pr, _Alloc>, ValueType, KeyType> : public ConstIterator<std::set<_Kty, _Pr, _Alloc>, ValueType, KeyType>
{
    typedef std::set<_Kty, _Pr, _Alloc> Container;
    typedef ConstIterator<Container, ValueType, KeyType> BaseIterator;
public:
    // Construction
    Iterator () {}
    Iterator (const Container &container) : BaseIterator(container) {}
    Iterator (const Container &container, const ValueType &lower, const ValueType &upper) : BaseIterator(container, lower, upper) {}

    // Assignment
    Iterator &operator= (const Container &rhs)
    {
        BaseIterator::operator= (rhs);
        return *this;
    }
};

template <class _Kty, class _Pr, class _Alloc, typename ValueType, typename KeyType>
class Iterator<std::multiset<_Kty, _Pr, _Alloc>, ValueType, KeyType> : public ConstIterator<std::multiset<_Kty, _Pr, _Alloc>, ValueType, KeyType>
{
    typedef std::multiset<_Kty, _Pr, _Alloc> Container;
    typedef ConstIterator<Container, ValueType, KeyType> BaseIterator;
public:
    // Construction
    Iterator () : BaseIterator() {}
    Iterator (const Container &container) : BaseIterator(container) {}
    Iterator (const Container &container, const ValueType &lower, const ValueType &upper) : BaseIterator(container, lower, upper) {}

    // Assignment
    Iterator &operator= (const Container &rhs)
    {
        BaseIterator::operator= (rhs);
        return *this;
    }
};

/////////////////////////////////////////////////////////////////
//
// MapIterator
//
/////////////////////////////////////////////////////////////////
template <typename Container, typename ValueType = typename Container::mapped_type, typename KeyType = typename Container::key_type>
class MapIterator : public IteratorBase 
{
public:

    // Construction
    MapIterator ()
    {
        m_curr = m_end;
    }
    MapIterator (Container &container)
    {
        m_curr = container.begin();
        m_end = container.end();
    }
    MapIterator (Container &container, const KeyType &lower, const KeyType &upper)
    {
        m_curr = container.lower_bound(lower);
        m_end = container.upper_bound(upper);
    }

    // Assignment
    MapIterator &operator= (Container &rhs)
    {
        m_curr = rhs.begin();
        m_end = rhs.end();
        return *this;
    }

    // Iterator accessors
    inline const KeyType &key () const
    {
        return m_curr->first;
    }
    inline ValueType &operator* () const
    {
        return m_curr->second;
    }
    inline ValueType *operator-> () const
    {
        return &m_curr->second;
    }
    inline void operator++ ()
    {
        m_curr++;
    }
    inline void operator++ (int)
    {
        m_curr++;
    }
    inline operator bool () const
    {
        return m_curr != m_end;
    }

private:
    typename Container::iterator m_curr;
    typename Container::iterator m_end;
};

/////////////////////////////////////////////////////////////////
//
// ConstMapIterator
//
/////////////////////////////////////////////////////////////////
template <typename Container, typename ValueType = typename Container::mapped_type, typename KeyType = typename Container::key_type>
class ConstMapIterator : public IteratorBase 
{
public:

    // Construction
    ConstMapIterator ()
    {
        m_curr = m_end;
    }
    ConstMapIterator (const Container &container)
    {
        m_curr = container.begin();
        m_end = container.end();
    }
    ConstMapIterator (const Container &container, const KeyType &lower, const KeyType &upper)
    {
        m_curr = container.lower_bound(lower);
        m_end = container.upper_bound(upper);
    }

    // Assignment
    ConstMapIterator &operator= (const Container &rhs)
    {
        m_curr = rhs.begin();
        m_end = rhs.end();
        return *this;
    }

    // Iterator accessors
    inline const KeyType &key () const
    {
        return m_curr->first;
    }
    inline const ValueType &operator* () const
    {
        return m_curr->second;
    }
    inline const ValueType *operator-> () const
    {
        return &m_curr->second;
    }
    inline void operator++ ()
    {
        m_curr++;
    }
    inline void operator++ (int)
    {
        m_curr++;
    }
    inline operator bool () const
    {
        return m_curr != m_end;
    }

private:
    typename Container::const_iterator m_curr;
    typename Container::const_iterator m_end;
};

/////////////////////////////////////////////////////////////////
//
// Specialize Iterator<map> and Iterator<multimap>
//
/////////////////////////////////////////////////////////////////
template <class _Kty, class _Ty, class _Pr, class _Alloc, typename ValueType, typename KeyType>
class Iterator<std::map<_Kty, _Ty, _Pr, _Alloc>, ValueType, KeyType> : public MapIterator<std::map<_Kty, _Ty, _Pr, _Alloc>, ValueType, KeyType>
{
    typedef std::map<_Kty, _Ty, _Pr, _Alloc> Container;
    typedef MapIterator<Container, ValueType, KeyType> BaseIterator;
public:
    // Construction
    Iterator () : BaseIterator () {}
    Iterator (Container &container) : BaseIterator(container) {}
    Iterator (Container &container, const KeyType &lower, const KeyType &upper) : BaseIterator(container, lower, upper) {}

    // Assignment
    Iterator &operator= (Container &rhs)
    {
        BaseIterator::operator= (rhs);
        return *this;
    }
};

template <class _Kty, class _Ty, class _Pr, class _Alloc, typename ValueType, typename KeyType>
class Iterator<std::multimap<_Kty, _Ty, _Pr, _Alloc>, ValueType, KeyType> : public MapIterator<std::multimap<_Kty, _Ty, _Pr, _Alloc>, ValueType, KeyType>
{
    typedef std::multimap<_Kty, _Ty, _Pr, _Alloc> Container;
    typedef MapIterator<Container, ValueType, KeyType> BaseIterator;
public:
    // Construction
    Iterator () : BaseIterator () {}
    Iterator (Container &container) : BaseIterator(container) {}
    Iterator (Container &container, const KeyType &lower, const KeyType &upper) : BaseIterator(container, lower, upper) {}

    // Assignment
    Iterator &operator= (Container &rhs)
    {
        BaseIterator::operator= (rhs);
        return *this;
    }
};

template <class _Kty, class _Ty, class _Pr, class _Alloc, typename ValueType, typename KeyType>
class ConstIterator<std::map<_Kty, _Ty, _Pr, _Alloc>, ValueType, KeyType> : public ConstMapIterator<std::map<_Kty, _Ty, _Pr, _Alloc>, ValueType, KeyType>
{
    typedef std::map<_Kty, _Ty, _Pr, _Alloc> Container;
    typedef ConstMapIterator<Container, ValueType, KeyType> BaseIterator;
public:
    // Construction
    ConstIterator () : BaseIterator () {}
    ConstIterator (const Container &container) : BaseIterator(container) {}
    ConstIterator (const Container &container, const KeyType &lower, const KeyType &upper) : BaseIterator(container, lower, upper) {}

    // Assignment
    ConstIterator &operator= (const Container &rhs)
    {
        BaseIterator::operator= (rhs);
        return *this;
    }
};

template <class _Kty, class _Ty, class _Pr, class _Alloc, typename ValueType, typename KeyType>
class ConstIterator<std::multimap<_Kty, _Ty, _Pr, _Alloc>, ValueType, KeyType> : public ConstMapIterator<std::multimap<_Kty, _Ty, _Pr, _Alloc>, ValueType, KeyType>
{
    typedef std::multimap<_Kty, _Ty, _Pr, _Alloc> Container;
    typedef ConstMapIterator<Container, ValueType, KeyType> BaseIterator;
public:
    // Construction
    ConstIterator () : BaseIterator () {}
    ConstIterator (const Container &container) : BaseIterator(container) {}
    ConstIterator (const Container &container, const KeyType &lower, const KeyType &upper) : BaseIterator(container, lower, upper) {}

    // Assignment
    ConstIterator &operator= (const Container &rhs)
    {
        BaseIterator::operator= (rhs);
        return *this;
    }
};

////////////////////////////////////////////////////////////////////////////////
//
// Array iterators
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, int n>
class Iterator<T[n], T, int> : public IteratorBase
{
public:

    // Construction
    Iterator ()
    {
        m_curr = m_end;
    }
    Iterator (T array[n])
    {
        m_curr = array;
        m_end = array + n;
    }

    // Assignment
    Iterator &operator= (T array[n])
    {
        m_curr = array;
        m_end = array + n;
        return *this;
    }

    // Iterator accessors
    inline T &operator* () const
    {
        return *m_curr;
    }
    inline T *operator-> () const
    {
        return m_curr; //.operator->();
    }
    inline void operator++ ()
    {
        m_curr++;
    }
    inline void operator++ (int)
    {
        m_curr++;
    }
    inline operator bool () const
    {
        return m_curr != m_end;
    }

private:
    T *m_curr;
    T *m_end;
};

template <typename T, int n>
class ConstIterator<T[n], T, int> : public IteratorBase
{
public:

    // Construction
    ConstIterator ()
    {
        m_curr = m_end;
    }
    ConstIterator (const T array[n])
    {
        m_curr = array;
        m_end = array + n;
    }

    // Assignment
    ConstIterator &operator= (const T array[n])
    {
        m_curr = array;
        m_end = array + n;
        return *this;
    }

    // Iterator accessors
    inline const T &operator* () const
    {
        return *m_curr;
    }
    inline const T *operator-> () const
    {
        return m_curr; //.operator->();
    }
    inline void operator++ ()
    {
        m_curr++;
    }
    inline void operator++ (int)
    {
        m_curr++;
    }
    inline operator bool () const
    {
        return m_curr != m_end;
    }

private:
    const T *m_curr;
    const T *m_end;
};

////////////////////////////////////////////////////////////////////////////////
//
// String iterators
//
////////////////////////////////////////////////////////////////////////////////
template<>
class Iterator<char *> : public IteratorBase
{
public:
    // Construction
    Iterator () : m_curr(0) 
    {
    }
    Iterator (char *ch) : m_curr(ch)
    {
    }

    // Assignment
    Iterator &operator= (char *ch)
    {
        m_curr = ch;
        return *this;
    }

    // Iterator accessors
    inline char &operator* ()
    {
        return *m_curr;
    }
    inline void operator++ ()
    {
        m_curr++;
    }
    inline void operator++ (int)
    {
        m_curr++;
    }
    inline operator bool () const
    {
        return *m_curr != 0;
    }

protected:
    char *m_curr;
};

template<>
class Iterator<const char *> : public IteratorBase
{
public:
    // Construction
    Iterator () : m_curr(0) 
    {
    }
    Iterator (const char *ch) : m_curr(ch)
    {
    }

    // Assignment
    Iterator &operator= (const char *ch)
    {
        m_curr = ch;
        return *this;
    }

    // Iterator accessors
    inline char operator* () const
    {
        return *m_curr;
    }
    inline void operator++ ()
    {
        m_curr++;
    }
    inline void operator++ (int)
    {
        m_curr++;
    }
    inline operator bool () const
    {
        return *m_curr != 0;
    }

protected:
    const char *m_curr;
};

template<>
class ConstIterator<char *> : public Iterator<const char *>
{
public:
    // Construction
    ConstIterator () {}
    ConstIterator (const char *ch) : Iterator<const char *>(ch) {}

    // Assignment
    ConstIterator &operator= (const char *ch)
    {
        this->m_curr = ch;
        return *this;
    }
};

template<>
class ConstIterator<const char *> : public Iterator<const char *>
{
public:
    // Construction
    ConstIterator () {}
    ConstIterator (const char *ch) : Iterator<const char *>(ch) {}

    // Assignment
    ConstIterator &operator= (const char *ch)
    {
        this->m_curr = ch;
        return *this;
    }
};

#endif
