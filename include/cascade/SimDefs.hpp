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
// SimDefs.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
//////////////////////////////////////////////////////////////////////

#ifndef SimDefs_hpp
#define SimDefs_hpp

#define _CASCADE

////////////////////////////////////////////////////////////////////////
//
// Constants
//
////////////////////////////////////////////////////////////////////////

#define CASCADE_VERSION "1.0"
#define CASCADE_DATE    "March 10, 2013"

// Maximum port size in bits
const unsigned CASCADE_MAX_PORT_SIZE = 0x10000;

// Maximum port delay in cycles
const unsigned CASCADE_MAX_PORT_DELAY = 0x4000;

// Default clock period (ps)
const unsigned CASCADE_DEFAULT_CLOCK_PERIOD = 1000;

// Predefined reset levels
const int RESET_COLD = 0;
const int RESET_WARM = 1;

////////////////////////////////////////////////////////////////////////
//
// Macros
//
////////////////////////////////////////////////////////////////////////

#define BEGIN_NAMESPACE_CASCADE namespace Cascade {
#define END_NAMESPACE_CASCADE };

/////////////////////////////////////////////////////////////////
//
// Assertions
//
/////////////////////////////////////////////////////////////////
#define CascadeValidate(exp, ...) descore_error(descore::runtime_error, exp, ::getAssertionContext(), true, str(__VA_ARGS__))

////////////////////////////////////////////////////////////////////////
//
// Helper Functions
//
////////////////////////////////////////////////////////////////////////

BEGIN_NAMESPACE_CASCADE

// Destroy a linked list
template <typename T>
void destroyList (T *&first)
{
    while (first)
    {
        T *temp = first;
        first = temp->next;
        delete temp;
    }
}

// Convert a string to a const char *
inline const char *toConstCharStar (const char *s)
{
    return s;
}
inline const char *toConstCharStar (const string &s)
{
    return *s;
}
inline const char *toConstCharStar (const strbuff &s)
{
    return *s;
}

END_NAMESPACE_CASCADE

#endif

