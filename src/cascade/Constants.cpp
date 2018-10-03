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
// Constants.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/22/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Cascade.hpp"
#include "Constants.hpp"

////////////////////////////////////////////////////////////////////////////////
//
// less
//
////////////////////////////////////////////////////////////////////////////////
namespace std
{
    bool less<Cascade::Constant *>::operator () (Cascade::Constant *lhs, Cascade::Constant *rhs) const
    {
        int size = min(lhs->size(), rhs->size());
        return memcmp(lhs->data(), rhs->data(), size) < 0;
    }
}

BEGIN_NAMESPACE_CASCADE

Constant::Constants Constant::s_constants;
byte *Constant::s_data = NULL;
int Constant::s_size = 0;

#define MIN(a,b) ((a)<(b)?(a):(b))

//////////////////////////////////////////////////////////////////
//
// Constant()
//
//////////////////////////////////////////////////////////////////
Constant::Constant (int size, const byte *data, bool copyData) :
m_dataOwner(copyData), m_size(size), m_data(copyData ? new byte[size] : (byte *) data)
{
    if (copyData)
        memcpy(m_data, data, size);
}

//////////////////////////////////////////////////////////////////
//
// ~Constant()
//
//////////////////////////////////////////////////////////////////
Constant::~Constant ()
{
    if (m_dataOwner)
        delete[] m_data;
}

//////////////////////////////////////////////////////////////////
//
// resize()
//
//////////////////////////////////////////////////////////////////
void Constant::resize (int size, const byte *data)
{
    assert(m_dataOwner);
    assert(m_size < size);
    assert(!memcmp(m_data, data, m_size));

    delete[] m_data;
    m_data = new byte[size];
    m_size = size;
    memcpy(m_data, data, size);
}

//////////////////////////////////////////////////////////////////
//
// getConstant()
//
//////////////////////////////////////////////////////////////////
const Constant *Constant::getConstant (int size, const byte *data)
{
    Constant temp(size, data, false);
    Constants::iterator it = s_constants.find(&temp);
    Constant *ret;
    if (it == s_constants.end())
    {
        // Couldn't find the constant, so insert a new one
        ret = new Constant(size, data, true);
        s_constants.insert(ret);
    }
    else
    {
        // Return the existing constant, resizing it if necessary
        ret = *it;
        if (size > ret->m_size)
            ret->resize(size, data);
    }
    return ret;
}

//////////////////////////////////////////////////////////////////
//
// initConstants()
//
//////////////////////////////////////////////////////////////////
static int align (int offset, int size)
{
    if (size == 1)
        return 0;
    if (size == 2)
        return offset & 1;
    return -offset & 3;
}

void Constant::initConstants ()
{
    // Determine how much size we need
    int offset = 0;
    for (Constant *c : s_constants)
        offset += align(offset, c->m_size) + c->m_size;

    // Allocate the constant array
    s_size = offset;
    Sim::stats.numConstantBytes = s_size;
    s_data = new byte[s_size];

    // Copy the constants and redirect the data pointers
    offset = 0;
    for (Constant *c : s_constants)
    {
        offset += align(offset, c->m_size);
        byte *data = s_data + offset;
        memcpy(data, c->m_data, c->m_size);
        delete[] c->m_data;
        c->m_data = data;
        c->m_dataOwner = false;
        offset += c->m_size;
    }
}

//////////////////////////////////////////////////////////////////
//
// cleanup()
//
//////////////////////////////////////////////////////////////////
void Constant::cleanup ()
{
    for (Constant *c : s_constants)
        delete c;
    s_constants.clear();
    delete[] s_data;
    s_data = NULL;
}

/////////////////////////////////////////////////////////////////
//
// isConstant()
//
/////////////////////////////////////////////////////////////////
bool Constant::isConstant (const void *data)
{
    const byte *ptr = (const byte *) data;
    return ((unsigned) (ptr - s_data)) < (unsigned) s_size;
}

END_NAMESPACE_CASCADE

