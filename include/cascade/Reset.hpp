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
// Reset.hpp
//
// Copyright (C) 2008 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 12/22/2008
//
// Reset pseudo-ports are used to bind interfaces when instantiating a
// C++ component within a Verilog simulation.  The reset port name(s)
// must exactly match the port names of the Verilog interface.  When
// a Verilog reset signal is asserted, reset() is called on the C++
// side instead of the usual update() functions on rising clock edges.
//
//////////////////////////////////////////////////////////////////////

#ifndef Reset_hpp_081222103953
#define Reset_hpp_081222103953

/////////////////////////////////////////////////////////////////
//
// ResetPort
//
/////////////////////////////////////////////////////////////////
BEGIN_NAMESPACE_CASCADE

class ResetPort
{
    DECLARE_NOCOPY(ResetPort);
public:
    ResetPort () : m_val(0)
    {
        Hierarchy::addPort(PORT_RESET, this, getPortInfo<bit>());
    }
    static void preConstruct (ResetPort *port, int numPorts, byte level)
    {
        if (!numPorts)
            port->m_level = level;
        port -= numPorts;
        for (int i = 0 ; i < numPorts ; i++)
            port[i].m_level = level;
    }

    // Accessors
    inline _bit operator= (bit val)
    {
        return m_val = val;
    }
    inline operator _bit () const
    {
        return m_val;
    }
    inline byte resetLevel () const
    {
        return m_level;
    }
    inline _bit *operator& ()
    {
        return &m_val;
    };

    // Return the name of this port
    strbuff getName () const
    {
        return PortName::getPortName(this);
    }
    string getAssertionContext () const
    {
        return *getName();
    }

private:
    _bit m_val;
    byte m_level;
};

END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// Declaration macro (implemented in Interface.hpp)
//
/////////////////////////////////////////////////////////////////

#define Reset(level, name) DECLARE_RESET_PORT(level, name)

#endif // #ifndef Reset_hpp_081222103953

