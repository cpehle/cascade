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
// StringBuffer.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
// Lightweight string class that supports efficient append operations
// with printf semantics.
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_StringBuffer_hpp_
#define descore_StringBuffer_hpp_

#include "descore.hpp"

class Archive;

/////////////////////////////////////////////////////////////////
//
// String Buffer
//
/////////////////////////////////////////////////////////////////

class strbuff
{
    // Shared data that lives just before the string buffer
    struct strdata
    {
        int size;    // Current size of string buffer
        int offset;  // Current append offset into buffer
        int copies;  // Number of additional strbuffs sharing this data
    };

public:

    // Create a new string buffer.  Optionally specify an initial string
    // with printf() or vprintf() semantics.
    strbuff ();
    strbuff (const char *fmt, ...) ATTRIBUTE_PRINTF(2);

    // Copy constructor - should only be invoked after string has been
    // constructed.
    strbuff (const strbuff &rhs);
    strbuff &operator= (const strbuff &rhs);

    // Cleanup
    ~strbuff ();

    // Reset the buffer to the empty string
    void clear ();

    // Append to the current string.  printf()/vprintf() semantics.
    void append (const char *fmt, ...) ATTRIBUTE_PRINTF(2);
    void vappend (const char *fmt, va_list args);

    // Append to the current string without printf semantics
    void putch (char ch);
    void puts (const char *sz);

    // Truncate the string
    void truncate (int len);

    // Length of current string
    inline int len () const
    {
        return ((strdata*)m_buff)[-1].offset;
    }

    // Retrieve the current string
    inline const char * operator* () const
    {
        return m_buff;
    }
    inline operator const char * () const
    {
        return m_buff;
    }

    // Comparison operators
    inline bool operator== (const char *rhs) const
    {
        return !strcmp(m_buff, rhs);
    }
    inline bool operator!= (const char *rhs) const
    {
        return strcmp(m_buff, rhs) != 0;
    }
    inline bool operator== (const strbuff &rhs) const
    {
        return !strcmp(m_buff, rhs);
    }
    inline bool operator!= (const strbuff &rhs) const
    {
        return strcmp(m_buff, rhs) != 0;
    }
    inline bool operator== (const string &rhs) const
    {
        return !strcmp(m_buff, *rhs);
    }
    inline bool operator!= (const string &rhs) const
    {
        return strcmp(m_buff, *rhs) != 0;
    }

    // Archiving
    void archive (Archive &ar);

private:
    void alloc ();
    void release ();
    void copy ();
    void grow ();

private:
    char *m_buff;    // Pointer to buffer
};

#ifdef DECLARE_ARCHIVE_OBJECT
DECLARE_ARCHIVE_OBJECT(strbuff);
#endif

#endif

