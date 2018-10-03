/*
Copyright 2011, D. E. Shaw Research.
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
// Enumeration.cpp
//
// Copyright (C) 2011 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 12/15/2011
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Enumeration.hpp"
#include "StringBuffer.hpp"
#include "Iterators.hpp"

BEGIN_NAMESPACE_DESCORE

#define DIGIT(ch) ((ch) >= '0' && (ch) <= '9')
#define UPPER(ch) ((ch) >= 'A' && (ch) <= 'Z')
#define LOWER(ch) ((ch) >= 'a' && (ch) <= 'z')
#define ALPHA(ch) (UPPER(ch) || LOWER(ch) || ((ch) == '_'))
#define ALPHANUM(ch) (DIGIT(ch) || ALPHA(ch))

static string substr (const char *ch, int len)
{
    char *sz = new char[len + 1];
    memcpy(sz, ch, len);
    sz[len] = 0;
    string ret = sz;
    delete[] sz;
    return ret;
}

static string parse_symbol (char *&ch)
{
    const char *start = ch;
    for (ch++ ; ALPHANUM(*ch) ; ch++);
    return substr(start, ch - start);
}

static void skipws (char *&ch)
{
    for ( ; *ch && *ch <= ' ' ; ch++);
}

static int eval (char *expr, std::map<string, int> &symbols);

////////////////////////////////////////////////////////////////////////////////
//
// EnumerationType()
//
////////////////////////////////////////////////////////////////////////////////
EnumerationType::EnumerationType (const char *name, const char *values, int numValues)
: m_name(name),
m_numValues(numValues)
{
    std::map<string, int> symbols;
    m_values = new string[numValues];
    char *myvalues = new char[strlen(values)+1];
    strcpy(myvalues, values);
    char *ch = myvalues;
    skipws(ch);
    int nextval = 0;
    while (*ch)
    {
        // Parse the symbol name
        assert(ALPHA(*ch), "Could not parse enumeration values for %s\n"
            "    Syntax error at: %s", name, ch);
        string value = parse_symbol(ch);
        skipws(ch);

        // Explicit value assignment?
        if (*ch == '=')
        {
            ch++;
            skipws(ch);
            char *end = strchr(ch, ',');
            bool comma = (end != NULL);
            if (!comma)
                end = ch + strlen(ch);
            *end = 0;
            int val;
            try
            {
                val = eval(ch, symbols);
            }
            catch (descore::runtime_error &e)
            {
                e.append("\n    while attempting to parse %s = %s", *value, ch);
                throw;
            }                
            assert(!nextval || (val >= nextval), "Could not parse enumeration values for %s\n"
                "   %s = %d makes the enumeration non-increasing", name, *value, val);
            assert(val < numValues, "Could not parse enumeration values for %s\n"
                "    %s = %d is out of range", name, *value, val);
            ch = end;
            if (comma)
                *end = ',';
            nextval = val;
        }

        // Set the value
        symbols[value] = nextval;
        m_values[nextval++] = value;

        // See if we're done
        if (*ch)
        {
            assert(*ch == ',', "Could not parse enumeration values for %s\n"
                "    Expected ',' at %s", name, ch);
            ch++;
            skipws(ch);
        }
    }
    delete[] myvalues;
}

////////////////////////////////////////////////////////////////////////////////
//
// getName()
//
////////////////////////////////////////////////////////////////////////////////
string EnumerationType::getName () const
{
    return m_name;
}

////////////////////////////////////////////////////////////////////////////////
//
// getValue()
//
////////////////////////////////////////////////////////////////////////////////
int EnumerationType::getValue (const string &symbol) const
{
    for (int i = 0 ; i < m_numValues ; i++)
    {
        if (symbol == m_values[i])
            return i;
    }
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
//
// getSymbol()
//
////////////////////////////////////////////////////////////////////////////////
string EnumerationType::getSymbol (int value) const
{
    if (!isValid(value))
        return m_name + "::???";
    return m_values[value];
}

////////////////////////////////////////////////////////////////////////////////
//
// maxValue()
//
////////////////////////////////////////////////////////////////////////////////
int EnumerationType::maxValue () const
{
    return m_numValues - 1;
}

////////////////////////////////////////////////////////////////////////////////
//
// isValid()
//
////////////////////////////////////////////////////////////////////////////////
bool EnumerationType::isValid (int value) const
{
    return (unsigned(value) < unsigned(m_numValues)) && **m_values[value];
}

////////////////////////////////////////////////////////////////////////////////
//
// getValesAsString()
//
////////////////////////////////////////////////////////////////////////////////
string EnumerationType::getValuesAsString () const
{
    strbuff values;
    bool first = true;
    for (int i = 0 ; i < m_numValues ; i++)
    {
        if (**m_values[i])
        {
            if (!first)
                values.puts(", ");
            first = false;
            values.append("%s", *m_values[i]);
        }
    }
    return *values;
}

////////////////////////////////////////////////////////////////////////////////
//
// parseValue()
//
////////////////////////////////////////////////////////////////////////////////
int EnumerationType::parseValue (istrcaststream &is) const
{
    descore::delimited_string s(".,[](){}|&^+-*/");
    is >> s;
    int v = getValue(s);
    assert_throw(strcast_error, v != -1, "Cannot convert \"%s\" to %s\n    Valid values are %s",
        *s, *m_name, *getValuesAsString());
    return v;
}

////////////////////////////////////////////////////////////////////////////////
//
// parseExpr()
//
////////////////////////////////////////////////////////////////////////////////

static char *split_top (char *expr, const char *op)
{
    for ( ; strchr("+-!~", *expr) || (*expr && *expr <= ' ') ; expr++);
    int depth = 0;
    for ( ; *expr ; expr++)
    {
        if (*expr == '(')
            depth++;
        else if (*expr == ')')
            depth--;
        else if (!depth && (*expr == *op) && (!op[1] || (expr[1] == op[1])))
        {
            // Special case: don't allow '<' to match '<<', or '>' to match '>>'
            if (op[1] || (expr[1] != '<' && expr[1] != '>' && expr[-1] != '<' && expr[-1] != '>'))
            {
                *expr = 0;
                return expr;
            }
        }
    }
    return NULL;
}

int eval (char *expr, std::map<string, int> &symbols)
{
    char *ch = expr;
    skipws(ch);

    // Ternary operation?
    char *ch2 = split_top(ch, "?");
    if (ch2)
    {
        char *ch3 = split_top(ch2, ":");
        assert(ch3);
        return eval(ch, symbols) ? eval(ch2 + 1, symbols) : eval(ch3 + 1, symbols);
    }

    // Binary operation?
#define _(op) \
    if ((ch2 = split_top(ch, #op)) != NULL) \
        return eval(ch, symbols) op eval(ch2 + strlen(#op), symbols);
    _(||)_(&&)_(|)_(^)_(&)_(!=)_(==)_(>=)_(<=)_(>)_(<)_(>>)_(<<)_(-)_(+)_(%)_(/)_(*);

    // Unary operation?
#undef _
#define _(op) if (*ch == *#op) return op eval(ch + 1, symbols);
    _(+)_(-)_(!)_(~);

    // Chomp trailing whitespace
    ch2 = ch + strlen(ch);
    for (ch2-- ; ch2 >= ch && *ch2 <= ' ' ; ch2--);
    ch2[1] = 0;

    // Parentheses?
    if (*ch == '(')
    {
        assert(*ch2 == ')');
        *ch2 = 0;
        return eval(ch + 1, symbols);
    }

    // Symbol?
    string val = ch;
    if (symbols.find(val) != symbols.end())
        return symbols[val];

    // Integer?
    return stringTo<int>(val);
}

END_NAMESPACE_DESCORE
