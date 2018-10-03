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
// Wildcard.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 04/17/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Wildcard.hpp"

#define MAYBE_TOLOWER(ch) (case_sensitive ? (ch) : tolower(ch))

////////////////////////////////////////////////////////////////////////////////
//
// allStar()
//
////////////////////////////////////////////////////////////////////////////////
static bool allStar (const char *ch)
{
    for ( ; *ch ; ch++)
    {
        if (*ch != '*')
            return false;
    }
    return true;
}


////////////////////////////////////////////////////////////////////////////////
//
// wildcardMatch()
//
////////////////////////////////////////////////////////////////////////////////
bool wildcardMatch (const char *s1, const char *s2, bool case_sensitive)
{
    const char *ch;

    if (*s1 == '*')
    {
        s1++;
        if (!*s1)
            return true;
        if (!*s2)
            return allStar(s1);
        for (ch = s2 ; *ch ; ch++)
        {
            if (wildcardMatch(s1, ch, case_sensitive))
                return true;
        }
        return false;
    }

    if (*s2 == '*')
    {
        s2++;
        if (!*s2)
            return true;
        if (!*s1)
            return allStar(s2);
        for (ch = s1 ; *ch ; ch++)
        {
            if (wildcardMatch(ch, s2, case_sensitive))
                return true;
        }
        return false;
    }

    if (!*s1)
        return !*s2;
    if (!*s2)
        return !*s1;

    if ((MAYBE_TOLOWER(*s1) == MAYBE_TOLOWER(*s2)) || (*s1 == '?') || (*s2 == '?'))
        return wildcardMatch(s1+1, s2+1, case_sensitive);
    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// wildcardPartialMatch()
//
// s1 is NOT a wildcard string
//
////////////////////////////////////////////////////////////////////////////////
static bool wildcardPartialMatch (const char *s1, const char *s2, bool case_sensitive)
{
    const char *ch;

    if (*s2 == '*')
    {
        s2++;
        if (allStar(s2))
            return true;
        for (ch = s1 ; *ch ; ch++)
        {
            if (wildcardPartialMatch(ch, s2, case_sensitive))
                return true;
        }
        return false;
    }

    if (!*s1)
        return !*s2;
    if (!*s2)
        return true;

    if ((MAYBE_TOLOWER(*s1) == MAYBE_TOLOWER(*s2)) || (*s2 == '?'))
        return wildcardPartialMatch(s1+1, s2+1, case_sensitive);
    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// wildcardFind()
//
// haystack is NOT a wildcard string
//
////////////////////////////////////////////////////////////////////////////////
const char *wildcardFind (const char *haystack, const char *needle, bool case_sensitive)
{
    if (allStar(needle))
        return haystack;
    for (const char *ch = haystack ; *ch ; ch++)
    {
        if (wildcardPartialMatch(ch, needle, case_sensitive))
            return ch;
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// wildcardSubsumed()
//
////////////////////////////////////////////////////////////////////////////////

// Helper function used when a sequence of wildcards has been stripped from s2
// that must match >= n characters.
static bool wildcardSubsumed (const char *s1, const char *s2, int n, bool case_sensitive)
{
    if (!*s1)
        return !n && !*s2;

    if (wildcardSubsumed(s1+1, s2, n, case_sensitive))
        return true;

    if (*s1 == '*')
        return false;

    if (n)
        return wildcardSubsumed(s1+1, s2, n-1, case_sensitive);
    
    return (MAYBE_TOLOWER(*s1) == MAYBE_TOLOWER(*s2)) && wildcardSubsumed(s1+1, s2+1, case_sensitive);
}

bool wildcardSubsumed (const char *s1, const char *s2, bool case_sensitive)
{
    if (!*s1)
        return allStar(s2);

    if (*s2 == '*' || *s2 == '?')
    {
        int n = 0;
        for ( ; *s2 == '*' || *s2 == '?' ; s2++)
        {
            if (*s2 == '?')
                n++;
        }
        return wildcardSubsumed(s1, s2, n, case_sensitive);
    }
    
    return (MAYBE_TOLOWER(*s1) == MAYBE_TOLOWER(*s2)) && wildcardSubsumed(s1+1, s2+1, case_sensitive);
}
