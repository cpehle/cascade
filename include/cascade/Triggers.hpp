/*
Copyright 2008, D. E. Shaw Research.
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
// Triggers.hpp
//
// Copyright (C) 2008 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 09/19/2008
//
// A trigger is an atomic action that can be associated with a
// FIFO or a regular port.  It can be either an activation trigger,
// which causes a component to be activated, or an ITrigger pointer,
// which calls ITrigger::trigger().
//
//
//////////////////////////////////////////////////////////////////////

#ifndef Triggers_hpp_080919075617
#define Triggers_hpp_080919075617

/////////////////////////////////////////////////////////////////
//
// Trigger interface
//
/////////////////////////////////////////////////////////////////

template <typename T>
struct ITrigger
{
    virtual ~ITrigger () {}
    virtual void trigger (const T &data) = 0;
};

BEGIN_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// Generic trigger
//
/////////////////////////////////////////////////////////////////
typedef ITrigger<byte> GenericTrigger;

/////////////////////////////////////////////////////////////////
//
// Trigger
//
// Construction-time structure
//
/////////////////////////////////////////////////////////////////
struct Trigger
{
    Trigger () : target(0) 
    {
    }
    Trigger (intptr_t _target, bool _activeLow)
        : target(_target), activeLow(_activeLow)
    {
    }
    Trigger (const Trigger &rhs)
    {
        *this = rhs;
    }
    Trigger &operator= (const Trigger &rhs)
    {
        target = rhs.target;
        activeLow = rhs.activeLow;
        return *this;
    }

    // Pointer - bit 0 distinguishes Component pointers (0) from ITrigger pointers 
    intptr_t target;

    // For regular ports, a trigger can be active-high (default - action occurs
    // on non-zero value) or active-low (action occurs on zero value).  This
    // flag is unused for FIFO triggers.
    bool activeLow;
};

/////////////////////////////////////////////////////////////////
//
// Trigger macros
//
/////////////////////////////////////////////////////////////////

// Trigger ITrigger flag snuck into pointer bit 0 to overload an intptr_t.  
// If this bit is set, then the pointer is an ITrigger pointer and the action 
// is to call trigger().  If this bit is clear, then the pointer is a Component 
// pointer and the action is to call activate().
#define TRIGGER_ITRIGGER   1

// Perform the appropriate action given an intptr_t
#define TRIGGER_ACTIVATE_TARGET(target, pdata) \
{ \
    if (target & TRIGGER_ITRIGGER) \
        ((GenericTrigger*) ((target) & ~((intptr_t) TRIGGER_ITRIGGER)))->trigger(*(const byte *)pdata); \
    else \
        ((Component *) (intptr_t) (target))->m_componentActive = 1; \
}

END_NAMESPACE_CASCADE

#endif // #ifndef Triggers_hpp_080919075617

