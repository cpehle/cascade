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
// PrintStrings.hpp
//
// Copyright (C) 2011 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 04/02/2011
//
// Pretty-print a set of strings in ls-style format
//
////////////////////////////////////////////////////////////////////////////////

#ifndef PrintStrings_hpp_110402064856
#define PrintStrings_hpp_110402064856

#include "defines.hpp"
#include <stdio.h>
#include <vector>
#include <set>
#include <map>
#include <string>
using std::string;

BEGIN_NAMESPACE_DESCORE

void printStrings (const std::set<string>    &strings, FILE *fout = stdout);
void printStrings (const std::vector<string> &strings, FILE *fout = stdout);

template <typename T>
void printStrings (const std::map<string, T> &strings, FILE *fout = stdout)
{
    std::vector<string> vecstrings;
    for (const auto &kv : strings)
        vecstrings.push_back(kv.first);
    printStrings(vecstrings, fout);
}

// Return the number of text columns in the current output console
int getConsoleWidth ();

END_NAMESPACE_DESCORE

#endif // #ifndef PrintStrings_hpp_110402064856

