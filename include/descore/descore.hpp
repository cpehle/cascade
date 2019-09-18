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
// descore.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/20/2007
//
// Common includes
//
//////////////////////////////////////////////////////////////////////

#ifndef descore_descore_hpp_
#define descore_descore_hpp_

// Disable a bunch of warnings
#define _CRT_SECURE_NO_DEPRECATE

#ifndef __GNUC__
// Allow nameless struct/union
#pragma warning(disable: 4201)
#endif

// The MSVC compiler warning for possible loss of data unfortunately fires
// in expressions like a += b where a and b are both bytes.  This is, of course,
// absolutely correct, and a limitation of ANSI C in that this expression is
// identical to a = a + b, where 'a + b' is automatically cast to an unsigned
// int.  It is unfortunate because we don't want to limit our use of a += b,
// particularly when the LHS is big (e.g. a function call that returns a ref).
//
// The warning is already slightly inconsistent to begin with in that it fires
// when an int is assigned to a byte, but not when an int is assigned to a 
// bitfield of an int32.  It also fires when an enum is assigned to a byte, 
// even if the possible values of the enum are all less than 256.
#ifndef __GNUC__
#pragma warning(disable: 4244)
#endif

// Allow constant conditional expressions.  There are two main cases where
// this occurs.  The first is while (1).  It's not a big deal to change
// this to for (;;), but while (1) *is* a common idiom.  The more important
// one is 'if' clauses in a templated function that we *want* to be 
// optimized out.
#ifndef __GNUC__
#pragma warning(disable: 4127)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <set>
#include <vector>
#include <map>
#include <list>
#include <string>
#include <stdexcept>
#include <sstream>
#include <deque>

using std::max;
using std::min;
using std::string;

// Only include the very basic stuff from here
#include "defines.hpp"
#include "type_traits.hpp"
#include "stltree.hpp"
#include "assert.hpp"
#include "strcast.hpp"
#include "Log.hpp"

#endif
