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
// AssertParams.hpp
//
// Copyright (C) 2011 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 02/19/2011
//
// Parameters used to control assertions
//
////////////////////////////////////////////////////////////////////////////////

#ifndef AssertParams_hpp_110219123536
#define AssertParams_hpp_110219123536

#include "Parameter.hpp"

BEGIN_NAMESPACE_DESCORE

struct AssertParams
{
    IntParameter (maxWarnings,  5,     
        "The maximum number of warnings allowed before an error is generated");

    BoolParameter(abortOnError, false, 
        "When an error occurs, immediately report the error and exit.  No exception will be thrown.");

    Parameter(std::vector<string>, disabledAssertions, "[]", 
        "List of strings (with wildcards) specifying disabled assertions.  An assertion is disabled if "
        "some substring of its output matches one of the strings in this list.");

    BoolParameter(disableDebugBreakpoint, false, 
        "Do not automatically trigger a breakpoint when running in a debugger and an error occurs");
};

// Instantiated in assert.cpp
extern ParameterGroup(AssertParams, assertParams, "assert");

END_NAMESPACE_DESCORE

#endif // #ifndef AssertParams_hpp_110219123536

