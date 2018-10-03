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
// StringTable.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
// Store a set of static strings so that each string is stored
// at most once.
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_StringTable_hpp_
#define descore_StringTable_hpp_

#include "defines.hpp"
#include <set>
#include <string.h>

BEGIN_NAMESPACE_DESCORE

/////////////////////////////////////////////////////////////////
//
// StringTableEntry
//
/////////////////////////////////////////////////////////////////
struct StringTableEntry
{
    const char *string;

    inline bool operator< (const StringTableEntry &rhs) const
    {
        return strcmp(string, rhs.string) < 0;
    }
};

/////////////////////////////////////////////////////////////////
//
// StringTable
//
/////////////////////////////////////////////////////////////////
class StringTable 
{
public:
    ~StringTable ();

    // Insert a new string into the table, and return a pointer to
    // the string that can be used for the lifetime of the table.
    const char *insert (const char *string);

    // Clear the table (invalidates all pointers that were returned)
    void clear ();

private:
    typedef std::set<StringTableEntry> StringSet;
    StringSet m_table;
};

END_NAMESPACE_DESCORE


#endif

