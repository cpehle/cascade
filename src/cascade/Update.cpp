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
// Update.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/22/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Cascade.hpp"
#include "Update.hpp"
#include "Ports.hpp"
#include "ClockDomain.hpp"
#include "Component.hpp"
#include "Clock.hpp"
#include <descore/StringTable.hpp>

BEGIN_NAMESPACE_CASCADE

void setUpdateFunctionName (UpdateFunction f, const char *name);
const char *getUpdateFunctionName (UpdateFunction f);

/////////////////////////////////////////////////////////////////
//
// Globals
//
/////////////////////////////////////////////////////////////////

// Linked list of all update wrappers
static UpdateWrapper *g_updateWrappers = NULL;

// Stack of update state for components under construction
static stack<Component *> g_componentStack;
static stack<UpdateWrapper *> g_wrapperStack;

// LSB lookup table
static byte g_lsb[0x10000];

// Table of update names
static descore::StringTable g_updateNames;

//////////////////////////////////////////////////////////////////
//
// UpdateWrapper()
//
//////////////////////////////////////////////////////////////////
UpdateWrapper::UpdateWrapper (Component *_component, UpdateFunction _update, const char *_name) 
: update(_update),
component(_component),
name(_name),
clock(NULL),
next(NULL), 
prev(NULL),
strongRefCnt(0),
weakRefCnt(0)
{
    CascadeValidate(!_component || _update, "Update wrapper created with no update function");
    if (_component)
        Sim::stats.numUpdates++;
}

//////////////////////////////////////////////////////////////////
//
// getName()
//
//////////////////////////////////////////////////////////////////
strbuff UpdateWrapper::getName () const
{
    return getUpdateName(component, update);
}

////////////////////////////////////////////////////////////////////////////////
//
// getAssertionContext()
//
////////////////////////////////////////////////////////////////////////////////
string UpdateWrapper::getAssertionContext () const
{
    return *getName();
}

/////////////////////////////////////////////////////////////////
//
// addStrongEdge()
//
/////////////////////////////////////////////////////////////////
void UpdateWrapper::addStrongEdge (UpdateWrapper *edge, PortWrapper *port)
{
    // Check to see if this is a redundant strong edge
    for (int i = 0 ; i < strongEdges.size() ; i++)
    {
        if (edge == strongEdges[i])
            return;
    }
    strongEdges.push(edge);
    strongPort.push(port);
}

/////////////////////////////////////////////////////////////////
//
// addWeakEdge()
//
/////////////////////////////////////////////////////////////////
void UpdateWrapper::addWeakEdge (UpdateWrapper *edge, int weight)
{
    // Check to see if we already have this weak edge
    for (int i = 0 ; i < weakEdges.size() ; i++)
    {
        if (edge == weakEdges[i])
        {
            weakWeight[i] += weight;
            return;
        }
    }
    weakEdges.push(edge);
    weakWeight.push(weight);
}

/////////////////////////////////////////////////////////////////
//
// resolveClockDomain()
//
/////////////////////////////////////////////////////////////////
void UpdateWrapper::resolveClockDomain ()
{
    if (clock)
        clockDomain = clock->resolveClockDomain();
    else
        clockDomain = component->getClockDomain();
}

//////////////////////////////////////////////////////////////////
//
// UpdateConstructor()
//
//////////////////////////////////////////////////////////////////
UpdateConstructor::UpdateConstructor (Component *component, UpdateFunction update, const char *updateName)
{
    setUpdateFunctionName(update, updateName);
    updateName = g_updateNames.insert(updateName);

    assert_always(g_wrapperStack.size(),
        "Update functions can only be declared from component constructors");

    // See if the wrapper already exists
    for (m_wrapper = g_wrapperStack.back() ; m_wrapper ; m_wrapper = m_wrapper->next)
    {
        if (m_wrapper->name == updateName)
            break;
    }

    // If not then construct it
    if (!m_wrapper)
    {
        m_wrapper = new UpdateWrapper(component, update, updateName);
        m_wrapper->next = g_wrapperStack.back();
        g_wrapperStack.back() = m_wrapper;
    }
}

//////////////////////////////////////////////////////////////////
//
// read()
//
//////////////////////////////////////////////////////////////////
void UpdateConstructor::read (PortWrapper *port, bool array)
{
    if (port->isFifo())
    {
        assert_always(!port->producer, 
            "%s cannot read producer side of fifo %s", *m_wrapper->getName(), *port->getName());
        assert_always(!(port->connection & FIFO_NOREADER), 
            "%s cannot read fifo %s which has been sent to the bit bucket", *m_wrapper->getName(), *port->getName());
    }
    port->readers.push_back(m_wrapper);
    if (array && !port->arrayInternal)
    {
        for (port = port->next ; port && port->arrayInternal ; port = port->next)
            port->readers.push_back(m_wrapper);
    }
}

void UpdateConstructor::Rd (const PortSet &ports)
{
    PortIterator it(ports);
    for ( ; it ; it++)
        read(it.wrapper()); 
}

//////////////////////////////////////////////////////////////////
//
// write()
//
//////////////////////////////////////////////////////////////////
void UpdateConstructor::write (PortWrapper *port, bool array)
{
    assert_always(port->connection == PORT_UNCONNECTED || port->connection == PORT_WIRED,
        "%s cannot be a writer of connected port %s", *m_wrapper->getName(), *port->getName());
    assert_always(!port->isFifo() || !(port->connection & FIFO_NOWRITER),
        "%s cannot be a writer of fifo %s which has been wired to zero", *m_wrapper->getName(), *port->getName());
    port->writers.push_back(m_wrapper);
    if (array && !port->arrayInternal)
    {
        for (port = port->next ; port && port->arrayInternal ; port = port->next)
            port->writers.push_back(m_wrapper);
    }
}

void UpdateConstructor::Wr (const PortSet &ports)
{
    PortIterator it(ports);
    for ( ; it ; it++)
    {
        PortWrapper *w = it.wrapper();
        if (w->connection == PORT_UNCONNECTED || w->connection == PORT_WIRED)
            write(w);
    }
}

/////////////////////////////////////////////////////////////////
//
// clock()
//
/////////////////////////////////////////////////////////////////
UpdateConstructor &UpdateConstructor::clock (Clock &clk)
{
    assert_always(!m_wrapper->clock, "%s is already assigned to clock %s", *m_wrapper->getName(), *m_wrapper->clock->getName());
    m_wrapper->clock = &clk;
    return *this;
}

//////////////////////////////////////////////////////////////////
//
// beginComponent()
//
//////////////////////////////////////////////////////////////////
void UpdateFunctions::beginComponent (Component *c)
{
    g_componentStack.push(c);
    g_wrapperStack.push(NULL);
}

//////////////////////////////////////////////////////////////////
//
// endComponent()
//
//////////////////////////////////////////////////////////////////
void UpdateFunctions::endComponent ()
{
    // Check to see if the default update function was registered;
    // register it if it was not.
    CascadeValidate(g_componentStack.size(), "Component has been constructed but the component stack is empty");
    Component *component = g_componentStack.back();
    UpdateFunction defaultUpdate = component->getDefaultUpdate();
    if (defaultUpdate != &Component::update)
    {
        UpdateWrapper *wrapper = g_wrapperStack.back();
        bool foundDefault = false;
        for ( ; !foundDefault && wrapper ; wrapper = wrapper->next)
            foundDefault = (wrapper->update == defaultUpdate);

        if (!foundDefault)
        {
            UpdateConstructor update(component, defaultUpdate, "update");
            wrapper = g_wrapperStack.back();

            // Make the default update function a reader of any inputs/registers with no readers 
            // and a writer of any unconnected/wired outputs/inouts/registers with no writers.
            PortIterator itPort(PortSet::AllPorts, component->getInterfaceDescriptor(), component);
            for ( ; itPort ; itPort++)
            {
                PortWrapper *port = itPort.wrapper();
                if (!port->readers.size())
                {
                    if ((port->direction != PORT_OUTPUT) && (port->direction != PORT_INOUT) && (port->direction != PORT_OUTFIFO))
                    {
                        if ((port->direction != PORT_INFIFO) || (!(port->connection & FIFO_NOREADER) && !port->producer))
                            port->readers.push_back(wrapper);
                    }
                }
                if (!port->writers.size())
                {
                    if (port->isFifo())
                    {
                        if ((port->direction != PORT_INFIFO) && ((port->direction != PORT_OUTFIFO) || !(port->connection & FIFO_NOWRITER)))
                            port->writers.push_back(wrapper);
                    }
                    else if ((port->direction != PORT_INPUT) && (port->connection == PORT_UNCONNECTED || port->connection == PORT_WIRED))
                        port->writers.push_back(wrapper);
                }
            }
        }
    }

    // Add the wrappers to the globals linked list of wrappers
    UpdateWrapper *wrapper = g_wrapperStack.back();
    while (wrapper)
    {
        UpdateWrapper *w = wrapper;
        wrapper = wrapper->next;
        w->next = g_updateWrappers;
        g_updateWrappers = w;
    }
    g_wrapperStack.pop();
    g_componentStack.pop();
}

//////////////////////////////////////////////////////////////////
//
// cleanup()
//
//////////////////////////////////////////////////////////////////
void UpdateFunctions::cleanup ()
{
    for (int i = 0 ; i < g_wrapperStack.size() ; i++)
        destroyList(g_wrapperStack[i]);
    g_wrapperStack.clear();
    g_componentStack.clear();
    destroyList(g_updateWrappers);
    g_updateWrappers = NULL;
}

/////////////////////////////////////////////////////////////////
//
// resolveClockDomains()
//
/////////////////////////////////////////////////////////////////
void UpdateFunctions::resolveClockDomains ()
{
    for (UpdateWrapper *wrapper = g_updateWrappers ; wrapper ; wrapper = wrapper->next)
        wrapper->resolveClockDomain();
}

//////////////////////////////////////////////////////////////////
//
// sort()
//
//////////////////////////////////////////////////////////////////
void UpdateFunctions::sort ()
{
    int i, j;

    // Make sure the update hierarchy got cleaned up
    CascadeValidate(!g_wrapperStack.size() && !g_componentStack.size(), 
        "Update hierarchy was not properly constructed");

    // Separate the update functions into clock domains
    while (g_updateWrappers)
    {
        UpdateWrapper *wrapper = g_updateWrappers;
        g_updateWrappers = wrapper->next;
        wrapper->clockDomain->registerUpdateFunction(wrapper);
    }

    // Initialize the LSB lookup table
    g_lsb[0] = 0;
    for (i = 1 ; i < 0x10000 ; i++)
    {
        for (j = 0 ; !(i & (1 << j)) ; j++);
        g_lsb[i] = j;
    }

    // Have the clock domains sort their update functions
    logInfo("Sorting update functions...\n");
    ClockDomain::doAcross(&ClockDomain::sortUpdateFunctions);
}

/////////////////////////////////////////////////////////////////
//
// sort()
//
/////////////////////////////////////////////////////////////////

// Helper functions to identify combinational cycles
static bool findCycle (UpdateWrapper *w);
static bool findCycle (UpdateWrapper *w, UpdateWrapper *endpoint);

#define WEAK_INDEX(cnt) ((cnt) > 255 ? 255 : cnt)
#define WEAK_INSERT(w) \
    int index = WEAK_INDEX(w->weakRefCnt); \
    w->next = weakList[index]; \
    if (w->next) \
        w->next->prev = w; \
    else \
    { \
        int i0 = index >> 4; \
        mask1[i0] |= 1 << (index & 15); \
        mask0 |= 1 << i0; \
    } \
    weakList[index] = w; \
    w->prev = NULL;

// Sort function
UpdateWrapper *UpdateFunctions::sort (UpdateWrapper *wrappers)
{
    UpdateWrapper *w;
    int i;

    // Compute reference counts and prev
    UpdateWrapper *prev = NULL;
    for (w = wrappers ; w ; w = w->next)
    {
        w->prev = prev;
        prev = w;
        for (i = 0 ; i < w->strongEdges.size() ; i++)
            w->strongEdges[i]->strongRefCnt++;
        for (i = 0 ; i < w->weakEdges.size() ; i++)
            w->weakEdges[i]->weakRefCnt += w->weakWeight[i];
    }

    // Sort the update functions into 256 ready lists by weakRefCnt
    UpdateWrapper *weakList[256];
    memset(weakList, 0, sizeof(weakList));

    // Two-level bitmask indicating which weak lists have update functions.
    // Bit k0 of mask0 indicates that mask1[k0] is non-zero.  Bit k1 of
    // mask1[k0] indicates that the list 16*k0 + k1 is non-empty.
    uint16 mask0 = 0;
    uint16 mask1[16];
    memset(mask1, 0, sizeof(mask1));

    // Initialize the sorted set and new list
    UpdateWrapper *first = NULL;
    UpdateWrapper **last = &first;
    for (w = wrappers ; w ; )
    {
        UpdateWrapper *wnext = w->next;
        if (!w->strongRefCnt)
        {
            w->remove(wrappers);
            WEAK_INSERT(w);
        }
        w = wnext;
    }

    // Sort the update functions until the ready lists are empty
    while (mask0)
    {
        int i0 = g_lsb[mask0];
        int index = (i0 << 4) + g_lsb[mask1[i0]];

        // Remove the wrapper from the ready list
        w = weakList[index];
        w->remove(weakList[index]);
        if (!weakList[index])
        {
            mask1[i0] -= 1 << (index & 15);
            if (!mask1[i0])
                mask0 -= 1 << i0;
        }

        // Add the wrapper to the sorted list
        *last = w;
        last = &(w->next);

        // Set the strong reference count to -1 so that we don't try to re-sort
        // the wrapper if its weak reference count is decremented
        w->strongRefCnt = -1;

        // Update reference counts, resorting if necessary
        for (i = 0 ; i < w->strongEdges.size() ; i++)
        {
            UpdateWrapper *wrapper = w->strongEdges[i];
            if (!--wrapper->strongRefCnt)
            {
                wrapper->remove(wrappers);
                WEAK_INSERT(wrapper);
            }
        }
        for (i = 0 ; i < w->weakEdges.size() ; i++)
        {
            UpdateWrapper *wrapper = w->weakEdges[i];
            if (!wrapper->strongRefCnt)
            {
                int index = WEAK_INDEX(wrapper->weakRefCnt);
                wrapper->remove(weakList[index]);
                if (!weakList[index]) 
                {
                    int i0 = index >> 4;
                    mask1[i0] -= 1 << (index & 15);
                    if (!mask1[i0])
                        mask0 -= 1 << i0;
                }
            }
            wrapper->weakRefCnt -= w->weakWeight[i];
            if (!wrapper->strongRefCnt)
            {
                WEAK_INSERT(wrapper);
            }
        }
    }

    // At this point if there are any wrappers left in the list then there
    // is a combinational cycle.
    if (wrappers)
    {
        findCycle(wrappers);
        die("No update order is possible: aborting");
    }

    *last = NULL;
    return first;
}

// Helper functions to identify combinational cycles
static int s_mark;

static bool findCycle (UpdateWrapper *w)
{
    s_mark = -1;

    for ( ; w ; w = w->next)
    {
        if (findCycle(w, w))
            return true;
        s_mark--;
    }

    return false;
}

static bool findCycle (UpdateWrapper *w, UpdateWrapper *endpoint)
{
    if (w->weakRefCnt == s_mark)
    {
        if (w == endpoint)
        {
            logerr("Combinational cycle detected:\n");
            logerr("    %s\n", *w->getName());
            return true;
        }
        return false;
    }

    w->weakRefCnt = s_mark;

    for (int i = 0 ; i < w->strongEdges.size() ; i++)
    {
        if (findCycle(w->strongEdges[i], endpoint))
        {
            logerr("        << %s\n", *w->strongPort[i]->getName());
            logerr("        << %s\n", *w->getName());
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////
//
// Support for update names
//
//////////////////////////////////////////////////////////////////

struct UpdateFunctionName
{
    UpdateFunction f;
    const char *name;
};

static stack<UpdateFunctionName> updateNames;

void setUpdateFunctionName (UpdateFunction f, const char *name)
{
    if (!strcmp(name, "update"))
        return;
    for (int i = 0 ; i < updateNames.size() ; i++)
    {
        if (updateNames[i].f == f)
            return;
    }
    updateNames.push(UpdateFunctionName());
    updateNames.back().f = f;
    updateNames.back().name = name;
}

const char *getUpdateFunctionName (UpdateFunction f)
{
    for (int i = 0 ; i < updateNames.size() ; i++)
    {
        if (updateNames[i].f == f)
            return updateNames[i].name;
    }
    return "update";
}

strbuff UpdateWrapper::getUpdateName (Component *component, UpdateFunction update)
{
    if (!component)
        return "<sentinel>";
    strbuff s;
    component->formatName(s, false);
    s.append("::%s()", getUpdateFunctionName(update));
    return s;
}

strbuff getUpdateName (const S_Update *update)
{
    return UpdateWrapper::getUpdateName(update->component, update->fn);
}

END_NAMESPACE_CASCADE

