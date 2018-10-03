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
// Clock.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 08/07/2007
//
// Special clock "port" used to define clocks and clock domains.
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_Clock_hpp_
#define Cascade_Clock_hpp_

#include "SimGlobals.hpp"
#include "Hierarchy.hpp"
#include "PortTypes.hpp"
#include "BitVector.hpp"

BEGIN_NAMESPACE_CASCADE
class ClockDomain;
END_NAMESPACE_CASCADE

class Clock
{
    DECLARE_NOCOPY(Clock);
    friend class Sim;
    friend class Component;
    friend class Cascade::ClockDomain;
public:

    // Add the clock to the interface descriptor
    Clock ()
    {
        // This is a hack to get around the fact that after the Clock() macro 
        // is defined (at the bottom of this file), we can no longer implement
        // the constructor because it will be interpreted as a macro.
        construct();
    }

    // Support for PortArray<Clock>
    static bool addPortArrayEntry (const void *arrayAddress)
    {
        return Cascade::Hierarchy::addPortArray(Cascade::PORT_CLOCK, arrayAddress, Cascade::getPortInfo<bit>(), sizeof(Clock));
    }

    /////////////////////////////////////////////////////////////////
    //
    // All clocks must be connected, generating, or dividing.
    //
    /////////////////////////////////////////////////////////////////

    // Connect two clock ports
    Clock &operator<< (Clock &rhs);

    // Make this clock the driver of the clock domain by supplying an
    // explicit clock period in ps as well as the offset from time zero
    // of the first rising edge, also in ps.
    void generateClock (int period = Cascade::params.DefaultClockPeriod, int offset = 0);

    // Make this clock the driver of the clock domain by supplying a
    // period ratio to another clock, as well as an offset in ps from that
    // clock's first rising edge.  Ratios less than 1 imply a faster clock.
    void divideClock (Clock &rhs, float ratio, int offset = 0);

    // Make this clock the driver of the clock domain by supplying an offset
    // to another clock.  Same as calling divideClock with a ratio of 1.0.
    void offsetClock (Clock &rhs, int offset);

    // Make this clock the driver of the clock domain via manual calls
    // to tick().
    void setManual ();

    // Permanently disable this clock domain.
    void disable ();

    // Simulate a rising clock edge (this can only be called if this clock
    // is the driver of its domain with period set to 0).
    void tick ();

    // Make this clock the default clock for the containing component.
    inline void setAsDefault ()
    {
        m_ptr |= 1;
    }

    // Return the port name of this clock
    strbuff getName () const;

    // Return the clock domain of this clock (resolving it if necessary)
    Cascade::ClockDomain *resolveClockDomain (bool required = true);

private:
    //----------------------------------
    // Pointer accessors
    //----------------------------------
    inline Clock *connection () const
    {
        return (Clock *) (m_ptr & ~((intptr_t) 3));
    }
    inline Cascade::ClockDomain *clockDomain () const
    {
        return (Cascade::ClockDomain *) (m_ptr & ~((intptr_t) 3));
    }
    inline void setConnection (Clock *clock)
    {
        m_ptr = (m_ptr & 1) | 2 | (intptr_t) clock;
    }
    inline void setClockDomain (Cascade::ClockDomain *domain)
    {
        m_ptr = (m_ptr & 1) | (intptr_t) domain;
    }

    //----------------------------------
    // Flags accessors
    //----------------------------------
    inline bool isDefault () const
    {
        return (m_ptr & 1) != 0;
    }
    inline bool connected () const
    {
        return (m_ptr & 2) != 0;
    }
    inline bool driving () const
    {
        return !connected() && clockDomain();
    }

    //----------------------------------
    // Other helpers
    //----------------------------------
    void construct ();
    static void cleanup ();
    string getAssertionContext () const;

private:

    // Multipurpose pointer.  The bottom two bits are flags.  Bit 0 indicates
    // that this is a default clock.  Bit 1 indicates the pointer type:
    // 1 if this points to another clock; 0 if this points to a clock domain.
    intptr_t m_ptr;
};

/////////////////////////////////////////////////////////////////
//
// Declaration macro (implemented in Interface.hpp)
//
/////////////////////////////////////////////////////////////////

#define Clock(name) DECLARE_NAMED_PORT(Clock, name)


#endif

