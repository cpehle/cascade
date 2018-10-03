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
// Update.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_Update_hpp_
#define Cascade_Update_hpp_

#include "SimDefs.hpp"
#include "Stack.hpp"
#include "Wrapper.hpp"

class Component;
class Clock;

BEGIN_NAMESPACE_CASCADE

typedef void (Component::*UpdateFunction) ();

template <typename T> class Port;
template <typename T> class FifoPort;
template <typename T> class PortArray;
class PortWrapper;
struct PortSet;
class UpdateWrapper;
class ClockDomain;

//////////////////////////////////////////////////////////////////
//
// UpdateWrapper
//
//////////////////////////////////////////////////////////////////
class UpdateWrapper : public Wrapper
{
    DECLARE_NOCOPY(UpdateWrapper);
public:
    UpdateWrapper (Component *_component = NULL, UpdateFunction _update = NULL, const char *_name = NULL);

    // Returns Component::Update
    strbuff getName () const;
    static strbuff getUpdateName (Component *component, UpdateFunction update);
    string getAssertionContext () const;

    // Graph construction helper functions
    void addStrongEdge (UpdateWrapper *edge, PortWrapper *port);
    void addWeakEdge (UpdateWrapper *edge, int weight);

    // List helper function
    inline void remove (UpdateWrapper *&list)
    {
        if (prev)
            prev->next = next;
        else
            list = next;
        if (next)
            next->prev = prev;
    }

    // Resolve the clock domain
    void resolveClockDomain ();

public:
    // Update function
    UpdateFunction update;
    Component *component;
    const char *name;

    union
    {
        Clock *clock;                // Used during construction phase
        ClockDomain *clockDomain;    // Used during initialization phase
    };
    
    // Linked list of update functions
    UpdateWrapper *next;
    UpdateWrapper *prev;
    
    // Reference graph
    stack<UpdateWrapper *, WrapperAlloc<UpdateWrapper *> > strongEdges;
    stack<UpdateWrapper *, WrapperAlloc<UpdateWrapper *> > weakEdges;
    stack<int, WrapperAlloc<int> >                         weakWeight;
    stack<PortWrapper *, WrapperAlloc<PortWrapper *> >     strongPort;  // port we write that someone else reads

    int strongRefCnt;
    int weakRefCnt;

    // 32 bits of storage used for different things at different times
    union
    {
        int index;  // After global sort, indicates update order
        int offset; // Indicates actual offset into update array
    };

    // Triggers
    stack<PortWrapper *, WrapperAlloc<PortWrapper *> > triggers;
};

//////////////////////////////////////////////////////////////////
//
// Helper class to create an UpdateWrapper and read/write
// access lists.
//
//////////////////////////////////////////////////////////////////
class UpdateConstructor
{
    DECLARE_NOCOPY(UpdateConstructor);
public:
    UpdateConstructor (Component *component, UpdateFunction update, const char *updateName);

    template <typename P1> 
    UpdateConstructor &reads (const P1 &p1) { Rd(p1); return *this; }
    template <typename P1, typename P2> 
    UpdateConstructor &reads (const P1 &p1, const P2 &p2) { Rd(p1); Rd(p2); return *this; }
    template <typename P1, typename P2, typename P3> 
    UpdateConstructor &reads (const P1 &p1, const P2 &p2, const P3 &p3) { Rd(p1); Rd(p2); Rd(p3); return *this; }
    template <typename P1, typename P2, typename P3, typename P4> 
    UpdateConstructor &reads (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4) { Rd(p1); Rd(p2); Rd(p3); Rd(p4); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5> 
    UpdateConstructor &reads (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5) { Rd(p1); Rd(p2); Rd(p3); Rd(p4); Rd(p5); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> 
    UpdateConstructor &reads (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5, const P6 &p6) { Rd(p1); Rd(p2); Rd(p3); Rd(p4); Rd(p5); Rd(p6); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> 
    UpdateConstructor &reads (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5, const P6 &p6, const P7 &p7) { Rd(p1); Rd(p2); Rd(p3); Rd(p4); Rd(p5); Rd(p6); Rd(p7); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> 
    UpdateConstructor &reads (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5, const P6 &p6, const P7 &p7, const P8 &p8) { Rd(p1); Rd(p2); Rd(p3); Rd(p4); Rd(p5); Rd(p6); Rd(p7); Rd(p8); return *this; }

    template <typename P1> 
    UpdateConstructor &writes (const P1 &p1) { Wr(p1); return *this; }
    template <typename P1, typename P2> 
    UpdateConstructor &writes (const P1 &p1, const P2 &p2) { Wr(p1); Wr(p2); return *this; }
    template <typename P1, typename P2, typename P3> 
    UpdateConstructor &writes (const P1 &p1, const P2 &p2, const P3 &p3) { Wr(p1); Wr(p2); Wr(p3); return *this; }
    template <typename P1, typename P2, typename P3, typename P4> 
    UpdateConstructor &writes (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4) { Wr(p1); Wr(p2); Wr(p3); Wr(p4); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5> 
    UpdateConstructor &writes (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5) { Wr(p1); Wr(p2); Wr(p3); Wr(p4); Wr(p5); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6> 
    UpdateConstructor &writes (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5, const P6 &p6) { Wr(p1); Wr(p2); Wr(p3); Wr(p4); Wr(p5); Wr(p6); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7> 
    UpdateConstructor &writes (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5, const P6 &p6, const P7 &p7) { Wr(p1); Wr(p2); Wr(p3); Wr(p4); Wr(p5); Wr(p6); Wr(p7); return *this; }
    template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8> 
    UpdateConstructor &writes (const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5, const P6 &p6, const P7 &p7, const P8 &p8) { Wr(p1); Wr(p2); Wr(p3); Wr(p4); Wr(p5); Wr(p6); Wr(p7); Wr(p8); return *this; }

    UpdateConstructor &clock (Clock &clk);

private:

    template <typename T> void Rd (const Port<T> &port) 
    { 
        read(port.wrapper); 
    }
    template <typename T> void Rd (const FifoPort<T> &port) 
    { 
        read(port.wrapper); 
    }
    template <typename T> void Rd (const T *port) 
    { 
        read(port->wrapper, true); 
    }
    template <typename T> void Rd (const PortArray<T> &ports)
    {
        if (ports.m_size)
            read(ports.m_array->wrapper, true);
    }
    void read (PortWrapper *port, bool array = false);
    void Rd (const PortSet &ports);

    template <typename T> void Wr (const Port<T> &port) 
    { 
        write(port.wrapper); 
    }
    template <typename T> void Wr (const FifoPort<T> &port) 
    { 
        write(port.wrapper); 
    }
    template <typename T> void Wr (const T *port)
    { 
        write(port->wrapper, true); 
    }
    template <typename T> void Wr (const PortArray<T> &ports)
    {
        if (ports.m_size)
            write(ports.m_array->wrapper, true);
    }
    void write (PortWrapper *port, bool array = false);
    void Wr (const PortSet &ports);

private:
    UpdateWrapper *m_wrapper;
};

//////////////////////////////////////////////////////////////////
//
// Update function declaration macro
//
//////////////////////////////////////////////////////////////////
#define UPDATE(fn) \
    Cascade::UpdateConstructor(this, (Cascade::UpdateFunction) &_thistype::fn, #fn)

//////////////////////////////////////////////////////////////////
//
// Static state and callback functions
//
//////////////////////////////////////////////////////////////////
struct UpdateFunctions
{
    // Called when a new component is being constructed
    static void beginComponent (Component *c);

    // Called when a component is fully constructed
    static void endComponent ();

    // Destroy/clean up all state
    static void cleanup ();

    // Resolve update wrapper clock domains
    static void resolveClockDomains ();

    // Sort the update wrappers (called during initialization)
    static void sort ();

    // Sort the update wrappers in an individual clock domain 
    // (returns a new linked list in sorted order).
    static UpdateWrapper *sort (UpdateWrapper *wrappers);
};

//////////////////////////////////////////////////////////////////
//
// Update array structures
//
//////////////////////////////////////////////////////////////////
struct S_Update
{
    UpdateFunction fn;
    Component      *component;   // NULL for a placeholder
    int32          numTriggers;  // Number of associated triggers
};

struct S_Trigger
{
    intptr_t target;        // Trigger target (ITrigger, or component to activate)
    byte     *value;        // Pointer to value to test
    uint16   size : 13;     // Size of value in bytes
    uint16   activeLow : 1; // Trigger when value == 0
    uint16   latch     : 1; // Trigger is associated with a Latch or Wired port
    uint16   active    : 1; // Latch trigger was active on previous cycle and is in m_stickyTriggers
    bool     fast;          // size == 1, activeLow == 0, latch == 0
    uint8    delay;         // Synchronous delay of trigger in clock cycles
};

// Component::update
strbuff getUpdateName (const S_Update *update);

END_NAMESPACE_CASCADE
 
#endif

