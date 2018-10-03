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
// SimArchive.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/22/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Cascade.hpp"
#include <descore/Archive.hpp>
#include "Waves.hpp"

Archive *SimArchive::s_ar = NULL;
Component *SimArchive::s_component = NULL;

// Magic number at the end of a simulation archive
const uint32 ARCHIVE_CHECKVAL = 0xe37adb02;

// Vector of callbacks
static std::vector<SimArchive::Callback> s_callbacks;

////////////////////////////////////////////////////////////////////////
//
// archiveSimulation()
//
// Archive an entire simulation
//
////////////////////////////////////////////////////////////////////////
void SimArchive::archiveSimulation (Archive &ar)
{
    // Make sure the simulation has been initialized
    if (Sim::state != Sim::SimInitialized)
        Sim::init();
    Sim::state = Sim::SimArchiving;

    // Callbacks
    for (unsigned i = 0 ; i < s_callbacks.size() ; i++)
        (*s_callbacks[i])(ar);

    s_ar = &ar;

    // Archive the checksum
    uint32 check = Sim::s_checksum;
    ar | Sim::s_checksum;
    assert_always(check == Sim::s_checksum, "Load error: hardware checksum does not match checksum of archive");

    // Archive time
    ar | Sim::simTime | Sim::simTicks;
    if (ar.isLoading() && Cascade::params.CheckpointInterval)
        Sim::nextCheckpoint = Sim::simTime + Cascade::params.CheckpointInterval * 1000;

    // Archive the clock domains (also archives all ports)
    Cascade::ClockDomain::archiveClockDomains(ar);

    // Archive the components
    Sim::doComponents(archiveComponent);

    // Refresh the signals state if we're loading
    if (ar.isLoading())
        Cascade::Waves::archive();

    // Archive a fixed checkval
    uint32 checkval = ARCHIVE_CHECKVAL;
    ar | checkval;
    assert_always(checkval == ARCHIVE_CHECKVAL, "Load error: invalid checkval");

    Sim::state = Sim::SimInitialized;
}

////////////////////////////////////////////////////////////////////////
//
// loadSimulation()
//
////////////////////////////////////////////////////////////////////////
void SimArchive::loadSimulation (const char *filename)
{
    Archive ar(filename, Archive::Load);
    archiveSimulation(ar);
}

////////////////////////////////////////////////////////////////////////
//
// saveSimulation()
//
////////////////////////////////////////////////////////////////////////
void SimArchive::saveSimulation (const char *filename, bool safeMode)
{
    Archive ar(filename, safeMode ? Archive::SafeStore : Archive::Store);
    archiveSimulation(ar);
}

/////////////////////////////////////////////////////////////////
//
// registerCallback()
//
/////////////////////////////////////////////////////////////////
void SimArchive::registerCallback (Callback callback)
{
    for (unsigned i = 0 ; i < s_callbacks.size() ; i++)
        assert_always(s_callbacks[i] != callback, "SimArchive callback is already registered");
    s_callbacks.push_back(callback);
}

/////////////////////////////////////////////////////////////////
//
// unregisterCallback()
//
/////////////////////////////////////////////////////////////////
void SimArchive::unregisterCallback (Callback callback)
{
    unsigned i;
    for (i = 0 ; (i < s_callbacks.size()) && (s_callbacks[i] != callback) ; i++);
    assert_always(i != s_callbacks.size(), "SimArchive callback is not registered");
    for ( ; i < s_callbacks.size() - 1 ; i++)
        s_callbacks[i] = s_callbacks[i+1];
    s_callbacks.pop_back();
}

////////////////////////////////////////////////////////////////////////
//
// archiveComponent()
//
////////////////////////////////////////////////////////////////////////
void SimArchive::archiveComponent (Component *component)
{
    if (component->autoArchive())
    {
        s_ar->clearValidationErrorFlag();
        s_component = component;
        component->archive(*s_ar);
        s_ar->archiveCheckval(0x69);
        _bit active = component->m_componentActive;
        *s_ar | active;
        component->m_componentActive = active;
        s_component = NULL;
        if (s_ar->validationError())
            log("Archive validation error in %s\n", *component->getName());
    }
}

/////////////////////////////////////////////////////////////////
//
// Component *
//
/////////////////////////////////////////////////////////////////
Archive &operator| (Archive &ar, Component *&component)
{
    std::vector<int> location;
    if (ar.isLoading())
    {
        ar | location;
        if (location.size())
        {
            component = Sim::topLevelComponents;
            while (location.size())
            {
                int index = location.back();
                location.pop_back();
                for (int i = 0 ; i < index ; i++)
                {
                    assert(component);
                    component = component->nextComponent;
                }
                assert(component);
                if (location.size())
                    component = component->childComponent;
            }
        }
        else
            component = NULL;
    }
    else
    {
        Component *c = component;
        while (c)
        {
            Component *sibling = c->parentComponent ? c->parentComponent->childComponent : Sim::topLevelComponents;
            int index = 0;
            for ( ; sibling != c ; sibling = sibling->nextComponent, index++);
            location.push_back(index);
            c = c->parentComponent;
        }
        ar | location;
    }
    return ar;
}

////////////////////////////////////////////////////////////////////////////////
//
// Interface *
//
////////////////////////////////////////////////////////////////////////////////
Archive &operator| (Archive &ar, Interface *&_interface)
{
    const Component *c;
    int offset;
    if (ar.isLoading())
    {
        ar | c | offset;
        _interface = c ? (Interface *) (intptr_t(c) + offset) : NULL;
    }
    else
    {
        c = _interface ? _interface->getComponent() : NULL;
        offset = (intptr_t) _interface - (intptr_t) c;
        ar | c | offset;
    }
    return ar;
}

