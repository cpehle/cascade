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
// Wildcard.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 04/17/2007
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_Wildcard_hpp_
#define descore_Wildcard_hpp_

/// Fully match two wildcard strings, where '*' matches any sequence of
/// zero or more characters and '?' matches any single character (the two
/// wildcard strings match if there exists some non-wildcard string that
/// matches both of them).
bool wildcardMatch (const char *s1, const char *s2, bool case_sensitive = true);

/// Return the first occurence of a wildcard needle within a non-wildcard
/// haystack, or NULL if the wildcard needle could not be found.
///
/// Note that haystack is *not* treated as a wildcard string, so the 
/// characters '*' and '?' are interpreted literally in haystack.
const char *wildcardFind (const char *haystack, const char *needle, bool case_sensitive = true);

/// Return true if any string that matches the first wildcard string
/// also matches the second wildcard string.
bool wildcardSubsumed (const char *s1, const char *s2, bool case_sensitive = true);

#endif

