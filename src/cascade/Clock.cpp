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
// Clock.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 08/07/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Cascade.hpp"

static std::vector<Clock *> globalClocks;

using namespace Cascade;

////////////////////////////////////////////////////////////////////////////////
//
// disabledClockDomain()
//
// Implement disabled clock domains by creating a manual clock domain that
// never get ticked.
//
////////////////////////////////////////////////////////////////////////////////

static ClockDomain *disabledClockDomain = NULL;

static ClockDomain *getDisabledClockDomain ()
{
    if (!disabledClockDomain)
        disabledClockDomain = new ClockDomain;
    return disabledClockDomain;
}

/////////////////////////////////////////////////////////////////
//
// Clock()
//
/////////////////////////////////////////////////////////////////
void Clock::construct ()
{
    if (Sim::state == Sim::SimNone)
        Sim::state = Sim::SimConstruct;
    m_ptr = 0;
    if (Hierarchy::currFrame)
        Hierarchy::addPort(PORT_CLOCK, this, getPortInfo<bit>());
    else
        globalClocks.push_back(this);
}

/////////////////////////////////////////////////////////////////
//
// operator<<
//
/////////////////////////////////////////////////////////////////
Clock &Clock::operator<< (Clock &rhs)
{
    assert_always(Sim::state == Sim::SimConstruct);
    assert_always(!connected(), "Clock is already connected");
    assert_always(!driving(), "Clock is already driven");

    Clock *target = &rhs;
    for ( ; target->connected() ; target = target->connection());
    assert_always(target != this, "Cannot connect clock to itself");
    setConnection(target);
    return *this;
}

/////////////////////////////////////////////////////////////////
//
// generateClock()
//
/////////////////////////////////////////////////////////////////
void Clock::generateClock (int period, int offset)
{
    assert_always(Sim::state == Sim::SimConstruct);
    assert_always(!(m_ptr & ~((intptr_t) 1)), "Clock source has already been declared");
    setClockDomain(new ClockDomain(period, offset));
}

/////////////////////////////////////////////////////////////////
//
// divideClock()
//
/////////////////////////////////////////////////////////////////
void Clock::divideClock (Clock &rhs, float ratio, int offset)
{
    assert_always(Sim::state == Sim::SimConstruct);
    assert_always(!(m_ptr & ~((intptr_t) 1)), "Clock source has already been declared");
    setClockDomain(new ClockDomain(&rhs, ratio, offset));
}

////////////////////////////////////////////////////////////////////////////////
//
// offsetClock()
//
////////////////////////////////////////////////////////////////////////////////
void Clock::offsetClock (Clock &rhs, int offset)
{
    divideClock(rhs, 1.0f, offset);
}

/////////////////////////////////////////////////////////////////
//
// setManual()
//
/////////////////////////////////////////////////////////////////
void Clock::setManual ()
{
    assert_always(Sim::state == Sim::SimConstruct);
    assert_always(!(m_ptr & ~((intptr_t) 1)), "Clock source has already been declared");
    setClockDomain(new ClockDomain());
}

////////////////////////////////////////////////////////////////////////////////
//
// disable()
//
////////////////////////////////////////////////////////////////////////////////
void Clock::disable ()
{
    assert_always(Sim::state == Sim::SimConstruct);
    assert_always(!(m_ptr & ~((intptr_t) 1)), "Clock source has already been declared");
    setClockDomain(getDisabledClockDomain());
}

/////////////////////////////////////////////////////////////////
//
// tick()
//
/////////////////////////////////////////////////////////////////
void Clock::tick ()
{
    // Automatically initialize the simulation if it has not been initialized
    if (Sim::state != Sim::SimInitialized)
        Sim::init();

    assert_always(!t_currentUpdate, "Clock cannot be manually ticked from within an update function");
    ClockDomain *domain = clockDomain();
    CascadeValidate(domain, "Clock has no clock domain");
    assert_always(domain != disabledClockDomain,
        "Clock is disabled and cannot be manually ticked");
    assert_always(!connected() && !domain->getPeriod(), 
        "Clock is automatically generated and cannot be manually ticked");
    domain->manualTick();
}

/////////////////////////////////////////////////////////////////
//
// getName()
//
/////////////////////////////////////////////////////////////////
strbuff Clock::getName () const
{
    for (unsigned i = 0 ; i < globalClocks.size () ; i++)
    {
        if (this == globalClocks[i])
            return strbuff("GlobalClock%d", i);
    }
    return PortName::getPortName(this);
}

/////////////////////////////////////////////////////////////////
//
// resolveClockDomain()
//
/////////////////////////////////////////////////////////////////
ClockDomain *Clock::resolveClockDomain (bool required)
{
    if (connected())
        setClockDomain(connection()->resolveClockDomain());
    ClockDomain *ret = clockDomain();
    assert_always(ret || !required, "Clock net has no clock driver");
    return ret;
}

/////////////////////////////////////////////////////////////////
//
// cleanup()
//
/////////////////////////////////////////////////////////////////
void Clock::cleanup ()
{
    globalClocks.clear();
    disabledClockDomain = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// getAssertionContext()
//
////////////////////////////////////////////////////////////////////////////////
string Clock::getAssertionContext () const
{
    return *getName();
}
