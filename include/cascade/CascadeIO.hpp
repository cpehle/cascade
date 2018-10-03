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
// CascadeIO.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
//////////////////////////////////////////////////////////////////////

#ifndef CascadeIO_hpp_
#define CascadeIO_hpp_

#include "Ports.hpp"
#include "Hierarchy.hpp"

// Native port types
template <typename T> class Input;
template <typename T> class Output;
template <typename T> class InOut;
template <typename T> class Register;

////////////////////////////////////////////////////////////////////////
//
// Helper macros for declaring specific port types
//
////////////////////////////////////////////////////////////////////////

// Helper macro to allow arrays of ports
#define DECLARE_PORT_ARRAY_ENTRY(type, dir) \
    static bool addPortArrayEntry (const void *arrayAddress) \
{ \
    return Cascade::Hierarchy::addPortArray(Cascade::dir, arrayAddress, Cascade::getPortInfo<T>(), sizeof(type<byte>)); \
}

// Disable all port connections by default (by making the connection operators private),
// then selectively enable allowed port connections.  A compiler error of the form
//
//   'InOut<T>::operator <<' : cannot access private member declared in class 'InOut<T>'
//
// indicates one of two connection errors:
//
// 1. An attempt to connect two ports of different types
// 2. An illegal InOut connection (InOut ports can only be non-synchronously 
//    connected to other InOut ports of the same type)
//
#define DECLARE_PORT(type, dir) \
public: \
    typedef Cascade::Port<T> port_t; \
    strbuff getName () const { return port_t::getName(); } \
    string getAssertionContext () const { return *getName(); } \
private: \
    void operator<< (Cascade::PortBase &rhs); \
    void operator<= (Cascade::PortBase &rhs); \
    type<T> (const type<T> &rhs); \
public: \
    inline T operator= (typename port_t::value_t data) { return port_t::operator= (data); } \
    template <typename T1> \
    inline T operator= (const Cascade::Port<T1> &rhs) { return *this = rhs(); } \
    inline T operator= (const type<T> &rhs) { return *this = rhs(); } \
    DECLARE_PORT_ARRAY_ENTRY(type, dir) \
    type<T> () : port_t(Cascade::dir) {}

// Macros for declaring connections
#define ALLOW_CONNECT(type) \
    type<T> &operator<< (type<T> &rhs) \
    { \
        assert_always(Sim::state == Sim::SimConstruct); \
        port_t::wrapper->connect(rhs.port_t::wrapper, 0); \
        return rhs; \
    }

#define ALLOW_COMB_CONNECTIONS() \
    ALLOW_CONNECT(Input) \
    ALLOW_CONNECT(Output) \
    ALLOW_CONNECT(InOut) \
    ALLOW_CONNECT(Register) \

#define ALLOW_SYNC_CONNECT(type) \
    type<T> &operator<= (type<T> &rhs) \
    { \
        assert_always(Sim::state == Sim::SimConstruct); \
        port_t::wrapper->connect(rhs.port_t::wrapper, 1); \
        return rhs; \
    }

#define ALLOW_SYNC_CONNECTIONS() \
    ALLOW_SYNC_CONNECT(Input) \
    ALLOW_SYNC_CONNECT(Output) \
    ALLOW_SYNC_CONNECT(InOut) \
    ALLOW_SYNC_CONNECT(Register) \

////////////////////////////////////////////////////////////////////////
//
// Input
//
////////////////////////////////////////////////////////////////////////
template <typename T>
class Input : public Cascade::Port<T>
{
    DECLARE_PORT(Input, PORT_INPUT);
    ALLOW_COMB_CONNECTIONS();
    ALLOW_SYNC_CONNECTIONS();
};

////////////////////////////////////////////////////////////////////////
//
// Output
//
////////////////////////////////////////////////////////////////////////
template <typename T>
class Output : public Cascade::Port<T>
{ 
    DECLARE_PORT(Output, PORT_OUTPUT);
    ALLOW_COMB_CONNECTIONS();
    ALLOW_SYNC_CONNECTIONS();
};

////////////////////////////////////////////////////////////////////////
//
// InOut
//
////////////////////////////////////////////////////////////////////////
template <typename T>
class InOut : public Cascade::Port<T>
{ 
    DECLARE_PORT(InOut, PORT_INOUT);
    ALLOW_CONNECT(InOut);
};

////////////////////////////////////////////////////////////////////////
//
// Register
//
////////////////////////////////////////////////////////////////////////
template <typename T>
class Register : public Cascade::Port<T>
{ 
    DECLARE_PORT(Register, PORT_REGISTER);
    ALLOW_COMB_CONNECTIONS();
    ALLOW_SYNC_CONNECTIONS();
};

#endif

