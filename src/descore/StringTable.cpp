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
// StringTable.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "StringTable.hpp"

BEGIN_NAMESPACE_DESCORE

/////////////////////////////////////////////////////////////////
//
// ~StringTable()
//
/////////////////////////////////////////////////////////////////
StringTable::~StringTable ()
{
    clear();
}

/////////////////////////////////////////////////////////////////
//
// insert()
//
/////////////////////////////////////////////////////////////////
const char *StringTable::insert (const char *string)
{
    if (!string)
        return NULL;

    StringTableEntry entry = { string };

    StringSet::iterator it = m_table.find(entry);
    if (it != m_table.end())
        return it->string;

    char *newString = new char[strlen(string) + 1];
    entry.string = newString;
    strcpy(newString, string);
    m_table.insert(entry);
    return newString;
}

/////////////////////////////////////////////////////////////////
//
// clear()
//
/////////////////////////////////////////////////////////////////
void StringTable::clear()
{
    for (const StringTableEntry &s : m_table)
        delete[] s.string;
    m_table.clear();
}

END_NAMESPACE_DESCORE
