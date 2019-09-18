/*
Copyright 2009, D. E. Shaw Research.
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
// strcast.hpp
//
// Copyright (C) 2009 D. E. Shaw Research
//
// Authors: J.P. Grossman (jp.grossman@deshawresearch.com)
//          John Salmon   (john.salmon@deshawresearch.com)
// Created: 02/14/2009
//
// Based on string_cast and stringprintf by John Salmon
//
//////////////////////////////////////////////////////////////////////

#ifndef strcast_hpp_090214090436
#define strcast_hpp_090214090436

#include "type_traits.hpp"
#include "assert.hpp"
#include <sstream>

/////////////////////////////////////////////////////////////////
//
// String conversion
//
/////////////////////////////////////////////////////////////////

// For those who hate typing s.c_str()... just type *s instead.
inline const char *operator* (const string &s)
{
    return s.c_str();
}

// Create a string with printf() or vprintf() semantics.
inline string str ()
{
    return "";
}
string str (const char *fmt, ...) ATTRIBUTE_PRINTF(1);
string vstr (const char *fmt, va_list args);

/////////////////////////////////////////////////////////////////
//
// Exception class
//
/////////////////////////////////////////////////////////////////
DECLARE_EXCEPTION(strcast_error);

/////////////////////////////////////////////////////////////////
//
// istream &operator>> (const char *)
//
// This operator is useful for parsing input streams that contain
// known delimiters.  e.g. a vector represented as "(1,2,3)" can
// be parsed from an input stream as
//
//  is >> "(" >> x >> "," >> y >> "," >> z >> ")"
//
/////////////////////////////////////////////////////////////////
std::istream &operator>> (std::istream &is, const char *delimstring);

template<typename T, typename A>
std::istream &operator>> (std::istream &is, std::vector<T, A> &vec);

template<typename T, typename A>
std::ostream &operator<< (std::ostream &os, const std::vector<T, A> &vec);

BEGIN_NAMESPACE_DESCORE
struct ContainerStringHelper;
END_NAMESPACE_DESCORE

std::ostream &operator<< (std::ostream &os, descore::ContainerStringHelper s);


/////////////////////////////////////////////////////////////////
//
// strcaststream
//
// A strcast stream fixes some of the issues with standard streams.
// For an istrcaststream:
//
//  - integer constants can appear in decimal, hex or octal
//  - int8/uint8 are treated as integers
//  - both 0/1 and "true"/"false" are recognized as booleans
//  - the thousands separator is cleared to work around a problem either in the
//    C++ standard or in MSVC debug builds that prevent comma-separated integers
//    from working
//  - eof() returns true if there are no characters to read
//  - peek() returns the eof character without setting the fail bit if there
//    are no characters to read
//
// For an ostrcaststream:
//
//  - int8/uint8 are treated as integers
//  - boolean values are converted to the strings "true" and "false"
//  - precision is set high enough to exactly represent floats/doubles
//  - pointers are prefixed with their type (gdb-style)
//
/////////////////////////////////////////////////////////////////

//--------------------------------------------------------
// Input stream: works with either a string or an istream
//--------------------------------------------------------
class istrcaststream
{
public:
    istrcaststream (std::istream &is) : m_is(is)
    {
        initstream();
    }
    istrcaststream (const string &s) : m_iss(s), m_is(m_iss)
    {
        initstream();
    }

private:
    void initstream ();

public:
    // Conversion to istream
    inline operator std::istream & ()
    {
        return m_is;
    }

    // Pointer operator for easy access to istream functions    
    inline std::istream *operator-> ()
    {
        return &m_is;
    }

    // operators for if (iss), if (!iss)
    inline operator void * () const
    {
        return &m_is; // (void *) m_is;
    }
    inline bool operator! () const
    {
        return !m_is;
    }

    // eof() returns true if there are no characters to read
    inline bool eof () const
    {
        return m_is.rdbuf()->in_avail() == 0;
    };

    // peek() returns the eof character if there are no characters
    // to read and does not set the fail bit
    inline int peek () const
    {
        return eof() ? std::istringstream::traits_type::eof() : m_is.peek();
    }

    // Explicitly skip whitespace (useful before calling ->peek() )
    void skipws ();

private:
    std::istringstream m_iss;
    std::istream &m_is;
};

// By default, delegate to the input stream
template <typename T>
inline istrcaststream &operator>> (istrcaststream &is, T &val)
{
    (std::istream &) is >> val;
    return is;
}

// Specialization for bool so that booleans can be read either as 0/1 or
// as "true"/"false" (defined in strcast.cpp)
template <>
istrcaststream &operator>> (istrcaststream &is, bool &val);

// Specialization for int8/uint8 to read as integer
template <>
inline istrcaststream &operator>> (istrcaststream &is, int8 &val)
{
    int n;
    (std::istream &) is >> n;
    val = (int8) n;
    return is;
}
template <>
inline istrcaststream &operator>> (istrcaststream &is, uint8 &val)
{
    unsigned n;
    (std::istream &) is >> n;
    val = (uint8) n;
    return is;
}

// Pointers
template  <typename T>
inline istrcaststream &operator>> (istrcaststream &iss, T *&ptr)
{
    iss.skipws();
    if (iss.peek() == '(')
        iss >> *str("(%s *) ", descore::type_traits<T>::name());
    intptr_t iptr;
    iss >> iptr;
    ptr = (T *) iptr;
    return iss;
}
template <typename T>
inline istrcaststream &operator>> (istrcaststream &iss, const T *&ptr)
{
    iss.skipws();
    if (iss.peek() == '(')
        iss >> *str("(%s *) ", descore::type_traits<T>::name());
    intptr_t iptr;
    iss >> iptr;
    ptr = (const T *) iptr;
    return iss;
}
inline istrcaststream &operator>> (istrcaststream &iss, char *&sz)
{
    (std::istream &) iss >> sz;
    return iss;
}
inline istrcaststream &operator>> (istrcaststream &iss, const char *&sz)
{
    (std::istream &) iss >> sz;
    return iss;
}

//--------------------------------------------------------
// Output stream: works with either a string or an ostream
//--------------------------------------------------------
class ostrcaststream
{
public:
    ostrcaststream () : m_os(m_oss)
    {
        initstream();
    }

    ostrcaststream (std::ostream &os) : m_os(os)
    {
        initstream();
    }

private:
    void initstream ();

public:
    // Conversion to ostream
    inline operator std::ostream& ()
    {
        return m_os;
    }

    // Pointer operator for easy access to ostream functions
    inline std::ostream *operator-> ()
    {
        return &m_os;
    }

    // Operators for if (oss), if (!oss)
    inline operator void * () const
    {
        return &m_os; // (void *) m_os;
    }
    inline bool operator! () const
    {
        return !m_os;
    }

    // Return the string (when using ostringstream)
    inline string str () const
    {
        return m_oss.str();
    }

private:
    std::ostringstream m_oss;
    std::ostream &m_os;
};

// By default, delegate to the ostream
template <typename T>
inline ostrcaststream &operator<< (ostrcaststream &oss, const T &val)
{
    (std::ostream &) oss << val;
    return oss;
}

// Specialization for int8/uint8 to write as integer
template <>
inline ostrcaststream &operator<< (ostrcaststream &oss, const int8 &val)
{
    (std::ostream &) oss << (int) val;
    return oss;
}
template <>
inline ostrcaststream &operator<< (ostrcaststream &oss, const uint8 &val)
{
    (std::ostream &) oss << (unsigned) val;
    return oss;
}

// By default, use streaming to convert to a string.  This allows stringification
// of a new type T to be defined by overloading *either* str(T) or 
// operator<<(ostream,T).  In both cases, ostrcaststream can be used 
// to obtain desirable results (as listed in the comments above), and str(...) can
// be used if printf semantics are convenient.  Note that if performance is a 
// concern then the specialization of str() can create and return a strbuff instead of
// a string (#include "StringBuffer.hpp")
//
// Example:
//
//  struct Complex
//  {
//      float real;
//      float imag;
//  };
//
// Any one of the following five functions will do the trick:
//
//  string str (const Complex &c)
//  {
//      ostrcaststream oss;
//      oss << c.real << "+" << c.imag << "i";
//      return oss.str();
//  }
//
//  string str (const Complex &c)
//  {
//      return str("%f+%fi", c.real, c.imag);
//  }
//
//  strbuff str (const Complex &c)  // must #include "StringBuffer.hpp"
//  {
//      return strbuff("%f+%fi", c.real, c.imag);
//  }
//
//  std::ostream &operator<< (std::ostream &os, const Complex &c)
//  {
//      ostrcaststream oss(os);
//      oss << c.real << "+" << c.imag << "i";
//      return os;
//  }
//
//  std::ostream &operator<< (std::ostream &os, const Complex &c)
//  {
//      os << str("%f+%fi", c.real, c.imag);
//      return os;
//  }
// 

// This indirection allows objects to override operator>> (ostrcaststream)
template <typename T>
inline void operator>> (const T &val, ostrcaststream &oss)
{
    oss << val;
}
template <typename T>
inline string str (const T &val)
{
    ostrcaststream oss;
    val >> oss;
    return oss.str();
}

#ifdef _MSC_VER
#define STRCAST_PTR_FMT "(%s *) 0x%p"
#else
#define STRCAST_PTR_FMT "(%s *) %p"
#endif

// Pointers
template <typename T>
inline ostrcaststream &operator<< (ostrcaststream &oss, T *ptr)
{
    oss << str(STRCAST_PTR_FMT, descore::type_traits<T>::name(), ptr);
    return oss;
}
template <typename T>
inline ostrcaststream &operator<< (ostrcaststream &oss, const T *ptr)
{
    oss << str(STRCAST_PTR_FMT, descore::type_traits<T>::name(), ptr);
    return oss;
}
inline ostrcaststream &operator<< (ostrcaststream &oss, char *sz)
{
    (std::ostream &) oss << sz;
    return oss;
}
inline ostrcaststream &operator<< (ostrcaststream &oss, const char *sz)
{
    (std::ostream &) oss << sz;
    return oss;
}

#undef STRCAST_PTR_FMT

// By default, use streaming to convert from a string.  This allows unstringification
// of a new type T to be defined by overloading *either*  fromString(T) or
// operator>>(istream,T).  In both cases, istrcaststream can be used to obtain
// desirable results (as listed in the comments above).  In the former case, scanf
// semantics can also be used.
//
// Example:
//
//  struct Complex
//  {
//      float real;
//      float imag;
//  };
//
// Any one of the following three functions will do the trick:
//
//  void fromString (Complex &c, const string &s)
//  {
//      if (sscanf(*s, "%f+%fi", &c.real, &c.imag) != 2)
//          throw strcast_error("Failed to convert \"%s\" to Complex", *s);
//  }
//
//  void fromString (Complex &c, const string &s)
//  {
//      istrcaststream iss(s);
//      iss >> c.real >> "+" >> c.imag >> "i";
//      if (!iss || !iss.eof())
//          throw strcast_error("Failed to convert \"%s\" to Complex", *s);
//  }
//
//  std::istream &operator>> (std::istream &is, Complex &c)
//  {
//      istrcaststream iss(is);
//      iss >> c.real >> "+" >> c.imag >> "i";
//      return is;
//  }
// 

// This indirection allows objects to override operator<< (istrcaststream)
template <typename T>
inline void operator<< (T &val, istrcaststream &iss)
{
    iss >> val;
}
template <typename T>
void fromString (T &val, const string &s)
{
    istrcaststream iss(s);
    iss.skipws();
    val << iss;
    iss.skipws();

    if (!iss || !iss.eof())
        throw strcast_error("Failed to convert \"%s\" to %s", *s, descore::type_traits<T>::name());
}

// Alternate form with return value
template <typename T>
inline T stringTo (const string &s)
{
    T val;
    fromString(val, s);
    return val;
}

BEGIN_NAMESPACE_DESCORE
void empty_error_hook (descore::runtime_error &);
END_NAMESPACE_DESCORE

// Alternate form that wraps fromString in a try/catch block and returns a 
// boolean to indicate success
template <typename T>
bool tryFromString (T &val, const string &s, string *error_message = NULL)
{
    descore::ErrorHook old = descore::setErrorHook(&descore::empty_error_hook);
    try
    {
        fromString(val, s);
    }
    catch (strcast_error &e)
    {
        e.handled();
        if (error_message)
            *error_message = e.message();
    	return false;
    }
    descore::setErrorHook(old);
    return true;
}

BEGIN_NAMESPACE_DESCORE

// Wrap the string conversion functions for T into a type that can be supplied to
// templated classes (and optionally overridden).
template <typename T>
struct string_cast
{
    static string str (const T &val)
    {
        return ::str(val);
    }
    static void fromString (T &val, const string &s)
    {
        ::fromString(val, s);
    }
};

END_NAMESPACE_DESCORE

/////////////////////////////////////////////////////////////////
//
// Optimization - specialization for string.  It's also important
// to specialize fromString<string> so that the *entire* string
// will be returned, including whitespace.
//
/////////////////////////////////////////////////////////////////
template<>
inline string str (const string &s)
{
    return s;
}

template<>
inline void fromString (string &val, const string &s)
{
    val = s;
}

/////////////////////////////////////////////////////////////////
//
// Containers
//
/////////////////////////////////////////////////////////////////

/*
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
*/

namespace descore
{
    template <typename T, typename Alloc = std::allocator<T> > class queue;
    template <typename T, typename Alloc = std::allocator<T> > class fifo;    
};

// Parsing a container of strings is problematic because the default conversion
// from string will suck up the delimiters as well as the strings.  Define a 
// helper structure that will stop at whitespace or delimiters (",[]{}=") unless
// the string is quoted.

BEGIN_NAMESPACE_DESCORE

template <typename T>
struct ContainerValueHelper
{
    ContainerValueHelper (const char *_delimiters, bool _allowInternalWhitespace = false) 
        : delimiters(_delimiters),
          allowInternalWhitespace(_allowInternalWhitespace)
    {
    }

    operator T () const
    {
        return val;
    }
    
    bool operator== (const string &rhs)
    {
        return val == rhs;
    }
    bool operator!= (const string &rhs)
    {
        return val != rhs;
    }
    bool operator== (const char *rhs)
    {
        return val == rhs;
    }
    bool operator!= (const char *rhs)
    {
        return val != rhs;
    }

    const char *delimiters;
    bool allowInternalWhitespace;
    T val;
};

typedef ContainerValueHelper<string> delimited_string;

struct ContainerStringHelper
{
    const char *sz;
};


template <typename T>
inline const T &oswrite_helper (const T &v)
{
    return v;
}

inline ContainerStringHelper oswrite_helper (const string &s)
{
    ContainerStringHelper ret = { s.c_str() };
    return ret;
}

END_NAMESPACE_DESCORE


template <typename T>
inline istrcaststream &operator>> (istrcaststream &is, descore::ContainerValueHelper<T> &val)
{
    return is >> val.val;
}

std::istream &operator>> (std::istream &is, descore::delimited_string &val);

template<>
istrcaststream &operator>> (istrcaststream &is, descore::delimited_string &val);

inline std::ostream &operator<< (std::ostream &os, const descore::delimited_string &val)
{
    return os << val.val;
}


/////////////////////////////////////////////////////////////////
//
// std::vector, std::deque
//
/////////////////////////////////////////////////////////////////
template <typename T>
void ostream_vector (std::ostream &os, const T &vec)
{
    ostrcaststream oss(os);
    oss << "[";
    for (int i = 0 ; i < (int) vec.size() ; i++)
    {
        if (i)
            oss << ", ";
        oss << descore::oswrite_helper(vec[i]);
    }
    oss << "]";
}

template <typename T>
void istream_vector (std::istream &is, T &vec)
{
    istrcaststream iss(is);
    vec.clear();
    iss >> "[";
    while (iss)
    {
        // Done?
        iss.skipws();
        char ch = iss->peek();
        if (ch == ']')
            break;

        // Separator
        if (vec.size())
            iss >> ",";

        // Value
        descore::ContainerValueHelper<typename T::value_type> val(",]");
        iss >> val;
        if (iss)
            vec.push_back(val.val);
    }
    iss >> "]";
}

// std::vector
template <typename T, typename A>
std::ostream &operator<< (std::ostream &os, const std::vector<T, A> &vec)
{
    ostream_vector(os, vec);
    return os;
}

template <typename T, typename A>
std::istream &operator>> (std::istream &is, std::vector<T, A> &vec)
{
    istream_vector(is, vec);
    return is;
}

// std::deque
template <typename T, typename A>
std::ostream &operator<< (std::ostream &os, const std::deque<T, A> &vec)
{
    ostream_vector(os, vec);
    return os;
}

template <typename T, typename A>
std::istream &operator>> (std::istream &is, std::deque<T, A> &vec)
{
    istream_vector(is, vec);
    return is;
}

// descore::queue
template <typename T, typename Alloc>
std::ostream &operator<< (std::ostream &os, const descore::queue<T,Alloc> &vec)
{
    ostream_vector(os, vec);
    return os;
}

template <typename T, typename Alloc>
std::istream &operator>> (std::istream &is, descore::queue<T,Alloc> &vec)
{
    istream_vector(is, vec);
    return is;
}

// descore::fifo
template <typename T, typename Alloc>
std::ostream &operator<< (std::ostream &os, const descore::fifo<T,Alloc> &vec)
{
    ostream_vector(os, vec);
    return os;
}

template <typename T, typename Alloc>
std::istream &operator>> (std::istream &is, descore::fifo<T,Alloc> &vec)
{
    istream_vector(is, vec);
    return is;
}

/////////////////////////////////////////////////////////////////
//
// std::list
//
/////////////////////////////////////////////////////////////////
template <typename T, typename A>
std::ostream &operator<< (std::ostream &os, const std::list<T, A> &vec)
{
    ostrcaststream oss(os);
    oss << "[";
    typename std::list<T, A>::const_iterator itBegin = vec.begin();
    typename std::list<T, A>::const_iterator itEnd = vec.end();
    typename std::list<T, A>::const_iterator it;
    for (it = itBegin ; it != itEnd ; it++)
    {
        if (it != itBegin)
            oss << ", ";
        oss << descore::oswrite_helper(*it);
    }
    oss << "]";
    return os;
}

template <typename T, typename A>
std::istream &operator>> (std::istream &is, std::list<T, A> &vec)
{
    istream_vector(is, vec);
    return is;
}

/////////////////////////////////////////////////////////////////
//
// std::set, std::multiset
//
/////////////////////////////////////////////////////////////////
template <typename T>
void ostream_set (std::ostream &os, const T &s)
{
    ostrcaststream oss(os);
    oss << "{";
    typename T::const_iterator itBegin = s.begin();
    typename T::const_iterator itEnd = s.end();
    typename T::const_iterator it;
    for (it = itBegin ; it != itEnd ; it++)
    {
        if (it != itBegin)
            oss << ", ";
        oss << descore::oswrite_helper(*it);
    }
    oss << "}";
}

template <typename T>
void istream_set (std::istream &is, T &s)
{
    istrcaststream iss(is);
    s.clear();
    iss >> "{";
    while (iss)
    {
        // Done?
        iss.skipws();
        char ch = iss->peek();
        if (ch == '}')
            break;

        // Separator
        if (s.size())
            iss >> ",";

        // Value
        descore::ContainerValueHelper<typename T::value_type> val(",}");
        iss >> val;
        if (iss)
            s.insert(val.val);
    }
    iss >> "}";
}

// std::set
template <typename K, typename P, typename A>
std::ostream &operator<< (std::ostream &os, const std::set<K, P, A> &s)
{
    ostream_set(os, s);
    return os;
}

template <typename K, typename P, typename A>
std::istream &operator>> (std::istream &is, std::set<K, P, A> &s)
{
    istream_set(is, s);
    return is;
}

// std::multiset
template <typename K, typename P, typename A>
std::ostream &operator<< (std::ostream &os, const std::multiset<K, P, A> &s)
{
    ostream_set(os, s);
    return os;
}

template <typename K, typename P, typename A>
std::istream &operator>> (std::istream &is, std::multiset<K, P, A> &s)
{
    istream_set(is, s);
    return is;
}

/////////////////////////////////////////////////////////////////
//
// std::map, std::multimap
//
/////////////////////////////////////////////////////////////////

// Need to specialize the input; can't just use pair because in a
// a map/multimap the 'second' type is always const.
template <typename T>
void istream_map (std::istream &is, T &s)
{
    istrcaststream iss(is);
    s.clear();
    iss >> "{";
    while (iss)
    {
        // Done?
        iss.skipws();
        char ch = iss->peek();
        if (ch == '}')
            break;

        // Separator
        if (s.size())
            iss >> ",";

        // Value
        descore::ContainerValueHelper<typename T::key_type> key("=");
        descore::ContainerValueHelper<typename T::mapped_type> val(",}");
        iss >> key >> "=>" >> val;
        if (iss)
            s.insert(typename T::value_type(key.val, val.val));
    }
    iss >> "}";
}

// std::pair
template <typename T1, typename T2>
std::ostream &operator<< (std::ostream &os, const std::pair<T1, T2> &p)
{
    ostrcaststream oss(os);
    oss << descore::oswrite_helper(p.first) << "=>" << descore::oswrite_helper(p.second);
    return os;
}

template <typename T1, typename T2>
std::istream &operator>> (std::istream &is, std::pair<T1, T2> &p)
{
    istrcaststream iss(is);
    descore::ContainerValueHelper<T1> val1("=");
    descore::ContainerValueHelper<T2> val2(",}]");
    iss >> val1 >> "=>" >> val2;
    if (iss)
    {
        p.first = val1.val;
        p.second = val2.val;
    }
    return is;
}

// std::map
template <typename K, typename T, typename P, typename A>
std::ostream &operator<< (std::ostream &os, const std::map<K, T, P, A> &s)
{
    ostream_set(os, s);
    return os;
}

template <typename K, typename T, typename P, typename A>
std::istream &operator>> (std::istream &is, std::map<K, T, P, A> &s)
{
    istream_map(is, s);
    return is;
}

// std::multimap
template <typename K, typename T, typename P, typename A>
std::ostream &operator<< (std::ostream &os, const std::multimap<K, T, P, A> &s)
{
    ostream_set(os, s);
    return os;
}

template <typename K, typename T, typename P, typename A>
std::istream &operator>> (std::istream &is, std::multimap<K, T, P, A> &s)
{
    istream_map(is, s);
    return is;
}

#endif // #ifndef strcast_hpp_090214090436

