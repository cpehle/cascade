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
// StringBuffer.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "StringBuffer.hpp"
#include "Archive.hpp"

// Initial size for string buffer.  Does not affect functionality
// as string buffers are resized as necessary.
#define INITIAL_BUFFER_SIZE 64

// Hard upper limit to catch runaway string errors
#define MAX_BUFF_SIZE 0x10000

// Macro to access strdata structure
#define STRDATA ((strdata*)m_buff)[-1]

/////////////////////////////////////////////////////////////////
//
// strbuff()
//
/////////////////////////////////////////////////////////////////
strbuff::strbuff ()
{
    alloc();
}

strbuff::strbuff (const char *fmt, ...)
{
    alloc();
    va_list args;
    va_start(args, fmt);
    vappend(fmt, args);
    va_end(args);
}

void strbuff::alloc ()
{
    m_buff = new char[INITIAL_BUFFER_SIZE + sizeof(strdata)] + sizeof(strdata);
    STRDATA.size = INITIAL_BUFFER_SIZE;
    STRDATA.offset = 0;
    STRDATA.copies = 0;
    *m_buff = 0;
}

strbuff::strbuff (const strbuff &rhs)
: m_buff(rhs.m_buff)
{
    STRDATA.copies++;
}

strbuff &strbuff::operator= (const strbuff &rhs)
{
    release();
    m_buff = rhs.m_buff;
    STRDATA.copies++;
    return *this;
}

/////////////////////////////////////////////////////////////////
//
// ~strbuff()
//
/////////////////////////////////////////////////////////////////
strbuff::~strbuff ()
{
    release();
}

void strbuff::release ()
{
    if (!STRDATA.copies--)
        delete[] (m_buff - sizeof(strdata));
}

/////////////////////////////////////////////////////////////////
//
// clear()
//
/////////////////////////////////////////////////////////////////
void strbuff::clear ()
{
    if (STRDATA.copies)
    {
        STRDATA.copies--;
        alloc();
    }
    else
    {
        STRDATA.offset = 0;
        *m_buff = 0;
    }
}

/////////////////////////////////////////////////////////////////
//
// append()
//
/////////////////////////////////////////////////////////////////
void strbuff::append (const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vappend(fmt, args);
    va_end(args);
}

void strbuff::vappend (const char *fmt, va_list args)
{
    copy();

    while (1)
    {
        va_list args_copy;
        va_copy(args_copy, args);
        int count = vsnprintf(m_buff + STRDATA.offset, STRDATA.size - STRDATA.offset, fmt, args_copy);
        va_end(args_copy);

        if (count > -1 && count < STRDATA.size - STRDATA.offset)
        {
            STRDATA.offset += count;
            return;
        }

        grow();
    }
}

void strbuff::copy ()
{
    // If this string buffer is not exclusively owned then we need to make a copy
    if (STRDATA.copies)
    {
        strdata &prev = STRDATA;
        int size = prev.size + sizeof(strdata);
        m_buff = new char[size] + sizeof(strdata);
        memcpy(&STRDATA, &prev, size);
        prev.copies--;
        STRDATA.copies = 0;
    }
}

void strbuff::grow ()
{
    char *newBuff = new char[STRDATA.size * 2 + sizeof(strdata)] + sizeof(strdata);
    memcpy(newBuff - sizeof(strdata), m_buff - sizeof(strdata), STRDATA.size + sizeof(strdata));
    delete[] (m_buff - sizeof(strdata));
    m_buff = newBuff;
    STRDATA.size *= 2;
    assert(STRDATA.size <= MAX_BUFF_SIZE, "Maximum string buffer size (0x%x) exceeded (infinite loop?)", MAX_BUFF_SIZE);
}

////////////////////////////////////////////////////////////////////////////////
//
// putch()/puts()
//
////////////////////////////////////////////////////////////////////////////////
void strbuff::putch (char ch)
{
    copy();

    if (STRDATA.offset + 1 >= STRDATA.size)
        grow();
    m_buff[STRDATA.offset++] = ch;
    m_buff[STRDATA.offset] = 0;
}

void strbuff::puts (const char *sz)
{
    copy();

    int count = strlen(sz);
    while (count >= STRDATA.size - STRDATA.offset)
        grow();
    memcpy(m_buff + STRDATA.offset, sz, count + 1);
    STRDATA.offset += count;
}

////////////////////////////////////////////////////////////////////////////////
//
// truncate()
//
////////////////////////////////////////////////////////////////////////////////
void strbuff::truncate (int _len)
{
    assert(_len <= STRDATA.offset, "Invalid length for string truncation (longer than current string)");
    assert(_len >= 0, "Invalid negative length for string truncation");

    if (!_len)
    {
        clear();
        return;
    }

    copy();

    STRDATA.offset = _len;
    m_buff[_len] = 0;
}

/////////////////////////////////////////////////////////////////
//
// Archiving
//
/////////////////////////////////////////////////////////////////
void strbuff::archive (Archive &ar)
{
    if (ar.isLoading())
    {
        release();
        int size;
        ar >> size;
        m_buff = new char[size + sizeof(strdata)] + sizeof(strdata);
        STRDATA.size = size;
        ar >> STRDATA.offset;
        STRDATA.copies = 0;
        ar.archiveData(m_buff, STRDATA.offset + 1);
    }
    else
    {
        ar << STRDATA.size << STRDATA.offset;
        ar.archiveData(m_buff, STRDATA.offset + 1);
    }
}

