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
// PrintTable.cpp
//
// Copyright (C) 2012 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 01/03/2013
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PrintTable.hpp"

BEGIN_NAMESPACE_DESCORE

#define __A__(n)    a##n
#define __ARG__(n)  Arg __A__(n)
#define __PARG__(n) &__A__(n)
#define __ARGS__    DESCORE_TABLE_ARGS(__ARG__)

#define DECLARE_ARGS \
    Arg last(false);                                        \
    Arg *pargs[] = { DESCORE_TABLE_ARGS(__PARG__), &last }; \
    std::vector<string> args;                               \
    for (int i = 0 ; pargs[i]->valid ; i ++)                \
        args.push_back(pargs[i]->s)

////////////////////////////////////////////////////////////////////////////////
//
// Column()
//
////////////////////////////////////////////////////////////////////////////////
Table::Column::Column (const string &name_flags) : name(name_flags), flags(0)
{
    string::size_type index;
    while ((index = name.rfind('|')) != string::npos)
    {
        flags |= stringTo<ColumnFlags>(name.substr(index+1));
        name = name.substr(0, index);
    }
    width = name.length();
}

////////////////////////////////////////////////////////////////////////////////
//
// Table()
//
////////////////////////////////////////////////////////////////////////////////
Table::Table (__ARGS__)
{
    DECLARE_ARGS;
    m_div.push_back(0);
    for (const string &c : args)
        addColumn(c);
}

////////////////////////////////////////////////////////////////////////////////
//
// addColumn()
//
////////////////////////////////////////////////////////////////////////////////
void Table::addColumn (const string &column)
{
    Column c(column);
    m_columns.push_back(c);
    m_headers.push_back(c.name);
}

////////////////////////////////////////////////////////////////////////////////
//
// addColumns()
//
////////////////////////////////////////////////////////////////////////////////
void Table::addColumns (__ARGS__)
{
    DECLARE_ARGS;
    for (const string &c : args)
        addColumn(c);
}

////////////////////////////////////////////////////////////////////////////////
//
// addDivider()
//
////////////////////////////////////////////////////////////////////////////////
void Table::addDivider ()
{
    m_div[m_rows.size()]++;
}

////////////////////////////////////////////////////////////////////////////////
//
// addRow()
//
////////////////////////////////////////////////////////////////////////////////
void Table::addRow (__ARGS__)
{
    DECLARE_ARGS;
    m_rows.push_back(std::vector<string>());
    m_div.push_back(0);
    for (const string &s : args)
        m_rows.back().push_back(s);
}

////////////////////////////////////////////////////////////////////////////////
//
// rowAppend()
//
////////////////////////////////////////////////////////////////////////////////
void Table::rowAppend (__ARGS__)
{
    assert(m_rows.size(), "addRow() must be called before rowAppend()");
    DECLARE_ARGS;
    for (const string &s : args)
        m_rows.back().push_back(s);
}

////////////////////////////////////////////////////////////////////////////////
//
// print()
//
////////////////////////////////////////////////////////////////////////////////
void Table::print (LogFile f /* = LOG_STDOUT */)
{
    // Are there any headers?
    bool print_header = false;
    for (const string &s : m_headers)
        print_header |= s.size() != 0;

    // Compute the number of columns
    int num_columns = m_headers.size();
    for (const std::vector<string> &row : m_rows)
        num_columns = max(num_columns, (int) row.size());
    if (!num_columns)
    {
        log(f, "<no data>\n");
        return;
    }

    // Fill in any missing columns
    m_columns.resize(num_columns);
    m_headers.resize(num_columns);
    m_columns[0].flags |= ColumnFlags::NODIV;

    // Compute the column widths and alignment
    for (std::vector<string> &row : m_rows)
    {
        row.resize(num_columns);
        for (int col = 0 ; col < num_columns ; col++)
        {
            const string &s = row[col];
            m_columns[col].width = max(m_columns[col].width, (int) s.size());
            if (!(m_columns[col].flags & (ColumnFlags::RALIGN | ColumnFlags::LALIGN)))
            {
                for (char ch : s)
                {
                    if (!strchr("0123456789.-", ch))
                    {
                        m_columns[col].flags |= ColumnFlags::LALIGN;
                        break;
                    }
                }
            }
        }
    }

    // Allocate the print buffer
    int buff_size = 0;
    for (int col = 0 ; col < num_columns ; col++)
        buff_size += m_columns[col].width + 3;
    char *buff = new char[buff_size];

    // Adjust the effective width of the last column
    if ((m_columns[num_columns-1].flags & ColumnFlags::LALIGN) && (m_headers[num_columns-1] != ""))
        m_columns[num_columns-1].width = m_headers[num_columns-1].length();

    // Create the column format strings
    string *fmt = new string[num_columns];
    string div;
    for (int col = 0 ; col < num_columns ; col++)
    {
        int flags = m_columns[col].flags;
        if (!(flags & ColumnFlags::NODIV))
        {
            fmt[col] += "|";
            div += "+";
        }
        if (!(flags & ColumnFlags::NOLPAD))
        {
            fmt[col] += " ";
            div += "-";
        }
        fmt[col] += "%";
        if (flags & ColumnFlags::LALIGN)
            fmt[col] += "-";
        fmt[col] += str(m_columns[col].width);
        fmt[col] += "s";
        for (int i = 0 ; i < m_columns[col].width ; i++)
            div += "-";
        if (!(flags & ColumnFlags::NORPAD))
        {
            fmt[col] += " ";
            div += "-";
        }
    }

    // Print the header
    if (print_header)
    {
        printRow(f, m_headers, fmt, buff);
        m_div[0]++;
    }
    for (int i = 0 ; i < m_div[0] ; i++)
        log(f, "%s\n", *div);

    // Print the rows
    int k = 1;
    for (const std::vector<string> &row : m_rows)
    {
        printRow(f, row, fmt, buff);
        for (int i = 0 ; i < m_div[k] ; i++)
            log(f, "%s\n", *div);
        k++;
    }

    delete[] fmt;
    delete[] buff;
}

////////////////////////////////////////////////////////////////////////////////
//
// printRow()
//
////////////////////////////////////////////////////////////////////////////////
void Table::printRow (LogFile f, const std::vector<string> &row, const string *fmt, char *buff)
{
    char *ch = buff;
    for (const string &s : row)
        ch += sprintf(ch, **fmt++, *s);
    log(f, "%s\n", buff);
}

END_NAMESPACE_DESCORE
