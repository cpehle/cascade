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
// PrintStrings.cpp
//
// Copyright (C) 2011 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 04/02/2011
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PrintStrings.hpp"

#if defined (_MSC_VER)
#include <windows.h>
#else
#include <termcap.h>
#endif

BEGIN_NAMESPACE_DESCORE

int getConsoleWidth ()
{
    int width = 80;
#ifdef _MSC_VER
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE); 
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo; 
    if (GetConsoleScreenBufferInfo(hStdout, &csbiInfo)) 
        width = csbiInfo.dwSize.X;
#else
    char *termtype = getenv("TERM");
    if (!termtype)
        termtype = (char*)"dumb";
    if (tgetent(NULL, termtype))
        width = tgetnum("co");
#endif
    if (width <= 0)
        width = 80;
    return width;
}

void printStrings (const std::vector<string> &strings, FILE *fout)
{
    std::set<string> sorted;
    for (const string &s : strings)
        sorted.insert(s);
    printStrings(sorted, fout);
}

void printStrings (const std::set<string> &strings, FILE *fout)
{
    int width = getConsoleWidth();
    int numTests = (int) strings.size();
    const char **names = new const char *[numTests];
    int i = 0;
    for (const string &s : strings)
        names[i++] = *s;

    // Display the strings using ls-style formatting
    int *colWidth = new int[numTests];
    for (int nrows = 1 ; nrows <= numTests ; nrows++)
    {
        int ncols = (numTests + nrows - 1) / nrows;

        // Compute the column widths
        int totalWidth = 0;
        for (i = 0 ; i < ncols ; i++)
        {
            colWidth[i] = 0;
            for (int j = 0 ; j < nrows && (nrows * i + j) < numTests ; j++)
            {
                int required = (int) strlen(names[nrows * i + j]);
                if (nrows * (i + 1) < numTests)
                    required += 2;
                if (required > colWidth[i])
                    colWidth[i] = required;
            }
            totalWidth += colWidth[i];
        }

        // Display and exit if we fit or if nrows == numTests
        if (totalWidth < width || nrows == numTests)
        {
            for (int j = 0 ; j < nrows ; j++)
            {
                for (i = 0 ; i < ncols && (nrows * i + j) < numTests ; i++)
                {
                    char fmt[8];
                    sprintf(fmt, "%%-%ds", colWidth[i]);
                    fprintf(fout, fmt, names[nrows * i + j]);
                }
                fprintf(fout, "\n");
            }
            delete[] names;
            delete[] colWidth;
            return;
        }
    }

    // Could get here if there are zero strings
    delete[] names;
    delete[] colWidth;
}


END_NAMESPACE_DESCORE
