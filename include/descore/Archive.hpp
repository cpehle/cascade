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
// Archive.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// Support for archiving to/from a file.  Provides the | operator as a
// replacement for the standard << and >> operators (which are still available)
// so that, for the most part, the same code can be used for both saving
// and loading.  This both simplifies the code and reduces the number of
// archiving-related bugs.  Example:
//
//    ar | m_val | m_count;
//
// A second mechanism which is supplied to help detect archiving problems
// is "safe mode".  When an archive is created in safe mode, check-bytes are
// inserted before every single primitive value.  These check-bytes are validated
// when an archive is loaded, which detects mismatches between code for saving
// and loading state.
//
// Primitives and containers of primitives (and containers of containers of
// primitives, etc) can be archived directly using the | operator.  Enumerations
// and "bag of bits" types (i.e. types with the default copy constructor and
// assignment operator) can also be archived directly.  If necessary (it's
// usually not necessary), use the DECLARE_ARCHIVE_PRIMITIVE macro to allow
// a type with a non-default assignment operator or copy constructor to be 
// archived as a "bag of bits" (i.e. don't use it for structures containing 
// pointers, or classes with virtual functions, such as STL containers).  
// There are already methods declared for archiving all of the STL container 
// classes, and DECLARE_ARCHIVE_PRIMITIVE overrides them, treating them as 
// buckets of bits, which fails. 
//
// Example:
//
//    struct Point
//    {
//        int x;
//        int y;
//    };
//    ...
//    Point p;
//    ...
//    ar | p;
//
// As a convenience, the DECLARE_ARCHIVE_OBJECT macro can be used to define
// the | operator for an object that has an archive() function.  Example:
//
//    struct TwoStrings
//    {
//        string s1;
//        string s2;
//        void archive (Archive &ar)
//        {
//            ar | s1 | s2;
//        }
//    };
//
//    DECLARE_ARCHIVE_OBJECT(TwoStrings);
//    ...
//    TwoStrings s;
//    ...
//    ar | s;
//
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_Archive_hpp_
#define descore_Archive_hpp_

#include "assert.hpp"

////////////////////////////////////////////////////////////////////////
//
// Archive class
//
////////////////////////////////////////////////////////////////////////
class Archive 
{
    DECLARE_NOCOPY(Archive);

    typedef void (Archive::*ArchivePrimitiveFunction) (void *buff, int size);

public:

    //----------------------------------
    // Constructor/Destructor
    //----------------------------------
    enum Mode
    {
        Load,        // Load from a file
        Store,       // Store to a file
        SafeStore,   // Store to a file with single-byte check values for debugging
        Validate,    // Compare to a file and assert equal

        //
        // Additional flags
        //

        // Load zeros past the end of the file instead of generating an assertion failure
        AllowReadPastEOF = 0x4,
    };

    // Use constructor to open archive, or do it manually
    Archive (const string &filename, int mode);
    Archive ();
    void open (const string &filename, int mode);

    // Use destructor to close archive, or do it manually
    ~Archive ();
    void close ();

    //----------------------------------
    // Accessors and operators
    //----------------------------------

    inline bool isLoading () const 
    { 
        return m_isLoading;
    }

    inline bool validationError () const
    {
        return m_validationError;
    }

    inline void clearValidationErrorFlag ()
    { 
        m_validationError = false;
    }

    // Return true if more data can be archived
    operator bool ();

    // Define << and >> operators
    template <typename T> inline Archive &operator<< (const T &v)
    {
        assert_always(!m_isLoading);
        *this | (T&) v;
        return *this;
    }
    template <typename T> inline Archive &operator>> (T &v)
    {
        assert_always(m_isLoading);
        *this | v;
        return *this;
    }

    // Archive arbitrary data
    void archiveData (void *buff, int size);

    // Helper function to archive a fixed-size array of primitives
    template <typename T> void archiveArray (T *data, int size);

    //----------------------------------
    // Internal helper functions
    //----------------------------------

    // vectors, deque, queue, fifo
    template <class Vector> void archiveVector (Vector &v);

    // set, multiset, map, multimap
    template <class Set> static void archiveSet (Archive &ar, Set &t);
    template <class Map> static void archiveMap (Archive &ar, Map &t);

    // Archive/validate the checkval (does nothing if not in safe mode)
    void archiveCheckval (char inc);

protected:

    // Archive a primitive
    void savePrimitive (void *buff, int size);
    void loadPrimitive (void *buff, int size);
    void validatePrimitive (void *buff, int size);

    // Functions to read/write compressed data
    void write (const void *buff, int size);
    void read (void *buff, int size);
    void writeBlock ();
    void readBlock ();

private:
    FILE *m_f;
    ArchivePrimitiveFunction m_fnArchivePrimitive;
    bool m_safeMode;
    bool m_isLoading;
    bool m_isReading;
    bool m_allowReadPastEOF;
    char m_checkval;
    bool m_validationError;

    // In-memory buffers for compression
    byte *m_data;        // Buffer for uncompressed data
    int  m_numBytes;     // Number of bytes in buffer when loading
    int  m_dataIndex;    // Read/write index into uncompressed data
    byte *m_compressed;  // Buffer for compressed data

};

/////////////////////////////////////////////////////////////////
//
// Declarations for archiving primitive types
//
/////////////////////////////////////////////////////////////////

// Helper structure to auto-archive bag-of-bits data
BEGIN_NAMESPACE_DESCORE

template <bool bag_of_bits> struct ArchiveHelper;

template <>
struct ArchiveHelper<true>
{
    template <typename T>
    static inline void archive (Archive &ar, T &v)
    {
        ar.archiveData((void *) &v, sizeof(T));
    }
};

END_NAMESPACE_DESCORE

// Generic archive operator
template <typename T>
inline Archive &operator| (Archive &ar, T &v)
{
    descore::ArchiveHelper<descore::type_traits<T>::is_bag_of_bits>::archive(ar, v);
    return ar;
}

// DECLARE_ARCHIVE_PRIMITIVE should *only* be used for "bag of bits" types.
// Generally this macro is not necessary since archiving is automatically
// defined for structures with default assignment operators and copy
// constructors.
//
// The __VA_ARGS__ is necessary in order to deal with templated types with
// multiple template arguments, because then 'type' will contain commas and
// the preprocessor will interpret this as multiple macro arguments.  e.g.
//
//   template <int a, int b>
//   DECLARE_ARCHIVE_PRIMITIVE(poly2<a,b>);
//
#define DECLARE_ARCHIVE_PRIMITIVE(type, ...) \
    template <> \
    inline Archive &operator| (Archive &ar, type, ##__VA_ARGS__ &v) \
    { \
        ar.archiveData(&v, sizeof(type, ##__VA_ARGS__)); \
        return ar; \
    } \

#define DECLARE_ARCHIVE_OBJECT(type, ...) \
    template <> \
    inline Archive &operator| (Archive &ar, type, ##__VA_ARGS__ &obj) \
    { \
        obj.archive(ar); \
        return ar; \
    }

#define DECLARE_TEMPLATE_ARCHIVE_PRIMITIVE(type, ...) \
    inline Archive &operator| (Archive &ar, type, ##__VA_ARGS__ &v) \
    { \
        ar.archiveData(&v, sizeof(type, ##__VA_ARGS__)); \
        return ar; \
    } \

#define DECLARE_TEMPLATE_ARCHIVE_OBJECT(type, ...) \
    inline Archive &operator| (Archive &ar, type, ##__VA_ARGS__ &obj) \
    { \
        obj.archive(ar); \
        return ar; \
    }

// Helper function to archive a fixed-size array of primitives
template <typename T>
void Archive::archiveArray (T *data, int size)
{
    int currSize = size;
    *this | size;
    assert_always(size == currSize, "Cannot load array of size %d: size in archive is %d", currSize, size);
    assert_always(!size || data);
    for (int i = 0 ; i < size ; i++)
        *this | data[i];
}

/////////////////////////////////////////////////////////////////
//
// declarations
//
/////////////////////////////////////////////////////////////////

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

template <typename T> class ParameterValue;

/////////////////////////////////////////////////////////////////
//
// string
//
/////////////////////////////////////////////////////////////////

Archive &operator| (Archive &ar, string &str);
Archive &operator| (Archive &ar, const string &str);
Archive &operator| (Archive &ar, const char *sz);
Archive &operator<< (Archive &ar, const char *sz);

/////////////////////////////////////////////////////////////////
//
// Parameter
//
/////////////////////////////////////////////////////////////////

template <typename T>
Archive &operator| (Archive &ar, ParameterValue<T> &param)
{
    T value = *param;
    ar | value;
    param = value;
    return ar;
}

/////////////////////////////////////////////////////////////////
//
// list
//
/////////////////////////////////////////////////////////////////

template <typename T, typename A>
Archive &operator| (Archive &ar, std::list<T, A> &l)
{
    int size = (int) l.size();
    ar | size;
    if (ar.isLoading())
    {
        l.clear();
        for (int i = 0 ; i < size ; i++)
        {
            T value;
            ar | value;
            l.push_back(value);
        }
    }
    else
    {
        typename std::list<T, A>::const_iterator it = l.begin();
        typename std::list<T, A>::const_iterator itEnd = l.end();
        for ( ; it != itEnd ; it++)
            ar | (T&) *it;
    }
    return ar;
}

/////////////////////////////////////////////////////////////////
//
// vector, deque, queue, fifo
//
/////////////////////////////////////////////////////////////////

template <class Vector>
void Archive::archiveVector (Vector &v)
{
    int size = (int) v.size();
    *this | size;
    if (isLoading())
        v.resize(size);
    for (int i = 0 ; i < size ; i++)
        *this | v[i];
}

template <typename T, typename A>
inline Archive &operator| (Archive &ar, std::vector<T, A> &v)
{
    ar.archiveVector(v);
    return ar;
}

template <typename T, typename A>
inline Archive &operator| (Archive &ar, std::deque<T, A> &v)
{
    ar.archiveVector(v);
    return ar;
}

template <typename T, typename Alloc>
inline Archive &operator| (Archive &ar, descore::queue<T,Alloc> &v)
{
    int highWaterMark = v.highWaterMark();
    ar | highWaterMark;
    v.setHighWaterMark(highWaterMark);
    ar.archiveVector(v);
    return ar;
}

template <typename T, typename Alloc>
inline Archive &operator| (Archive &ar, descore::fifo<T,Alloc> &v)
{
    int highWaterMark = v.highWaterMark();
    ar | highWaterMark;
    v.setHighWaterMark(highWaterMark);
    ar.archiveVector(v);
    return ar;
}

template <typename A>
Archive &operator| (Archive &ar, std::vector<bool, A> &v)
{
    int size = (int) v.size();
    ar | size;
    bool b;
    if (ar.isLoading())
    {
        v.resize(size);
        for (int i = 0 ; i < size ; i++)
        {
            ar | b;
            v[i] = b;
        }
    }
    else
    {
        for (int i = 0 ; i < size ; i++)
        {
            b = v[i];
            ar | b;
        }
    }
    return ar;
}


/////////////////////////////////////////////////////////////////
//
// set, multiset
//
/////////////////////////////////////////////////////////////////

template <class Set> 
void Archive::archiveSet (Archive &ar, Set &s)
{
    int size = (int) s.size();
    ar | size;
    if (ar.isLoading())
    {
        s.clear();
        typename Set::value_type value;
        for (int i = 0 ; i < size ; i++)
        {
            ar >> value;
            s.insert(value);
        }
    }
    else
    {
        typename Set::const_iterator it = s.begin();
        typename Set::const_iterator itEnd = s.end();
        for ( ; it != itEnd ; it++)
            ar << *it;
    }
}

template <typename K, typename P, typename A>
inline Archive &operator| (Archive &ar, std::set<K, P, A> &s)
{
    Archive::archiveSet(ar, s);
    return ar;
}

template <typename K, typename P, typename A>
inline Archive &operator| (Archive &ar, std::multiset<K, P, A> &s)
{
    Archive::archiveSet(ar, s);
    return ar;
}

/////////////////////////////////////////////////////////////////
//
// map, multimap
//
/////////////////////////////////////////////////////////////////

template <class Map> 
void Archive::archiveMap (Archive &ar, Map &m)
{
    int size = (int) m.size();
    ar | size;
    if (ar.isLoading())
    {
        m.clear();
        typename Map::key_type key;
        typename Map::mapped_type value;
        for (int i = 0 ; i < size ; i++)
        {
            ar >> key >> value;
            m.insert(typename Map::value_type(key, value));
        }
    }
    else
    {
        typename Map::const_iterator it = m.begin();
        typename Map::const_iterator itEnd = m.end();
        for ( ; it != itEnd ; it++)
            ar << it->first << it->second;
    }
}

template <typename K, typename T, typename P, typename A>
inline Archive &operator| (Archive &ar, std::map<K, T, P, A> &m)
{
    Archive::archiveMap(ar, m);
    return ar;
}

template <typename K, typename T, typename P, typename A>
inline Archive &operator| (Archive &ar, std::multimap<K, T, P, A> &m)
{
    Archive::archiveMap(ar, m);
    return ar;
}

/////////////////////////////////////////////////////////////////
//
// pair
//
/////////////////////////////////////////////////////////////////

template <typename T1, typename T2>
Archive &operator| (Archive &ar, std::pair<T1, T2> &p)
{
    ar | p.first;
    ar | p.second;
    return ar;
}

/////////////////////////////////////////////////////////////////
//
// strbuff
//
/////////////////////////////////////////////////////////////////
#ifdef descore_StringBuffer_hpp_
DECLARE_ARCHIVE_OBJECT(strbuff);
#endif


#endif
