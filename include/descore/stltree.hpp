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

//////////////////////////////////////////////////////////////////////
//
// stltree.hpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 07/19/2010
//
// Address some issues with the STL tree containers
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_stltree_hpp
#define descore_stltree_hpp

namespace std
{
    template <typename T, typename A> class list;
    template <typename T, typename A> class vector;
    template <typename T, typename A> class deque;
    template <typename T1, typename T2> struct pair;
    template <typename K, typename P, typename A> class set;
    template <typename K, typename P, typename A> class multiset;
    template <typename K, typename T, typename P, typename A> class map;
    template <typename K, typename T, typename P, typename A> class multimap;
};

//////////////////////////////////////////////////////////////////////
//
// Trees of pointers
//
//////////////////////////////////////////////////////////////////////

// By default, don't allow STL trees indexed by a pointer since this is a 
// common source of non-determinism.
namespace std
{
    template <typename T> struct REFER_TO_DESCORE_STLTREE_HPP {};
    template <typename T> struct less <T *> 
    {
        typedef typename REFER_TO_DESCORE_STLTREE_HPP<T *>::INVALID_STL_TREE_INDEXED_BY_POINTER _;
    };
}

// If an STL tree indexed by a pointer is necessary, the above static assertion
// can be avoided by:
//
//  (1) Manually defining a custom std::less<T*> for the pointer type T*, or
//  (2) Specifying descore::allow_ptr<T*> as the last template argument, e.g.
//
//        std::map<void *, Node *, descore::allow_ptr<void *> >
//
//  (3) Defining std::less<T*> using the macro DESCORE_ALLOW_STL_TREE_TYPE(T*), e.g.
//
//        DESCORE_ALLOW_STL_TREE_TYPE(MyNodeType *);
//
BEGIN_NAMESPACE_DESCORE
template <typename T> struct allow_ptr;
template <typename T> struct allow_ptr <T *>
{ 
    bool operator() (const T *lhs, const T *rhs) const
    { 
        return lhs < rhs;
    } 
};
template <typename T> struct allow_ptr <const T *>
{ 
    bool operator() (const T *lhs, const T *rhs) const
    { 
        return lhs < rhs;
    } 
};
END_NAMESPACE_DESCORE

#define DESCORE_ALLOW_STL_TREE_TYPE(T) \
    namespace std \
    { \
        template <> struct less<T> : public descore::allow_ptr<T> {}; \
    }

// Exception for char *, required by std::basic_string
DESCORE_ALLOW_STL_TREE_TYPE(const char *);

// Exceptions for boost
namespace boost
{
    namespace detail
    {
        class sp_counted_base;
    }
    namespace system
    {
        class error_category;
    }
}
DESCORE_ALLOW_STL_TREE_TYPE(boost::detail::sp_counted_base *);
DESCORE_ALLOW_STL_TREE_TYPE(const boost::system::error_category *);

//////////////////////////////////////////////////////////////////////
//
// Comparison of trees
//
//////////////////////////////////////////////////////////////////////

// The default comparison operators for sets and maps use operator<
// to compare individual elements instead of std::less<T>.  This 
// produces surprising and inconsistent results, and defeats the intention
// of std::less<> specializations.  So, don't allow this operators.
// Use descore::lt, descore::gt, descore::lte, descore::gte instead.
//
// Additionally, comparison operators for multimaps do not produce
// correct results because they don't account for the fact that multiple
// instances of a given key can appear in any order.
//
// Finally, the equality operators (==, !=) on sets and maps require
// the existence of operator== for the contained types, whereas the sets
// and maps themselves only require the existence of std::less<>.
// Use descore::eq and descore::neq when operator== is unavailable
// for the index type.

BEGIN_NAMESPACE_DESCORE

template <typename P, typename IT>
bool stl_tree_less (IT it1, IT end1, IT it2, IT end2)
{
    P lt;
    for ( ; it1 != end1 ; it1++, it2++)
    {
        if (it2 == end2)
            return false;
        if (lt(*it1, *it2))
            return true;
        if (lt(*it2, *it1))
            return false;
    }
    return (it2 != end2);
}

template <typename P, typename IT>
bool stl_tree_eq (IT it1, IT end1, IT it2, IT end2)
{
    P lt;
    for ( ; it1 != end1 ; it1++, it2++)
    {
        if (it2 == end2)
            return false;
        if (lt(*it1, *it2) || lt(*it2, *it1))
            return false;
    }
    return (it2 == end2);
}

template <typename T1, typename T2, typename P1, typename P2>
struct pair_less
{
    bool operator() (const std::pair<T1, T2> &lhs, const std::pair<T1, T2> &rhs) const
    {
        P1 lt1;
        P2 lt2;
        return lt1(lhs.first, rhs.first) ? true :
            lt1(rhs.first, lhs.first) ? false :
            lt2(lhs.second, rhs.second);
    }
};

// These are implemented below
template <typename K, typename T, typename P, typename A>
bool multimap_less (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs);

template <typename K, typename T, typename P, typename A>
bool multimap_eq (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs);

END_NAMESPACE_DESCORE

//
// These implementations of the comparision operators will either work properly or
// generate compile-time errors due to ambigous overloads.
//

// set
template <typename K, typename P, typename A>
inline bool operator< (const std::set<K, P, A> &lhs, const std::set<K, P, A> &rhs)
{
    return descore::stl_tree_less<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename P, typename A>
inline bool operator> (const std::set<K, P, A> &lhs, const std::set<K, P, A> &rhs)
{
    return descore::stl_tree_less<P>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename P, typename A>
inline bool operator<= (const std::set<K, P, A> &lhs, const std::set<K, P, A> &rhs)
{
    return !descore::stl_tree_less<P>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename P, typename A>
inline bool operator>= (const std::set<K, P, A> &lhs, const std::set<K, P, A> &rhs)
{
    return !descore::stl_tree_less<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}

// multiset
template <typename K, typename P, typename A>
inline bool operator< (const std::multiset<K, P, A> &lhs, const std::multiset<K, P, A> &rhs)
{
    return descore::stl_tree_less<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename P, typename A>
inline bool operator> (const std::multiset<K, P, A> &lhs, const std::multiset<K, P, A> &rhs)
{
    return descore::stl_tree_less<P>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename P, typename A>
inline bool operator<= (const std::multiset<K, P, A> &lhs, const std::multiset<K, P, A> &rhs)
{
    return !descore::stl_tree_less<P>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename P, typename A>
inline bool operator>= (const std::multiset<K, P, A> &lhs, const std::multiset<K, P, A> &rhs)
{
    return !descore::stl_tree_less<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}

// map
template <typename K, typename T, typename P, typename A>
inline bool operator< (const std::map<K, T, P, A> &lhs, const std::map<K, T, P, A> &rhs)
{
    typedef descore::pair_less<K, T, P, std::less<T> > pair_less;
    return descore::stl_tree_less<pair_less>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename T, typename P, typename A>
inline bool operator> (const std::map<K, T, P, A> &lhs, const std::map<K, T, P, A> &rhs)
{
    typedef descore::pair_less<K, T, P, std::less<T> > pair_less;
    return descore::stl_tree_less<pair_less>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename T, typename P, typename A>
inline bool operator<= (const std::map<K, T, P, A> &lhs, const std::map<K, T, P, A> &rhs)
{
    typedef descore::pair_less<K, T, P, std::less<T> > pair_less;
    return !descore::stl_tree_less<pair_less>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename T, typename P, typename A>
inline bool operator>= (const std::map<K, T, P, A> &lhs, const std::map<K, T, P, A> &rhs)
{
    typedef descore::pair_less<K, T, P, std::less<T> > pair_less;
    return !descore::stl_tree_less<pair_less>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}

// multimap
template <typename K, typename T, typename P, typename A>
inline bool operator< (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return descore::multimap_less(lhs, rhs);
}
template <typename K, typename T, typename P, typename A>
inline bool operator> (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return descore::multimap_less(rhs, lhs);
}
template <typename K, typename T, typename P, typename A>
inline bool operator<= (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return !descore::multimap_less(rhs, lhs);
}
template <typename K, typename T, typename P, typename A>
inline bool operator>= (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return !descore::multimap_less(lhs, rhs);
}
template <typename K, typename T, typename P, typename A>
inline bool operator== (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return descore::multimap_eq(lhs, rhs);
}
template <typename K, typename T, typename P, typename A>
inline bool operator!= (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return !descore::multimap_eq(lhs, rhs);
}

//
// descore::lt/gt/lte/gte/eq/neq.  These operators are always guaranteed to work.
//

BEGIN_NAMESPACE_DESCORE

// set
template <typename K, typename P, typename A>
inline bool lt (const std::set<K, P, A> &lhs, const std::set<K, P, A> &rhs)
{
    return stl_tree_less<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename P, typename A>
inline bool gt (const std::set<K, P, A> &lhs, const std::set<K, P, A> &rhs)
{
    return stl_tree_less<P>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename P, typename A>
inline bool lte (const std::set<K, P, A> &lhs, const std::set<K, P, A> &rhs)
{
    return !stl_tree_less<P>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename P, typename A>
inline bool gte (const std::set<K, P, A> &lhs, const std::set<K, P, A> &rhs)
{
    return !stl_tree_less<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename P, typename A>
inline bool eq (const std::set<K, P, A> &lhs, const std::set<K, P, A> &rhs)
{
    return stl_tree_eq<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename P, typename A>
inline bool neq (const std::set<K, P, A> &lhs, const std::set<K, P, A> &rhs)
{
    return !stl_tree_eq<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}

// multiset
template <typename K, typename P, typename A>
inline bool lt (const std::multiset<K, P, A> &lhs, const std::multiset<K, P, A> &rhs)
{
    return stl_tree_less<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename P, typename A>
inline bool gt (const std::multiset<K, P, A> &lhs, const std::multiset<K, P, A> &rhs)
{
    return stl_tree_less<P>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename P, typename A>
inline bool lte (const std::multiset<K, P, A> &lhs, const std::multiset<K, P, A> &rhs)
{
    return !stl_tree_less<P>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename P, typename A>
inline bool gte (const std::multiset<K, P, A> &lhs, const std::multiset<K, P, A> &rhs)
{
    return !stl_tree_less<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename P, typename A>
inline bool eq (const std::multiset<K, P, A> &lhs, const std::multiset<K, P, A> &rhs)
{
    return stl_tree_eq<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename P, typename A>
inline bool neq (const std::multiset<K, P, A> &lhs, const std::multiset<K, P, A> &rhs)
{
    return !stl_tree_eq<P>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}

// map
template <typename K, typename T, typename P, typename A>
inline bool lt (const std::map<K, T, P, A> &lhs, const std::map<K, T, P, A> &rhs)
{
    typedef pair_less<K, T, P, std::less<T> > pair_less;
    return stl_tree_less<pair_less>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename T, typename P, typename A>
inline bool gt (const std::map<K, T, P, A> &lhs, const std::map<K, T, P, A> &rhs)
{
    typedef pair_less<K, T, P, std::less<T> > pair_less;
    return stl_tree_less<pair_less>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename T, typename P, typename A>
inline bool lte (const std::map<K, T, P, A> &lhs, const std::map<K, T, P, A> &rhs)
{
    typedef pair_less<K, T, P, std::less<T> > pair_less;
    return !stl_tree_less<pair_less>(rhs.rbegin(), rhs.rend(), lhs.rbegin(), lhs.rend());
}
template <typename K, typename T, typename P, typename A>
inline bool gte (const std::map<K, T, P, A> &lhs, const std::map<K, T, P, A> &rhs)
{
    typedef pair_less<K, T, P, std::less<T> > pair_less;
    return !stl_tree_less<pair_less>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename T, typename P, typename A>
inline bool eq (const std::map<K, T, P, A> &lhs, const std::map<K, T, P, A> &rhs)
{
    typedef pair_less<K, T, P, std::less<T> > pair_less;
    return stl_tree_eq<pair_less>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}
template <typename K, typename T, typename P, typename A>
inline bool neq (const std::map<K, T, P, A> &lhs, const std::map<K, T, P, A> &rhs)
{
    typedef pair_less<K, T, P, std::less<T> > pair_less;
    return !stl_tree_eq<pair_less>(lhs.rbegin(), lhs.rend(), rhs.rbegin(), rhs.rend());
}

// multimap
template <typename K, typename T, typename P, typename A>
inline bool lt (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return multimap_less(lhs, rhs);
}
template <typename K, typename T, typename P, typename A>
inline bool gt (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return multimap_less(rhs, lhs);
}
template <typename K, typename T, typename P, typename A>
inline bool lte (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return !multimap_less(rhs, lhs);
}
template <typename K, typename T, typename P, typename A>
inline bool gte (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return !multimap_less(lhs, rhs);
}
template <typename K, typename T, typename P, typename A>
inline bool eq (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return multimap_eq(lhs, rhs);
}
template <typename K, typename T, typename P, typename A>
inline bool neq (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    return !multimap_eq(lhs, rhs);
}

// multimap_less
template <typename K, typename T, typename P, typename A>
bool multimap_less (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    typedef std::multimap<K, T, P, A> mmap;
    typename mmap::const_reverse_iterator it[2] = { lhs.rbegin(), rhs.rbegin() };
    typename mmap::const_reverse_iterator end[2] = { lhs.rend(), rhs.rend() };
    while (it[0] != end[0])
    {
        if (it[1] == end[1])
            return false;
        const K &key1 = it[0]->first;
        const K &key2 = it[1]->first;
        P lt;
        if (lt(key1, key2))
            return true;
        if (lt(key2, key1))
            return false;

        std::multiset<T> s[2];
        for (int i = 0 ; i < 2 ; i++)
        {
            for ( ; it[i] != end[i] && !lt(it[i]->first, key1) ; it[i]++)
                s[i].insert(it[i]->second);
        }
        if (descore::lt(s[0], s[1]))
            return true;
        if (descore::lt(s[1], s[0]))
            return false;
    }
    return (it[1] != end[1]);
}

// multimap_eq
template <typename K, typename T, typename P, typename A>
bool multimap_eq (const std::multimap<K, T, P, A> &lhs, const std::multimap<K, T, P, A> &rhs)
{
    typedef std::multimap<K, T, P, A> mmap;
    typename mmap::const_reverse_iterator it[2] = { lhs.rbegin(), rhs.rbegin() };
    typename mmap::const_reverse_iterator end[2] = { lhs.rend(), rhs.rend() };
    while (it[0] != end[0])
    {
        if (it[1] == end[1])
            return false;
        const K &key1 = it[0]->first;
        const K &key2 = it[1]->first;
        P lt;
        if (lt(key1, key2) || lt(key2, key1))
            return false;

        std::multiset<T> s[2];
        for (int i = 0 ; i < 2 ; i++)
        {
            for ( ; it[i] != end[i] && !lt(it[i]->first, key1) ; it[i]++)
                s[i].insert(it[i]->second);
        }
        if (descore::neq(s[0], s[1]))
            return false;
    }
    return (it[1] == end[1]);
}

//
// Also define lt for other containers to handle sets of vectors, etc.
//
template <typename T, typename A>
bool lt (const std::vector<T, A> &lhs, const std::vector<T, A> &rhs)
{
    return stl_tree_less<std::less<T> >(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}
template <typename T, typename A>
bool lt (const std::deque<T, A> &lhs, const std::deque<T, A> &rhs)
{
    return stl_tree_less<std::less<T> >(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}
template <typename T, typename A>
bool lt (const std::list<T, A> &lhs, const std::list<T, A> &rhs)
{
    return stl_tree_less<std::less<T> >(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

END_NAMESPACE_DESCORE

//
// Define std::less for sets and maps so that trees of trees will work
//

BEGIN_NAMESPACE_DESCORE

template <typename T>
struct tree_less
{
    bool operator() (const T &lhs, const T &rhs) const
    {
        return lt(lhs, rhs);
    }
};

END_NAMESPACE_DESCORE

namespace std
{
    template <typename K, typename P, typename A>
    struct less<std::set<K, P, A> > : public descore::tree_less<std::set<K, P, A> > {}; 

    template <typename K, typename P, typename A>
    struct less<std::multiset<K, P, A> > : public descore::tree_less<std::multiset<K, P, A> > {}; 

    template <typename K, typename T, typename P, typename A>
    struct less<std::map<K, T, P, A> > : public descore::tree_less<std::map<K, T, P, A> > {};

    template <typename K, typename T, typename P, typename A>
    struct less<std::multimap<K, T, P, A> > : public descore::tree_less<std::multimap<K, T, P, A> > {};

    template <typename T, typename A>
    struct less<std::vector<T, A> > : public descore::tree_less<std::vector<T, A> > {};

    template <typename T, typename A>
    struct less<std::deque<T, A> > : public descore::tree_less<std::deque<T, A> > {};

    template <typename T, typename A>
    struct less<std::list<T, A> > : public descore::tree_less<std::list<T, A> > {};
}

#endif // #ifndef descore_stltree_hpp
 
