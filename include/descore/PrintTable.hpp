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
// PrintTable.hpp
//
// Copyright (C) 2012 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 01/03/2013
//
// Pretty-print a table
//
////////////////////////////////////////////////////////////////////////////////

#ifndef PrintTable_hpp_737004071327947834
#define PrintTable_hpp_737004071327947834

#include "Log.hpp"
#include "Enumeration.hpp"

BEGIN_NAMESPACE_DESCORE

// To increase the number of arguments accepted by the various vararg-style functions, just add more
// terms to DESCORE_TABLE_ARGS.

#define DESCORE_TABLE_ARGS(X) X(1),X(2),X(3),X(4),X(5),X(6),X(7),X(8),X(9),X(10),X(11),X(12),X(13),X(14),X(15)
#define __ARG__(n) Arg a##n = false
#define __ARGS__ DESCORE_TABLE_ARGS(__ARG__)

////////////////////////////////////////////////////////////////////////////////
//
// Column flags
//
////////////////////////////////////////////////////////////////////////////////
DECLARE_ENUMERATION(ColumnFlags,
        NODIV   = 1,  // Don't separate this column from the previous one with a divider
        NOLPAD  = 2,  // Don't internally pad this column with a space on the left
        NORPAD  = 4,  // Don't internally pad this column with a space on the right
        NOPAD   = 6,  // Don't internally pad this column with a space on either side
        RALIGN  = 8,  // Right-align this column (default for numeric columns)
        LALIGN  = 16  // Left-align this column (default for non-numeric columns)
    );

////////////////////////////////////////////////////////////////////////////////
//
// Table
//
////////////////////////////////////////////////////////////////////////////////
class Table
{
    struct Column
    {
        Column (const string &name_flags = "");
        
        string name;
        int    flags;
        int    width;
    };

    struct Arg
    {
        Arg (bool)             : s(""), valid(false) {}
        Arg (const string &_s) : s(_s), valid(true) {}
        Arg (const char *_s)   : s(_s), valid(true) {}

        string s;
        bool   valid;
    };

public:

    Table (__ARGS__);

    // Add one or more columns
    void addColumn  (const string &column);
    void addColumns (__ARGS__);

    // Add a divider after the current row
    void addDivider ();

    // Add a new row/append to the current row
    void addRow    (__ARGS__);
    void rowAppend (__ARGS__);

    // Print the table
    void print (LogFile f = LOG_STDOUT);

private:
    void printRow (LogFile f, const std::vector<string> &row, const string *fmt, char *buff);

private:
    std::vector<Column>            m_columns; // Table columns
    std::vector<string>            m_headers; // Column names
    std::list<std::vector<string>> m_rows;    // Table rows
    std::vector<int>               m_div;     // Print dividers after row k-1
};

#undef __ARG__
#undef __ARGS__

END_NAMESPACE_DESCORE

#endif // #ifndef PrintTable_hpp_737004071327947834
