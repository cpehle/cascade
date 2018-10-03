/*
Copyright 2012, D. E. Shaw Research.
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
// MapIterators.hpp
//
// Copyright (C) 2012 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 01/31/2013
//
// Helper types and macros to make iterating over maps (and multimaps) a bit cleaner.
//
// 1) Iterate over values
//
//     for_map_values (type var, map)
//        ...
//
// is syntactic sugar for
//
//     for (auto &kv : map)
//     {
//         type var = kv.second;
//         ...
//     }
//
// 2) Iterate over keys
//
//     for_map_keys (type var, map)
//        ...
//
// is syntactic sugar for
//
//     for (auto &kv : map)
//     {
//         type var = kv.first;
//         ...
//     }
//
// 3) Iterate over values with access to keys
//
//    for (MapItem<K,T> it : map)  // or MapItem<MapType>, or ConstMapItem<>
//    {
//        K key   = it.key;
//        T value = it.value;      // or *v
//        ...
//    }
//
// is syntactic sugar for
//
//    for (std::pair<K,T> it : map)
//    {
//        K key   = it.first;
//        T value = it.second;
//        ...
//    }
//
////////////////////////////////////////////////////////////////////////////////

#ifndef MapIterators_hpp_737004071327947834
#define MapIterators_hpp_737004071327947834

#include "defines.hpp"
#include <utility>

////////////////////////////////////////////////////////////////////////////////
//
// MapItem 
//
////////////////////////////////////////////////////////////////////////////////

template <typename KeyType, typename ValueType = void>
struct MapItem
{
private:
    MapItem &operator= (const MapItem &);
public:
    MapItem (std::pair<const KeyType, ValueType> &kv) : key(kv.first), value(kv.second) {}

    const KeyType &key;
    ValueType     &value;
};

template <typename TMap>
struct MapItem<TMap, void> : public MapItem<typename TMap::key_type, typename TMap::mapped_type>
{
    MapItem (typename TMap::value_type &kv) : MapItem<typename TMap::key_type, typename TMap::mapped_type>(kv) {}
};

template <typename KeyType, typename ValueType = void>
struct ConstMapItem
{
private:
    ConstMapItem &operator= (const ConstMapItem &);
public:
    ConstMapItem (const std::pair<const KeyType, ValueType> &kv) : key(kv.first), value(kv.second) {}

    const KeyType   &key;
    const ValueType &value;
};

template <typename TMap>
struct ConstMapItem<TMap, void> : public ConstMapItem<typename TMap::key_type, typename TMap::mapped_type>
{
    ConstMapItem (const typename TMap::value_type &kv) : ConstMapItem<typename TMap::key_type, typename TMap::mapped_type>(kv) {}
};

////////////////////////////////////////////////////////////////////////////////
//
// for_map_xxx
//
////////////////////////////////////////////////////////////////////////////////

BEGIN_NAMESPACE_DESCORE

//----------------------------------
// Helper structures
//----------------------------------

template <typename TIter, typename TVal>
struct MapKeyIterator
{
    MapKeyIterator (TIter _it) : it(_it) {}
    bool operator != (const MapKeyIterator &rhs) { return it != rhs.it; }
    const TVal &operator* () const { return it->first; }
    void operator++ () { ++it; }

    TIter it;
};

template <typename TIter, typename TVal>
struct MapItemIterator
{
    MapItemIterator (TIter _it) : it(_it) {}
    bool operator != (const MapItemIterator &rhs) { return it != rhs.it; }
    TVal &operator* () const { return it->second; }
    void operator++ () { ++it; }

    TIter it;
};

template <typename TMap, typename TIter>
struct MapAdapter
{
private:
    MapAdapter &operator= (const MapAdapter &);
public:

    MapAdapter (TMap &m) : m_map(m) {}
    TIter begin () const { return m_map.begin(); }
    TIter end   () const { return m_map.end();   }

    TMap &m_map;
};

//----------------------------------
// Helper functions
//----------------------------------

template <typename TMap>
MapAdapter<const TMap, MapKeyIterator<typename TMap::const_iterator, typename TMap::key_type>> 
map_keys (const TMap &m)
{
    return MapAdapter<const TMap, MapKeyIterator<typename TMap::const_iterator, typename TMap::key_type>>(m);
}

template <typename TMap>
MapAdapter<TMap, MapItemIterator<typename TMap::iterator, typename TMap::mapped_type>> map_values (TMap &m)
{
    return MapAdapter<TMap, MapItemIterator<typename TMap::iterator, typename TMap::mapped_type>>(m);
}

template <typename TMap>
MapAdapter<const TMap, MapItemIterator<typename TMap::const_iterator, const typename TMap::mapped_type>> map_values (const TMap &m)
{
    return MapAdapter<const TMap, MapItemIterator<typename TMap::const_iterator, const typename TMap::mapped_type>>(m);
}

END_NAMESPACE_DESCORE

//----------------------------------
// Actual macros
//----------------------------------

#define for_map_keys(var,map) \
    if (bool _foreach_break = false) {} else \
        for (auto && _foreach_map = map ; !_foreach_break ; _foreach_break = true) \
            for (var : descore::map_keys(_foreach_map))

#define for_map_values(var,map) \
    if (bool _foreach_break = false) {} else \
        for (auto && _foreach_map = map ; !_foreach_break ; _foreach_break = true) \
            for (var : descore::map_values(_foreach_map))


#endif // #ifndef MapIterators_hpp_737004071327947834
