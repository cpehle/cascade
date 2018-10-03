/*
Copyright 2010, D. E. Shaw Research.
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
// hex.hpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 02/01/2010
//
// Arrays for printing/parsing hex digits
//
//////////////////////////////////////////////////////////////////////

#ifndef hex_hpp_100201062426
#define hex_hpp_100201062426

#include "defines.hpp"

BEGIN_NAMESPACE_DESCORE

// hex_to_sz[d] returns a null-terminated single-character string containing
// the hex digit corresponding to d, with a-f in lower-case.
extern const char hex_to_sz[16][2];

// Same as above, but return A-F in upper-case.
extern const char hex_to_sz_upper[16][2];

// hex_to_ch returns the single character hex digit corresponding to d, with
// a-f in lower case.
extern const char hex_to_ch[16];

// Same as above, but return A-F in upper-case.
extern const char hex_to_ch_upper[16];

// Convert a hex character to its numerical equivalent (0-15), or return -1
// for non-hex characters.  The array is pre-padded with 128 -1's, so when
// convenient it's safe to index it with a char without worrying about negative
// indices.
extern const int8 * const ch_to_hex;

// Convenience helpers
inline bool is_hex (char ch)
{
    return ch_to_hex[(int) ch] != -1;
}
inline int8 to_hex (char ch)
{
    return ch_to_hex[(int) ch];
}

// Helper function to parse hex constants.  MUCH faster than scanf("%x").
// Returns the number of characters that were processed.
template <typename T>
int parse_hex (const char *ch, T &val)
{
    int n = 0;
    for (val = 0 ; ch_to_hex[(int) ch[n]] != -1 ; n++)
        val = (val << 4) | ch_to_hex[(int) ch[n]];
    return n;
}

END_NAMESPACE_DESCORE

#endif // #ifndef hex_hpp_100201062426

