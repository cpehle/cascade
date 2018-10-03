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
// Verilog.cpp
//
// Copyright (C) 2008 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 12/23/2008
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#ifdef _VERILOG

#include <descore/AssertParams.hpp>
#include "Cascade.hpp"
#include <vcsuser.h>
#include <list>
#include <descore/Iterators.hpp>

extern "C" void vcs_atexit (void (*pfun) (int code));

BEGIN_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// validatePortName helper functions
//
// Make sure the Cascade and Verilog port names are compatible when
// binding a port.
//
/////////////////////////////////////////////////////////////////

static int stripType (const char *&name)
{
    if (*name == 'i')
    {
        if (name[1] == '_')
        {
            name += 2;
            return PORT_INPUT;
        }
        if (name[1] == 'n' && name[2] == '_')
        {
            name += 3;
            return PORT_INPUT;
        }
        if (name[1] == 'o' && name[2] == '_')
        {
            name += 3;
            return PORT_INOUT;
        }
    }
    else if (*name == 'o')
    {
        if (name[1] == '_')
        {
            name += 2;
            return PORT_OUTPUT;
        }
        if (name[1] == 'u' && name[2] == 't' && name[3] == '_')
        {
            name += 3;
            return PORT_OUTPUT;
        }
    }
    return -1;
}

#define IS_ALPHA(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define TOLOWER(c)  (((c) >= 'A' && (c) <= 'Z') ? ((c) + 'a' - 'A') : (c))

// See if sz1 contains the substring given by (sz2, len)
static bool hasSubstring (const char *sz1, const char *sz2, int len)
{
    for ( ; *sz1 ; sz1++)
    {
        int i;
        for (i = 0 ; i < len ; i++)
        {
            if (TOLOWER(sz1[i]) != TOLOWER(sz2[i]))
                break;
        }
        if (i == len)
            return true;
    }
    return false;
}

// See if sz1 contains a substring of sz2 that has at least two characters
// and is surrounded by non-letters.
static bool hasSubstring (const char *sz1, const char *sz2)
{
    for (const char *ch = sz2 ; *ch ; ch++)
    {
        // Find the start of an alpha sequence of length >= 2
        if (!IS_ALPHA(*ch) || !(IS_ALPHA(ch[1])))
            continue;
        if (ch > sz2 && IS_ALPHA(ch[-1]))
            continue;

        // Find the end of the alpha sequence
        int len;
        for (len = 2 ; IS_ALPHA(ch[len]) ; len++);

        if (hasSubstring(sz1, ch, len))
            return true;
    }
    return false;
}

static bool namesAreSimilar (const char *cname, const char *vname)
{
    // Strip off and validate any leading i_, o_, in_, out_, io_
    int cdir = stripType(cname);
    int vdir = stripType(vname);
    if (cdir != -1 && vdir != -1 && cdir != vdir)
        return false;

    return !strcmp(cname, vname) || hasSubstring(cname, vname) || hasSubstring(vname, cname);
}

// Strict test for when ExactPortNames is true.  The names must match exactly with
// the following exceptions:
//
//  (1) Non-alphanumeric characters are ignored after the first character
//  (2) Case is ignored
//  (3) i_/o_ must appear at the start of the Verilog port name; the matching in_/out_
//      can appear either at the start of the Cascade port name or after a period.
//  (4) leading interface names can be skipped in Cascade port name
//
static bool _namesMatch (const char *cname, const char *vname)
{
    // Check for the verilog port direction
    if (!((*vname == 'i' || *vname == 'o') && vname[1] == '_'))
        return false;
    bool parsed_cdir = false; // True once we've parsed in_/out_ from cname
    bool check_cdir = true;   // True if we should check for in_/out_ (at start and after '.') 

    // Match the names
    const char *cch = cname;
    const char *vch = vname + 2;
    while (*cch && *vch)
    {
        // Check for the Cascade port direction
        if (check_cdir)
        {
            check_cdir = false;
            if (!strncmp(cch, "in_", 3) || !strncmp(cch, "out_", 4) ||
                !strncmp(cch, "i_", 2) || !strncmp(cch, "o_", 2))
            {
                if (*cch != *vname)
                    return false;
                parsed_cdir = true;
                cch += (cch[1] == '_') ? 2 : (cch[2] == '_') ? 3 : 4;
                continue;
            }
        }

        // Check for a match
        if (tolower(*cch) != tolower(*vch))
            return false;

        // Advance
        for (cch++ ; strchr(".,()[]", *cch) ; cch++);
        for (vch++ ; *vch == '_' ; vch++);

        // Do we need to check for the Cascade port direction?
        check_cdir = !parsed_cdir && (cch[-1] == '.');
    }

    // Names match if we've reached the end of both strings
    return parsed_cdir && !*cch && !*vch;
}

static bool namesMatch (const char *cname, const char *vname)
{
    while (cname)
    {
        if (!strcmp(cname, vname) || _namesMatch(cname, vname))
            return true;
        cname = strchr(cname, '.');
        if (cname)
            cname++;
    }
    return false;
}

/////////////////////////////////////////////////////////////////
//
// VerilogPortBinding
//
/////////////////////////////////////////////////////////////////

static string s_bindingFmt;

//----------------------------------
// Helper function
//----------------------------------
static const char *strDirection (int direction)
{
    return (direction == vpiInput) ? "Input" :
        (direction == vpiOutput) ? "Output" :
        (direction == vpiInout) ? "InOut" :
        "???";
}

//----------------------------------
// Constructor
//----------------------------------
VerilogPortBinding::VerilogPortBinding (vpiHandle port, 
                                        vpiHandle module, 
                                        const InterfaceEntry *entry, 
                                        void *address,
                                        const strbuff &name,
                                        bool reverseDirection)
: m_vpiHandle(port ? vpi_handle_by_name(vpi_get_str(vpiName, port), module) : NULL),
  m_direction((Cascade::PortDirection) entry->direction),
  m_info(entry->portInfo),
  m_sizeInBits(0),
  m_initializedDPI(false),
  m_port((Port<byte> *) address),
  m_next(NULL)
{
    if (port)
    {
        // Get the port name
        m_name = string(vpi_get_str(vpiFullName, module)) + "." + vpi_get_str(vpiName, port);
        if (entry->name)
            validatePortName(*name);

        // Verify the direction
        int portDirection = vpi_get(vpiDirection, port);
        int expectedDirection = 
            ((m_direction == PORT_INPUT) || (m_direction == PORT_RESET) || (m_direction == PORT_CLOCK)) ? vpiInput :
            (m_direction == PORT_OUTPUT) ? vpiOutput :
            vpiInout;
        assert_always(portDirection == expectedDirection, 
                      "Mismatched port direction\n"
                      "    Verilog:  dir = %-6s  port = '%s'\n"
                      "    Cascade:  dir = %-6s  port = '%s'",
                      strDirection(portDirection), *m_name,
                      strDirection(expectedDirection), *name);

        // Verify the size
        m_sizeInBits = vpi_get(vpiSize, port);
        validatePortSize(*name);

        log(*s_bindingFmt, *name, *m_name);
    }
    else if (entry->name)
        m_name = *name;
    else
        m_name = "";

    if (reverseDirection)
    {
        if (m_direction == PORT_INPUT)
            m_direction = PORT_OUTPUT;
        else if (m_direction == PORT_OUTPUT)
            m_direction = PORT_INPUT;
    }

    if (m_direction != PORT_RESET && m_direction != PORT_CLOCK)
    {
        if (m_direction == PORT_INPUT)
        {
            assert_always(!m_port->wrapper->connectedTo, "Cannot bind Verilog port to connected port %s", *name);
            assert_always(m_port->wrapper->type != PORT_PULSE, "Cannot bind Verilog port to pulse port %s", *name);
        }

        // Resolve combinational connections so that we're looking at the right port 
        // when we inspect the flags and possibly set the type to PORT_LATCH.
        m_port = m_port->wrapper->getTerminalWrapper()->port;
        
        // If this port is written from Verilog, then set the type to latch so that it doesn't
        // have any valid flags in debug builds.  Also set the 'verilog_wr' wrapper flag.
        if (m_direction == PORT_INPUT || m_direction == PORT_INOUT)
        {
            m_port->setType(PORT_LATCH);
            m_port->wrapper->verilog_wr = 1;
        }
        else
            m_port->wrapper->verilog_rd = 1;
    }
}

//----------------------------------
// validatePortSize()
//----------------------------------
void VerilogPortBinding::validatePortSize (const char *cname) const
{
    int compareSize = m_sizeInBits;
    if (!m_info->exact)
    {
        if (compareSize <= 8)
            compareSize = 8;
        else if (compareSize <= 16)
            compareSize = 16;
        else if (compareSize <= 32)
            compareSize = 32;
        else
            compareSize = (compareSize + 31) & ~31;
    }
    assert_always(compareSize == m_info->sizeInBits,
                  "Mismatched port size\n"
                  "    Verilog:  size = %2d  port = '%s'\n"
                  "    Cascade:  size = %2d%s  port = '%s'",
                  m_sizeInBits, *m_name,
                  m_info->sizeInBits, m_info->exact ? "" : "  [rounded]", cname);
}

//----------------------------------
// validatePortName()
//----------------------------------
static const char *stripDots (const char *ch)
{
    const char *dot;
    while ((dot = strchr(ch, '.')) != NULL)
        ch = dot + 1;
    return ch;
}

void VerilogPortBinding::validatePortName (const char *cname) const
{
    if (!cname || !*cname)
        return;
    bool match = false;
    const char *vname = stripDots(*m_name);
    if (m_direction == PORT_RESET || m_direction == PORT_CLOCK)
        match = !strcmp(stripDots(cname), vname);
    else if (params.ExactPortNames)
        match = namesMatch(cname, vname);
    else
        match = namesAreSimilar(stripDots(cname), vname);

    assert_always(match, "Cannot bind Verilog port '%s' to Cascade port '%s' (name mismatch)\n", *m_name, cname);
}

//----------------------------------
// initializeDPI()
//----------------------------------
void VerilogPortBinding::initializeDPI (const char *name, int sizeInBits)
{
    string cname = m_name;
    m_name = name;
    validatePortName(*cname);

    m_sizeInBits = sizeInBits;
    validatePortSize(*cname);

    m_initializedDPI = true;
}

//----------------------------------
// updateIn()
//----------------------------------

struct ReadVpiWordArray : public IReadWordArray
{
    ReadVpiWordArray (const s_vpi_vecval *v) : value(v) {}

    uint32 getWord (int i)
    {
        return value[i].aval & ~value[i].bval;
    }

    const s_vpi_vecval *value;
};

void VerilogPortBinding::copyIn (IReadWordArray &array)
{
    // Copy the value
    byte *dest = (m_direction == PORT_RESET) ? ((byte *) m_resetPort) : m_port->value;
    m_info->bitmap->mapVtoC(dest, array);
}

void VerilogPortBinding::updateIn ()
{
    // Fetch the value
    s_vpi_value value;
    value.format = vpiVectorVal;
    vpi_get_value(m_vpiHandle, &value);

    // Copy the value
    ReadVpiWordArray array(value.value.vector);
    copyIn(array);
}

void VerilogPortBinding::updateIn (svBitVecVal *value, const char *name, int sizeInBits)
{
    // On the first updateIn(), validate and store the port size and name
    if (!m_initializedDPI)
        initializeDPI(name, sizeInBits);

    // Copy the value
    ReadUint32Array array(value);
    copyIn(array);
}

//----------------------------------
// updateOut()
//----------------------------------

struct WriteVpiWordArray : public IWriteWordArray
{
    WriteVpiWordArray (s_vpi_vecval *v, int sizeInBits) : value(v), size(sizeInBits), changed(false) {}

    void setWord (int i, uint32 w)
    {
        uint32 mask = (i == (size / 32)) ? ((1 << (size & 31)) - 1) : 0xffffffff;
        changed |= (((w ^ value[i].aval) | value[i].bval) & mask) != 0;
        value[i].aval = w;
        value[i].bval = 0;
    }

    s_vpi_vecval *value;
    int size;
    bool changed;
};

void VerilogPortBinding::updateOut ()
{
    // Fetch the current value
    s_vpi_value value;
    value.format = vpiVectorVal;
    vpi_get_value(m_vpiHandle, &value);
    s_vpi_vecval *vecval = value.value.vector;

    // Construct the current value, checking to see if there is a change
    WriteVpiWordArray array(vecval, m_info->sizeInBits);
    const byte *portValue = (m_direction == PORT_RESET) ? ((const byte *) m_resetPort) : m_port->value;
    m_info->bitmap->mapCtoV(array, portValue);

    // Set the value if there's a change
    if (array.changed)
    {
        vpiHandle h = vpi_put_value(m_vpiHandle, &value, NULL, vpiForceFlag);
        vpi_free_object(h);
    }
}

void VerilogPortBinding::updateOut (svBitVecVal *value, const char *name, int sizeInBits)
{
    // On the first updateOut(), validate and store the port size and name
    if (!m_initializedDPI)
        initializeDPI(name, sizeInBits);

    // Copy the bits
    m_info->bitmap->mapCtoV(value, m_port->value);
}

/////////////////////////////////////////////////////////////////
//
// Callbacks
//
/////////////////////////////////////////////////////////////////
static PLI_INT32 vpi_clock_callback (s_cb_data *cb)
{
    if (cb->value->value.integer)
    {
        VerilogClockBinding *clk = (VerilogClockBinding *) cb->user_data;
        clk->cmodule->vpiTick(clk->clock);
    }
    return 0;
}

static PLI_INT32 vpi_update_out_callback (s_cb_data *cb)
{
    ((VerilogModule *) cb->user_data)->updateOut();
    return 0;
}

/////////////////////////////////////////////////////////////////
//
// VerilogModule
//
/////////////////////////////////////////////////////////////////

VerilogModule *VerilogModule::s_first = NULL;

static void validateNames (PortSet::PortSetType portSet, InterfaceDescriptor *descriptor, Component *module)
{
    const char *setName = (portSet == PortSet::Clocks) ? "Clock" : "Reset";

    int numPorts = 0;
    bool unnamedPort = false;
    for (PortIterator it(portSet, descriptor, module) ; it ; it++, numPorts++)
        unnamedPort |= !it.entry()->name;

    assert((numPorts < 2) || !unnamedPort, "Multiple %s ports in Verilog module %s require explicit names."
           "\n    Please use the %s() macro to declare the ports.", setName, *module->getName(), setName);
}

//----------------------------------
// Construct a new verilog module
//----------------------------------
VerilogModule::VerilogModule (string name, 
                              string verilogName, 
                              VerilogModuleInterface type, 
                              Component *component, 
                              IMPL_CTOR)
: m_cmoduleName(string("[CModule]") + verilogName),
  m_componentName((type == VPI_SIMULATION_MODULE) ? NULL : *m_cmoduleName),
  m_module(component),
  m_type(type),
  m_impl(component ? VERILOG_MODULE : CASCADE_MODULE),
  m_portBindings(NULL),
  m_clockBindings(NULL),
  m_dpiNextPort(NULL),
  m_updateIn(true),
  m_next(s_first)
{
    s_first = this;
    log("Creating %s\n", *m_cmoduleName);

    // Construct the component if this is a Cascade component
    if (!m_module)
        m_module = VerilogModuleFactory::constructComponent(name);

    // CModule port iterator.  Don't bind clock or reset ports if this is a simulation module
    // (simulation modules are expected to control clock and reset themselves).
    InterfaceDescriptor *descriptor = m_module->getInterfaceDescriptor();
    PortSet::PortSetType portSet = PortSet::AllIOs;
    if (type != VPI_SIMULATION_MODULE)
        portSet = (PortSet::PortSetType) (PortSet::AllIOs | PortSet::Resets | PortSet::Clocks);
    PortIterator itCPorts(portSet, descriptor, m_module);
    validateNames(PortSet::Clocks, descriptor, m_module);
    validateNames(PortSet::Resets, descriptor, m_module);

    // Construct the list of CModule ports
    std::list<CModulePort> cports;
    int maxlen = 0;
    for (PortIterator itCPorts(portSet, descriptor, m_module) ; itCPorts ; itCPorts++)
    {
        if (itCPorts.hasWrapper() && itCPorts.wrapper()->noverilog)
            continue;
        CModulePort cport = { itCPorts.entry(), itCPorts.address(), itCPorts.getName() };
        cports.push_back(cport);
        int len = strlen(cport.name);
        if (len > maxlen)
            maxlen = len;
    }
    s_bindingFmt = str("  %%-%ds <=> %%s\n", maxlen);

    if (m_type != DPI_MODULE)
    {
        if (cports.size())
        {
            // Verilog port iterator
            vpiHandle module = vpi_handle_by_name((PLI_BYTE8 *) *verilogName, NULL);
            assert_always(module, "Failed to obtain module handle");
            vpiHandle itVPorts = vpi_iterate(vpiPort, module);
            assert_always(itVPorts, "Failed to obtain port iterator");

            // Construct the list of Verilog ports.  If any Verilog port name exactly matches
            // one and only one CModule port name, then bind it immediately.
            std::list<vpiHandle> vports;
            vpiHandle vport;
            while ((vport = vpi_scan(itVPorts)) != NULL)
            {
                // Look for exact name match
                std::list<CModulePort>::iterator it = cports.begin();
                std::list<CModulePort>::iterator itEnd = cports.end();
                std::list<CModulePort>::iterator match = cports.end();
                for ( ; it != itEnd ; it++)
                {
                    const char *vname = vpi_get_str(vpiName, vport);
                    if (namesMatch(it->name, vname))
                    {
                        if (match == itEnd)
                            match = it;
                        else
                        {
                            match = itEnd;
                            break;
                        }
                    } 
                }
                if (match != itEnd)
                {
                    bindPort(vport, module, *match);
                    cports.erase(match);
                }
                else
                    vports.push_back(vport);
            }
            
            // Bind the remaining ports in order
            Iterator<std::list<CModulePort> > itc(cports);
            Iterator<std::list<vpiHandle> > itv(vports);
            for ( ; itc && itv ; itc++, itv++)
                bindPort(*itv, module, *itc);
            
            // Make sure we bound all the ports
            assert_always(!itv, "Unmatched Verilog port '%s'", vpi_get_str(vpiName, *itv));
        }
    }
    else // DPI_MODULE - no port handles
    {
        VerilogPortBinding **lastBinding = &m_portBindings;
        for (const CModulePort &cport : cports)
        {
            // Create the port binding
            *lastBinding = new VerilogPortBinding(NULL, NULL, 
                                                  cport.entry, cport.address, cport.name);
            lastBinding = &((*lastBinding)->m_next);

            // If it's a clock, create the clock callback
            if (cport.entry->direction == PORT_CLOCK)
            {
                // Create the clock binding
                VerilogClockBinding *clock = new VerilogClockBinding; 
                clock->name = cport.entry->name;
                clock->clock = (Clock *) cport.address;
                clock->next = m_clockBindings;
                m_clockBindings = clock;
                
                // Set the clock to manual update
                clock->clock->setManual();
            }
        }

        // Find the first input port
        m_dpiNextPort = m_portBindings;
        dpiAdvance();
    }

    // At least one clock port is required unless this is a simulation module
    assert_always((type == VPI_SIMULATION_MODULE) || m_clockBindings, "At least one clock port is required for a Verilog module");
}

//----------------------------------
// bindPort()
//----------------------------------
void VerilogModule::bindPort (vpiHandle port, vpiHandle module, const CModulePort &cport)
{
    // Create the port binding
    bool reverseDirection = (m_impl == VERILOG_MODULE);
    VerilogPortBinding *binding = new VerilogPortBinding(port, module, cport.entry, cport.address, 
                                                         cport.name, reverseDirection);
    binding->m_next = m_portBindings;
    m_portBindings = binding;
    
    // If it's a clock, create the clock callback
    if (cport.entry->direction == PORT_CLOCK)
    {
        // Create the clock binding
        VerilogClockBinding *clock = new VerilogClockBinding; 
        clock->cmodule = this;
        clock->clock = (Clock *) cport.address;
        clock->port = vpi_handle_by_name(vpi_get_str(vpiName, port), module);
        clock->next = m_clockBindings;
        m_clockBindings = clock;
        
        // If this is a Cascade component then set the clock to manual and
        // register a callback
        if (m_impl == CASCADE_MODULE)
        {
            clock->clock->setManual();
        
            // Register the callback
            port = vpi_handle_by_name(vpi_get_str(vpiName, port), module);
            s_vpi_time  time_s  = { vpiSimTime };
            s_vpi_value value_s = { vpiIntVal };
            s_cb_data cb = { cbValueChange, &vpi_clock_callback, port, &time_s, &value_s };
            cb.user_data = (PLI_BYTE8 *) clock;
            vpiHandle callback_handle = vpi_register_cb(&cb);
            vpi_free_object(callback_handle);
        }
    }
}

//----------------------------------
// dpiAdvance()
//----------------------------------
void VerilogModule::dpiAdvance ()
{
    for ( ; m_dpiNextPort ; m_dpiNextPort = m_dpiNextPort->m_next)
    {
        if (m_dpiNextPort->m_direction == PORT_INOUT)
            break;
        if (m_updateIn && (m_dpiNextPort->m_direction == PORT_INPUT || m_dpiNextPort->m_direction == PORT_RESET))
            break;
        if (!m_updateIn && (m_dpiNextPort->m_direction == PORT_OUTPUT))
            break;
    }
}

//----------------------------------
// Destructor
//----------------------------------

VerilogModule::~VerilogModule ()
{
    while (m_portBindings)
    {
        VerilogPortBinding *temp = m_portBindings;
        m_portBindings = m_portBindings->m_next;
        delete temp;
    }
    while (m_clockBindings)
    {
        VerilogClockBinding *temp = m_clockBindings;
        m_clockBindings = m_clockBindings->next;
        delete temp;
    }
    if (m_impl == CASCADE_MODULE)
        delete m_module;
}

//----------------------------------
// dpiTransfer()
//----------------------------------
void VerilogModule::dpiTransfer (svBitVecVal *value, const char *name, int sizeInBits, bool input)
{
    // Switch from pop to push?
    if (m_updateIn != input)
    {
        m_updateIn = input;
        m_dpiNextPort = m_portBindings;
        dpiAdvance();
    }

    // Make sure there's a port
    assert_always(m_dpiNextPort, "Unmatched Verilog port '%s'", name);

    // Do the transfer
    if (value)
    {
        if (input)
            m_dpiNextPort->updateIn(value, name, sizeInBits);
        else
            m_dpiNextPort->updateOut(value, name, sizeInBits);
    }

    // Next port
    m_dpiNextPort = m_dpiNextPort->m_next;
    dpiAdvance();
}

//----------------------------------
// clkTick()
//----------------------------------
void VerilogModule::clkTick (Clock *clock)
{
    // Catch up to the verilog simulation time
    if (!Sim::verilogCallbackPump)
    {
        uint32 tf_upperTime, tf_lowerTime = tf_getlongtime((int *) &tf_upperTime);
        uint64 currTime = (((uint64) tf_upperTime) << 32) | tf_lowerTime;
        Sim::runUntil(currTime);
    }

    // Check for reset
    bool is_in_reset = false;
    for (VerilogPortBinding *port = m_portBindings ; port ; port = port->m_next)
    {
        if ((port->m_direction == PORT_RESET) && *port->m_resetPort)
        {
            is_in_reset = true;
            Sim::reset(m_module, port->m_resetPort->resetLevel());
        }
    }

    // If we're not in reset then tick the clock
    if (!is_in_reset)
        clock->tick();
}

void VerilogModule::dpiTick (const char *clockName)
{
    // Rising clock edge
    VerilogClockBinding *clock = m_clockBindings;
    for ( ; clock && clock->name && strcmp(clock->name, clockName) ; clock = clock->next);
    assert(clock, "Clock '%s' not found", clockName);
    clkTick(clock->clock);
}

void VerilogModule::vpiTick (Clock *clock)
{
    if (Sim::state != Sim::SimInitialized)
        Sim::init();

    // Copy values from Verilog to C++ if we haven't already
    if (m_updateIn)
    {
        for (VerilogPortBinding *port = m_portBindings ; port ; port = port->m_next)
        {
            if (port->m_direction == PORT_INPUT || port->m_direction == PORT_INOUT || 
                (port->m_direction == PORT_RESET && m_impl == CASCADE_MODULE))
            {
                port->updateIn();
            }
        }
    }

    // Rising clock edge
    if (clock && (m_impl == CASCADE_MODULE))
        clkTick(clock);
    
    // Schedule the update out for the next delta cycle if we haven't already
    if (m_updateIn)
    {
        s_cb_data cb = { cbReadWriteSynch, &vpi_update_out_callback };
        struct t_vpi_time time;
        time.type = vpiSimTime;
        time.high = 0;
        time.low = 0;
        cb.time = &time;
        cb.user_data = (PLI_BYTE8 *) this;
        vpiHandle callback_handle = vpi_register_cb(&cb);
        vpi_free_object(callback_handle);
    }
    m_updateIn = false;
}

//----------------------------------
// updateOut()
//----------------------------------
void VerilogModule::updateOut ()
{
    for (VerilogPortBinding *port = m_portBindings ; port ; port = port->m_next)
    {
        if (port->m_direction == PORT_OUTPUT || port->m_direction == PORT_INOUT ||
            (port->m_direction == PORT_RESET && m_impl == VERILOG_MODULE))
        {
            port->updateOut();
        }
    }
    m_updateIn = true;
}

//----------------------------------
// initModules()
//----------------------------------
void VerilogModule::initModules ()
{
    for (VerilogModule *m = s_first ; m ; m = m->m_next)
        m->init();
}

//----------------------------------
// init()
//----------------------------------
void VerilogModule::init ()
{
    // Initialize the C++ side with zero (this is necessary because
    // if there are unused bits then they will have garbage in them, and if the port
    // is, e.g., a u2, then it can have an invalid value).  
    for (VerilogPortBinding *port = m_portBindings ; port ; port = port->m_next)
    {
        if (port->m_direction == PORT_INPUT || port->m_direction == PORT_INOUT)
            memset(port->m_port->nonConstPtr(), 0, port->m_info->sizeInBytes);

        // Do an initial copy of values between Verilog and C++ for VPI modules
        if (m_type != DPI_MODULE)
        {
            if (port->m_direction == PORT_INPUT || port->m_direction == PORT_INOUT || 
                (port->m_direction == PORT_RESET && m_impl == CASCADE_MODULE))
            {
                port->updateIn();
            }
            else if (port->m_direction != PORT_CLOCK)
                port->updateOut();
        }
    }

    // If this is a Verilog module within Cascade then register the clock
    // ports with the appropriate clock domains.
    if (m_impl == VERILOG_MODULE)
    {
        for (VerilogClockBinding *clock = m_clockBindings ; clock ; clock = clock->next)
            clock->clock->resolveClockDomain()->registerVerilogClock(clock->port);
    }
}

/////////////////////////////////////////////////////////////////
//
// VerilogModuleFactory
//
/////////////////////////////////////////////////////////////////

//----------------------------------
// Register a named factory instance
//----------------------------------
VerilogModuleFactory::VerilogModuleFactory (const char *name)
{
    assert_always(s_factoryMap().find(name) == s_factoryMap().end(), "The CModule name '%s' is already in use", name);
    s_factoryMap()[name] = this;
}

//----------------------------------
// Construct a component by name
//----------------------------------
Component *VerilogModuleFactory::constructComponent (string name)
{
    assert_always(s_factoryMap().find(name) != s_factoryMap().end(), "Unknown CModule name: '%s'", *name);
    s_currName() = name;
    Component *c = s_factoryMap()[name]->constructComponent();
    s_namedParams().clear();
    s_indexedParams().clear();
    return c;
}

//----------------------------------
// Add a parameter
//----------------------------------
void VerilogModuleFactory::addNamedParam (const char *name, int val)
{
    assert_always(s_namedParams().find(name) == s_namedParams().end(), "Parameter '%s' is already defined", name);
    s_namedParams()[name] = val;
}
void VerilogModuleFactory::addIndexedParam (int val)
{
    s_indexedParams().push_back(val);
}

//----------------------------------
// Retrieve a named or indexed parameter
//----------------------------------
int VerilogModuleFactory::param (const char *name)
{
    return param(name, 0, true);
}
int VerilogModuleFactory::param (const char *name, int defaultValue, bool required)
{
    const char *ch = strchr(name, '|');
    const char *param = ch ? ch + 1 : name;
    if (s_namedParams().find(param) != s_namedParams().end())
        return s_namedParams()[param];
    int index = atoi(name);
    if (ch && ((unsigned) index) < s_indexedParams().size())
        return s_indexedParams()[index];
    assert_always(!required,
                  "Failed to construct CModule '%s': Required parameter '%s' is missing", *s_currName(), param);
    return defaultValue;
}

//----------------------------------
// Component/module registration
//----------------------------------

static std::map<string, string> s_verilogModules;

void VerilogModuleFactory::registerModule (const char *name, const char *verilogName)
{
    assert_always(s_verilogModules.find(name) == s_verilogModules.end(),
                  "Verilog module '%s' is already registered", name);
    s_verilogModules[name] = verilogName;
}

void VerilogModuleFactory::registerComponent (const char *name, Component *component)
{
    std::map<string, string>::const_iterator it = s_verilogModules.find(name);
    assert_always(it != s_verilogModules.end(), 
                  "Verilog module '%s' has not been registered", name);
    new VerilogModule(name, *(it->second), Cascade::VPI_MODULE, component);

    // Always set the first clock as the default, since otherwise we won't be able to resolve
    // any of the port clock domains.
    ClockIterator clock(component);
    if (clock)
        (*clock)->setAsDefault();
}

bool VerilogModuleFactory::isModuleRegistered (const char *name)
{
    return s_verilogModules.find(name) != s_verilogModules.end();
}

//----------------------------------
// Wrappers for static variables.  These
// functions ensure the static data members
// are intialized before they are used.
//----------------------------------

VerilogModuleFactory::FactoryMap& VerilogModuleFactory::s_factoryMap( )
{
    static FactoryMap *fm = new FactoryMap;
    return *fm;
}

string& VerilogModuleFactory::s_currName( )
{
    static string *s = new string;
    return *s;
}

std::map<string, int>& VerilogModuleFactory::s_namedParams( )
{
    static std::map<string, int> *m = new std::map<string, int>;
    return *m;
}

std::vector<int>& VerilogModuleFactory::s_indexedParams( )
{
    static std::vector<int> *v = new std::vector<int>;
    return *v;
}

////////////////////////////////////////////////////////////////////////////////
//
// try/catch macros
//
////////////////////////////////////////////////////////////////////////////////
#undef TRY
#undef CATCH
#define TRY try {
#define CATCH \
    } \
    catch (descore::runtime_error &error) \
    { \
        error.reportAndExit(); \
    }

////////////////////////////////////////////////////////////////////////////////
//
// vpi_run_callback()
//
////////////////////////////////////////////////////////////////////////////////
static PLI_INT32 vpi_run_callback (s_cb_data *)
{
    TRY

    // Copy in values (also schedules the copy out)
    for (VerilogModule *m = VerilogModule::s_first ; m ; m = m->m_next)
        m->vpiTick(NULL);
    
    Sim::run();
    uint64 nextTick = Sim::simTime;
    
    // Translate nextTick to the Verilog time precision
    int precision = tf_gettimeprecision();
    assert_always(precision <= -12, "Cascade requires a time precision of 1ps or finer");
    for ( ; precision < -12 ; precision++)
        nextTick *= 10;
    
    // Schedule the next callback
    s_cb_data cb = { cbAtStartOfSimTime, &vpi_run_callback };
    struct t_vpi_time time;
    time.type = vpiSimTime;
    time.high = (uint32) (nextTick >> 32);
    time.low = (uint32) nextTick;
    cb.time = &time;
    vpiHandle callback_handle = vpi_register_cb(&cb);
    vpi_free_object(callback_handle);

    CATCH

    return 0;
}

/////////////////////////////////////////////////////////////////
//
// Shared VPI/DPI helpers
//
/////////////////////////////////////////////////////////////////
static void logConsoleOutput (const char *sz, FILE *)
{
    io_printf((char *) sz);
}

static void cleanupVerilogSimulation (int)
{
    while (VerilogModule::s_first)
    {
        VerilogModule *m = VerilogModule::s_first;
        VerilogModule::s_first = m->m_next;
        delete m;
    }
}

static void initVerilogSimulation ()
{
    Sim::isVerilogSimulation = true;
    descore::setLogConsoleOutput(&logConsoleOutput);
    vcs_atexit(&cleanupVerilogSimulation);
}

END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// DPI functions
//
/////////////////////////////////////////////////////////////////
extern "C" void setCModuleParam (const char *name, int value)
{
    TRY
    if (!Sim::isVerilogSimulation)
        Cascade::initVerilogSimulation();
    Cascade::VerilogModuleFactory::addNamedParam(name, value);
    CATCH
}

extern "C" void *createCModule (const char *name, const char *verilogName)
{
    TRY
    if (!Sim::isVerilogSimulation)
        Cascade::initVerilogSimulation();
    return new Cascade::VerilogModule(name, verilogName, Cascade::DPI_MODULE);
    CATCH
    return NULL;
}

extern "C" void clockCModule (void *module, const char *clockName)
{
    TRY
    // Sim::init() will be automatically called from tick() via runUntil() if necessary
    if (!Sim::isVerilogSimulation)
        Cascade::initVerilogSimulation();
    ((Cascade::VerilogModule *) module)->dpiTick(clockName);
    CATCH
}

static void pushToC (void *module, const svBitVecVal *value, const char *name, int sizeInBits)
{
    TRY
    if (!Sim::isVerilogSimulation)
        Cascade::initVerilogSimulation();
    if (Sim::state != Sim::SimInitialized)
        Sim::init();
    ((Cascade::VerilogModule *) module)->dpiTransfer((svBitVecVal *) value, name, sizeInBits, true);
    CATCH
}

static void popFromC (void *module, svBitVecVal *value, const char *name, int sizeInBits)
{
    TRY
    if (!Sim::isVerilogSimulation)
        Cascade::initVerilogSimulation();
    if (Sim::state != Sim::SimInitialized)
        Sim::init();
    ((Cascade::VerilogModule *) module)->dpiTransfer(value, name, sizeInBits, false);
    CATCH
}

extern "C" void ignoreCPort (void *module, const char *name, int input)
{
    TRY
    if (!Sim::isVerilogSimulation)
        Cascade::initVerilogSimulation();
    if (Sim::state != Sim::SimInitialized)
        Sim::init();
    ((Cascade::VerilogModule *) module)->dpiTransfer(NULL, name, 0, input ? true : false);
    CATCH
}

#define DECLARE_DPI_TRANSFER(N) \
extern "C" void pushToC##N (void *module, const svBitVecVal *value, const char *name, int sizeInBits) \
{ \
    pushToC(module, value, name, sizeInBits); \
} \
extern "C" void popFromC##N (void *module, svBitVecVal *value, const char *name, int sizeInBits) \
{ \
    popFromC(module, value, name, sizeInBits); \
}

#define DECLARE_DPI_TRANSFERS(N1,N2,N3,N4,N5,N6,N7,N8) \
DECLARE_DPI_TRANSFER(N1) \
DECLARE_DPI_TRANSFER(N2) \
DECLARE_DPI_TRANSFER(N3) \
DECLARE_DPI_TRANSFER(N4) \
DECLARE_DPI_TRANSFER(N5) \
DECLARE_DPI_TRANSFER(N6) \
DECLARE_DPI_TRANSFER(N7) \
DECLARE_DPI_TRANSFER(N8) \

DECLARE_DPI_TRANSFERS(32,  64,  96,  128, 160, 192, 224, 256);
DECLARE_DPI_TRANSFERS(288, 320, 352, 384, 416, 448, 480, 512);
DECLARE_DPI_TRANSFERS(544, 576, 608, 640, 672, 704, 736, 768);
DECLARE_DPI_TRANSFERS(800, 832, 864, 896, 928, 960, 992, 1024);

extern "C" void setCModuleTraces (const char *traces)
{
    TRY
    descore::setTraces(traces);
    CATCH
}

extern "C" void dumpCModuleVars (const char *dumps)
{
    TRY
    Sim::setDumps(dumps);
    CATCH
}

extern "C" void disableCAssertion (const char *message)
{
    TRY
    descore::assertParams.disabledAssertions->push_back(message);
    CATCH
}

extern "C" void setCParameter (const char *name, const char *value)
{
    TRY
    bool success = Parameter::setValueByString(name, value);
    assert_always(success, "Failed to set parameter %s to %s", name, value);
    CATCH
}

/////////////////////////////////////////////////////////////////
//
// VPI functions
//
/////////////////////////////////////////////////////////////////

static vpiHandle tfcall;
static vpiHandle args;

static void vpi_begin ()
{
    tfcall = vpi_handle(vpiSysTfCall, 0);
    assert_always(tfcall, "Failed to obtain TfCall handle");
    args = vpi_iterate(vpiArgument, tfcall);
    assert_always(args, "Failed to obtain TfCall args");
}

static vpiHandle vpi_getHandle (const char *name)
{
    vpiHandle arg = vpi_scan(args);
    assert_always(arg, "Too few arguments supplied to %s: missing %s", 
                  vpi_get_str(vpiName, tfcall), name);
    return arg;
}

static string vpi_getString (const char *name)
{
    vpiHandle arg = vpi_getHandle(name);
    s_vpi_value value;
    value.format = vpiStringVal;
    vpi_get_value(arg, &value);
    return value.value.str;
}

static int vpi_getInt (const char *name)
{
    vpiHandle arg = vpi_getHandle(name);
    s_vpi_value value;
    value.format = vpiIntVal;
    vpi_get_value(arg, &value);
    return value.value.integer;
}

static void createCModule (Cascade::VerilogModuleInterface type)
{
    if (!Sim::isVerilogSimulation)
        Cascade::initVerilogSimulation();

    s_vpi_value value;

    vpi_begin();

    // CModule name
    string name = vpi_getString("CModule name");

    // Verilog module name
    vpiHandle module = vpi_handle(vpiScope, tfcall);
    assert_always(module, "Failed to obtain module handle");
    string verilogName = vpi_get_str(vpiFullName, module);

    // Additional arguments
    vpiHandle arg;
    while ((arg = vpi_scan(args)))
    {
        value.format = vpiObjTypeVal;
        vpi_get_value(arg, &value);
        assert_always(value.format == vpiIntVal, "CModule parameters must be integers");
        Cascade::VerilogModuleFactory::addIndexedParam(value.value.integer);
    }

    // Create the CModule
    new Cascade::VerilogModule(name, verilogName, type);
}

extern "C" void vpi_createCModule ()
{
    TRY
    createCModule(Cascade::VPI_MODULE);
    CATCH
}

extern "C" void vpi_setCModuleParam ()
{
    TRY
    if (!Sim::isVerilogSimulation)
        Cascade::initVerilogSimulation();
    vpi_begin();
    string name = vpi_getString("CModule parameter name");
    int value = vpi_getInt("CModule parameter value");
    Cascade::VerilogModuleFactory::addNamedParam(*name, value);
    CATCH
}

extern "C" void vpi_registerModule ()
{
    TRY
    vpi_begin();
    string name = vpi_getString("module name");
    vpiHandle module = vpi_getHandle("module");
    string verilogName = vpi_get_str(vpiFullName, module);
    Cascade::VerilogModuleFactory::registerModule(*name, *verilogName);
    CATCH
}

extern "C" void vpi_runSimulation ()
{
    static bool running_simulation = false;
    TRY
    assert_always(!running_simulation, "$run_simulation cannot be called multiple times");
    running_simulation = true;

    createCModule(Cascade::VPI_SIMULATION_MODULE);
    Sim::init();
    Sim::verilogCallbackPump = true;
    Cascade::vpi_run_callback(NULL);
    CATCH
}

extern "C" void vpi_setTraces ()
{
    TRY
    vpi_begin();

    // Traces string
    string traces = vpi_getString("Trace specifiers");
    descore::setTraces(*traces);
    CATCH
}

extern "C" void vpi_dumpvars ()
{
    TRY
    vpi_begin();

    // Signals string
    string dumps = vpi_getString("Dump specifiers");
    Sim::setDumps(*dumps);
    CATCH
}

extern "C" void vpi_disableAssertion ()
{
    TRY
    vpi_begin();
    string message = vpi_getString("Assertion message");
    descore::assertParams.disabledAssertions->push_back(message);
    CATCH
}

extern "C" void vpi_setParameter ()
{
    TRY
    vpi_begin();
    string name = vpi_getString("Parameter name");
    string value = vpi_getString("Parameter value");
    bool success = Parameter::setValueByString(name, value);
    assert_always(success, "Failed to set parameter %s to %s", *name, *value);
    CATCH
}

#endif // #ifdef _VERILOG

