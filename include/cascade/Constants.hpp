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
// Constants.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// Dynamically create storage for constants that can be accessed by ports.
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_Constants_hpp_
#define Cascade_Constants_hpp_

#include "SimDefs.hpp"

BEGIN_NAMESPACE_CASCADE
class Constant;
END_NAMESPACE_CASCADE

// Comparison operator for sets of constants
namespace std
{
    template <> struct less <Cascade::Constant *>
    {
        bool operator() (Cascade::Constant *lhs, Cascade::Constant *rhs) const;
    };
}

BEGIN_NAMESPACE_CASCADE

// Wrapper class for an actual constant
class Constant
{
    DECLARE_NOCOPY(Constant);

    // Constructor/destructor
    Constant (int size, const byte *data, bool copyData);
    ~Constant ();

public:
    // Make a constant bigger keeping the low bits the same
    void resize (int size, const byte *data);

    // Accessors
    inline int size() const { return m_size; }
    inline const byte *data() const { return m_data; }

private:
    bool m_dataOwner;
    int  m_size;
    byte *m_data;

public:
    // Allocate a new constant or find an existing one
    static const Constant *getConstant (int size, const byte *data);

    // Allocate the constant array and update the data pointers
    static void initConstants ();

    // Delete the space allocated for the constants
    static void cleanup ();

    // Determine if a pointer points to a constant
    static bool isConstant (const void *data);

private:
    typedef std::set<Constant *> Constants;
    static Constants s_constants;

    static byte *s_data; // Storage for constants
    static int s_size;   // Size of storage
};   

END_NAMESPACE_CASCADE

#endif

