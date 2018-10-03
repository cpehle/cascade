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
// strcast.cpp
//
// Copyright (C) 2009 D. E. Shaw Research
//
// Authors: J.P. Grossman (jp.grossman@deshawresearch.com)
//          John Salmon   (john.salmon@deshawresearch.com)
// Created: 02/15/2009
//
// Based on string_cast and stringprintf by John Salmon
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "strcast.hpp"

// str() attempts to use a static buffer of this size; it fails over to
// a dynamically allocated buffer that is deleted after the contents
// are copied to a string (which is less efficient)
#define STR_STATIC_BUFFER_SIZE 256

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
std::istream &operator>> (std::istream &is, const char *delimstring)
{
    // If we're skipping whitespace, then skip whitespace in both the stream
    // and delimstring
    if (is.flags() & std::ios_base::skipws)
    {
        for ( ; *delimstring && *delimstring <= ' ' ; delimstring++);
        for ( ; is && is.rdbuf()->in_avail() && is.peek() <= ' ' ; is.get());
    }

    // Match/extract as much of delimstring as possible
    while (*delimstring && is && is.rdbuf()->in_avail() && is.peek() == *delimstring)
    {
        delimstring++;
        is.get();
    }

    // Set the failed bit if we didn't reach the end of delimstring
    if (*delimstring)
        is.setstate(std::ios_base::failbit);

    return is;
}

/////////////////////////////////////////////////////////////////
//
// istrcaststream::initstream()
//
/////////////////////////////////////////////////////////////////
void istrcaststream::initstream ()
{
    // JKS: by clearing basefield, integer conversions are 'as if' done by %i,
    // which does something sensible with 0xb, 11 and 013.
    m_is.unsetf(std::ios::basefield);

    // See http://www.tech-archive.net/Archive/VC/microsoft.public.vc.stl/2007-01/msg00035.html
    // The claim is that in the default C++ locale, the thousands separator is supposed
    // to be ','.  But g++ does not observe this, and in MSVC this is only the case in
    // debug builds, not release builds.  Unfortunately, this causes a failure
    // when trying to read integers separated by commas.  Explicitly set the locale to "C"
    // here, which clears the thousands separator so that reads of comma-separated
    // integers will work.
#ifdef _MSC_VER
#ifdef _DEBUG
    m_is.imbue(std::locale("C"));
#endif
#endif
}

/////////////////////////////////////////////////////////////////
//
// istrcaststream operator>> (bool)
//
/////////////////////////////////////////////////////////////////
template <>
istrcaststream &operator>> (istrcaststream &is, bool &val)
{
    char c;
    is >> c;
    switch (c)
    {
    case 'f':
        is >> "alse";
    case '0':
        val = false;
        break;

    case 't':
        is >> "rue";
    case '1':
        val = true;
        break;

    default:
        is->setstate(std::ios_base::failbit);
        break;
    }
    return is;
}

/////////////////////////////////////////////////////////////////
//
// istrcaststream::skipws()
//
/////////////////////////////////////////////////////////////////
void istrcaststream::skipws ()
{
    for ( ; m_is && m_is.rdbuf()->in_avail() && m_is.peek() <= ' ' ; m_is.get());
}

/////////////////////////////////////////////////////////////////
//
// ostrcaststream::initstream()
//  
/////////////////////////////////////////////////////////////////
void ostrcaststream::initstream ()
{
    // JKS: One could jump through MANY hoops to try to evaluate exactly
    // how many digits are needed.  One day, it might even be
    // standardized as numeric_limits::max_digits10, e.g.,
    // http://www2.open-std.org/JTC1/SC22/WG21/docs/papers/2005/n1822.pdf.
    // But why bother?  It's much better to specify too much precision
    // than too little.  C99 gives us DECIMAL_DIG, which is enough
    // to preserve the value of any floating point type.  C99 didn't
    // bother to specialize for each type (arguably an oversight), so
    // we won't either.
#ifdef DECIMAL_DIG
    m_os.precision(DECIMAL_DIG);
#else
    m_os.precision(21); // enough for a 64-bit, radix-2 significand, e.g., x86 long double
#endif

    // Output booleans as "true"/"false"
    m_os.setf(std::ios_base::boolalpha);
}

/////////////////////////////////////////////////////////////////
//
// str()
//
/////////////////////////////////////////////////////////////////
string str (const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    string ret = vstr(fmt, args);
    va_end(args);
    return ret;
}

string vstr (const char *fmt, va_list args)
{
    char buff[STR_STATIC_BUFFER_SIZE];
    va_list args_copy;

    // First attempt to print into a static buffer.
    // NOTE: vsnprintf is not implemented the same on all platforms, but
    //       all implementations share the following semantics:
    //       - a negative return value indicates failure (not enough space)
    //       - a return value between 0 and n-1 indicates success
    //       - a return value >= n gives the length of the string (not including
    //         the null termination) that would be produced on success.
    va_copy(args_copy, args);
    int ret = vsnprintf(buff, STR_STATIC_BUFFER_SIZE, fmt, args_copy);
    va_end(args_copy);
    if (ret > -1 && ret < STR_STATIC_BUFFER_SIZE)
        return buff;

    // If the static buffer wasn't big enough then use a dynamic buffer.  If
    // vsnprintf returns the required size on failure then we can allocate the
    // exact size and succeed immediately; otherwise we'll keep doubling the 
    // buffer until we succeed.
    int size = (ret > 0) ? (ret + 1) : 2 * STR_STATIC_BUFFER_SIZE;
    for (;;)
    {
        char *buff = new char[size];
        va_copy(args_copy, args);
        ret = vsnprintf(buff, size, fmt, args_copy);
        va_end(args_copy);

        if (ret > -1 && ret < size)
        {
            string strret = buff;
            delete[] buff;
            return strret;
        }
        delete[] buff;
        size *= 2;
    }
}

/////////////////////////////////////////////////////////////////
//
// ContainerValueHelper<string>
//
/////////////////////////////////////////////////////////////////
static void read_delimited_string (istrcaststream &is, descore::delimited_string &val)
{
    std::ostringstream oss;

    // Parse a string, up to top-level whitespace or a delimiter unless it's quoted.
    is.skipws();
    if (is.peek() == '"')
    {
        is->get();
        while (is)
        {
            char ch = is.peek();
            if (ch == '\"')
                break;
            if (ch == '\\')
                is->get();
            ch = is->get();
            oss << ch;
        }
        is->get();
        val.val = oss.str();
    }
    else
    {
        int depth = 0;
        int len = 0;
        int lastNonWhitespace = 0;
        while (!is.eof())
        {
            char ch = is.peek();
            if (!depth && ((!val.allowInternalWhitespace && ch <= ' ') || strchr(val.delimiters, ch)))
                break;
            if (ch == '(' || ch == '{' || ch == '[')
                depth++;
            else if (depth && (ch == ')' || ch == '}' || ch == ']'))
                depth--;
            oss << ch;
            len++;
            if (ch > ' ')
                lastNonWhitespace = len;
            is->get();
        }
        val.val = oss.str();
        if (lastNonWhitespace < len)
            val.val = val.val.substr(0, lastNonWhitespace);
    }
}

std::istream &operator>> (std::istream &is, descore::ContainerValueHelper<string> &val)
{
    istrcaststream iss(is);
    read_delimited_string(iss, val);
    return is;
}

template<>
istrcaststream &operator>> (istrcaststream &is, descore::delimited_string &val)
{
    read_delimited_string(is, val);
    return is;
}

static std::ostream &quote_string (std::ostream &os, const char *ch)
{
    os << '"';
    for ( ; *ch ; ch++)
    {
        if (*ch == '"')
            os << '\\';
        os << *ch;
    }
    os << '"';
    return os;
}

#define MAX_DEPTH 8

std::ostream &operator<< (std::ostream &os, descore::ContainerStringHelper s)
{
    // Pace quotes around the string if:
    //
    // (1) It has any top-level delimiters (white space, ',', '=')
    // (2) It has unbalanced parentheses/brackets/braces
    // (3) The parenthesis/bracket/brace depth is too large
    //
    char parenStack[MAX_DEPTH];
    int parenDepth = 0;
    const char *ch = s.sz;
    for ( ; *ch ; ch++)
    {
        if (*ch == '(' || *ch == '[' ||  *ch == '{')
        {
            if (parenDepth == MAX_DEPTH)
                return quote_string(os, s.sz);
            parenStack[parenDepth++] = *ch + 1 + (*ch != '(');
        }
        else if (*ch == ')' || *ch == ']' || *ch == '}')
        {
            if (!parenDepth || (*ch != parenStack[--parenDepth]))
                return quote_string(os, s.sz);
        }
        else if (!parenDepth && (*ch <= ' ' || *ch == ',' || *ch == '='))
            return quote_string(os, s.sz);
    }
    if (parenDepth)
        return quote_string(os, s.sz);
    os << s.sz;
    return os;
}

BEGIN_NAMESPACE_DESCORE
void empty_error_hook (descore::runtime_error &)
{
}
END_NAMESPACE_DESCORE
