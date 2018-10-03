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
// Component.cpp
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
#include "Clock.hpp"

using namespace Cascade;

////////////////////////////////////////////////////////////////////////
//
// Component()
//
////////////////////////////////////////////////////////////////////////
Component::Component () : 
parentComponent(NULL), 
childComponent(NULL),
nextComponent(NULL), 
m_componentTraces(0),
m_componentActive(0),
m_componentId(COMPONENT_NULL_ID)
{
    registerEvent(&Component::activate);

    Sim::stats.numComponents++;

    parentComponent = Hierarchy::setComponent(this);
    if (Hierarchy::currFrame->type == Hierarchy::COMPONENT)
    {
        // Add this component to the linked list of its parent's children,
        // or to the top level linked list if it has no parent.
        Component **ppChild = &Sim::topLevelComponents;

        if (parentComponent)
            ppChild = &parentComponent->childComponent;
        for ( ; *ppChild ; ppChild = &((*ppChild)->nextComponent));
        *ppChild = this;

        // All components by default start in the active state
        m_componentActive = 1;
    }

    // Add this component to the update hierarchy if it's not an array
    if (!Hierarchy::currFrame->array)
        Cascade::UpdateFunctions::beginComponent(this);
}

////////////////////////////////////////////////////////////////////////
//
// ~Component()
//
// Notify the infrastructure that we're cleaning up
//
////////////////////////////////////////////////////////////////////////
Component::~Component ()
{
    // The Array destructor sets parentComponent to this to indicate that the 
    // component is not in the hierarchy.  Otherwise, clean up.
    if (parentComponent != this)
    {
        // Make sure we're not leaking components
        for (Component *c = childComponent ; c ; c = c->nextComponent)
            warn(false, "Memory leak detected: failed to delete component %s", *c->getName());

        // Remove the component from the hierarchy
        Component **ppcomponent = parentComponent ? &parentComponent->childComponent : &Sim::topLevelComponents;
        for ( ; *ppcomponent != this ; ppcomponent = &((*ppcomponent)->nextComponent))
            CascadeValidate(*ppcomponent, "Could not locate component being deleted within hierarchy");
        *ppcomponent = nextComponent;

        // Clean up if this was the last component
        if (!Sim::topLevelComponents)
            Sim::cleanupInternal();
    }
}

////////////////////////////////////////////////////////////////////////
//
// getClockPeriod()
//
// Get the period of this component's clock in picoseconds
//
////////////////////////////////////////////////////////////////////////
int Component::getClockPeriod ()
{
    return getClockDomain()->getPeriod();
}

////////////////////////////////////////////////////////////////////////
//
// formatLocalName()
//
////////////////////////////////////////////////////////////////////////
bool Component::formatLocalName (strbuff &s) const
{
    const char *name = getComponentName();
    if (name)
    {
        if (!parentComponent || !parentComponent->suppressChildName())
            s.puts(name);
        if (m_componentId != COMPONENT_NULL_ID)
        {
            if (parentComponent)
                parentComponent->formatChildId(s, m_componentId);
            else
                s.append("%d", m_componentId);
        }
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////
//
// formatName()
//
////////////////////////////////////////////////////////////////////////
void Component::formatName (strbuff &s, bool separator) const
{
    if (parentComponent)
        parentComponent->formatName(s);
    if (formatLocalName(s) && separator && !suppressDot())
        s.putch('.');
}

////////////////////////////////////////////////////////////////////////
//
// formatChildId()
//
// Format the child id.  Default is simply %d.
//
////////////////////////////////////////////////////////////////////////
void Component::formatChildId (strbuff &s, int id) const
{
    s.append("%d", id);
}

////////////////////////////////////////////////////////////////////////
//
// getName()
//
////////////////////////////////////////////////////////////////////////
strbuff Component::getName () const
{
    strbuff s;
    formatName(s, false);
    return s;
}

////////////////////////////////////////////////////////////////////////////////
//
// getLocalName()
//
////////////////////////////////////////////////////////////////////////////////
strbuff Component::getLocalName () const
{
    strbuff s;
    formatLocalName(s);
    return s;
}

////////////////////////////////////////////////////////////////////////////////
//
// setTrace()
//
////////////////////////////////////////////////////////////////////////////////
void Component::setTrace (const char *key /* = "" */ )
{
    descore::setTrace(*getName(), key);
}

////////////////////////////////////////////////////////////////////////////////
//
// unsetTrace()
//
////////////////////////////////////////////////////////////////////////////////
void Component::unsetTrace (const char *key /* = "" */ )
{
    descore::unsetTrace(*getName(), key);
}

////////////////////////////////////////////////////////////////////////
//
// archive()
//
////////////////////////////////////////////////////////////////////////
void Component::archive (Archive &)
{
    die("archive() has not been implemented");
}

/////////////////////////////////////////////////////////////////
//
// getClockDomain()
//
/////////////////////////////////////////////////////////////////
Cascade::ClockDomain *Component::getClockDomain (bool required) const
{
    ClockIterator it(this);
    if (!it)
    {
        if (parentComponent)
            return parentComponent->getClockDomain(required);
        else
            return ClockDomain::getDefaultClockDomain();
    }
    Clock *clock = *it;
    it++;
    if (!it)
        return clock->resolveClockDomain();
    for ( ; it && !clock->isDefault() ; it++)
        clock = *it;
    if (clock->isDefault())
    {
        for ( ; it ; it++)
        {
            Clock *clock2 = *it;
            assert_always(!clock2->isDefault(),
                "%s has two default clocks:\n    %s and %s",
                *getName(), *clock->getName(), *clock2->getName());
        }
        return clock->resolveClockDomain();
    }

    assert(!required, "Unable to resolve clock domain");
    return NULL;
}

/////////////////////////////////////////////////////////////////
//
// scheduleEventInternal()
//
/////////////////////////////////////////////////////////////////
void Component::scheduleEventInternal (int delay, Cascade::IEvent *event)
{
    assert_always(delay > 0, "Attempted to schedule event with non-positive delay %d", delay);
    ClockDomain *domain = t_currentClockDomain ? t_currentClockDomain : getClockDomain();
    domain->scheduleEvent(delay, event);
}

/////////////////////////////////////////////////////////////////
//
// getTickCount()
//
/////////////////////////////////////////////////////////////////
int Component::getTickCount () const
{
    ClockDomain *domain = t_currentClockDomain ? t_currentClockDomain : getClockDomain();
    return domain->getTickCount();
}
