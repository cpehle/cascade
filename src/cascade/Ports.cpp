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
// Ports.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/22/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Cascade.hpp"
#include "Constants.hpp"

BEGIN_NAMESPACE_CASCADE

// All ports are added to this list as they are constructed
static PortList g_ports;         

// During initialization, combinationally connected ports are put aside and
// then handled at the end.
static PortList g_connectedPorts;

// The order here must match the order of PortDirection in Interface.hpp
const char *PortName::portName[NUM_PORT_DIRECTIONS] = 
{
    "Input", "Output", "InOut", "Register", "FifoInput", "FifoOutput", "Temp", "Clock", "Reset", "Signal"
};

// Helper for constructing port arrays
int PortWrapper::s_arrayIndex = -1;

//////////////////////////////////////////////////////////////////
//
// PortWrapper()
//
//////////////////////////////////////////////////////////////////
PortWrapper::PortWrapper (void *_port, uint16 _size, PortDirection dir)
: port((Port<byte> *) _port),
size(_size),
direction(dir),
type(PORT_NORMAL),
connection(PORT_UNCONNECTED),
readOnly(0),
producer(0),
isD(0),
noverilog(0),
arrayInternal((s_arrayIndex >= 0) ? (s_arrayIndex++ > 0) : 0),
patched(0),
nofake(0),
fifoDisableFlowControl(0),
mark(0),
verilog_wr(0),
verilog_rd(0),
fifoSize(0),
delay(0),
parent(_port ? Hierarchy::getComponent() : NULL),
next(NULL),
connectedTo(NULL)
{
    if (dir != PORT_TEMP)
    {
        g_ports.addPort(this);
        Sim::stats.numPorts++;
    }
    if (dir == PORT_INFIFO || dir == PORT_OUTFIFO)
        Sim::stats.numFifos++;
}

//////////////////////////////////////////////////////////////////
//
// wireTo()
//
// Wire a port directly to a variable of the same type.  It is an 
// error to wire a port to a value if the port is already connected.
//
//////////////////////////////////////////////////////////////////
void PortWrapper::wireTo (const void *data)
{
    assert_always(type == PORT_NORMAL, "Cannot wire port to variable after its type has been set");

    // Validation and find the terminal port if this is an InOut
    PortWrapper *terminalPort = (direction == PORT_INOUT) ? getTerminalWrapper() : this;
    assert_always(!terminalPort->connectedTo, "Port is already connected to a constant, variable or another port");
    assert_always(!terminalPort->verilog_wr, "Port is already bound to a Verilog port");

    terminalPort->wire = data;
    terminalPort->connection = PORT_WIRED;
    terminalPort->readOnly = 1;
}

//////////////////////////////////////////////////////////////////
//
// wireToConst()
//
// Wire a port directly to a constant of the same type.  It is an 
// error to wire a port to a value if the port (or a connected port) 
// is already wired.
//
//////////////////////////////////////////////////////////////////
void PortWrapper::wireToConst (const void *data)
{
    assert_always(type == PORT_NORMAL, "Cannot wire port to constant after its type has been set");

    // Validation and find the terminal port if this is an InOut
    PortWrapper *terminalPort = (direction == PORT_INOUT) ? getTerminalWrapper() : this;
    assert_always(!terminalPort->connectedTo, "Port is already connected to a constant, variable or another port");
    assert_always(!terminalPort->verilog_wr, "Port is already bound to a Verilog port");

    // Wire it up
    terminalPort->constant = Constant::getConstant(size, (const byte *) data);
    terminalPort->connection = PORT_CONSTANT;
    terminalPort->readOnly = 1;
    writers.clear();
}

//////////////////////////////////////////////////////////////////
//
// connect()
//
// Connect one port to another.  Combinationally connected ports become synonyms; 
// they will refer to the same value and valid flag.  It is an error to connect
// a port to itself, and it is an error to connect two ports which are
// wired (directly or indirectly) to variables or constants.
//
// The following connection rules are enforced by making certain
// connections private or unavailable at compile time:
//
// * InOuts can only be connect to other InOuts, and only with combinational connections
//
// Other connection rules enforced in this function:
//
// * port is read-only after connection except for InOut
// * port cannot be connected twice unless it is an InOut
//
//////////////////////////////////////////////////////////////////
void PortWrapper::connect (PortWrapper *rhs, int _delay)
{
    assert_always(rhs, "Attempted to connect to NULL port");
    assert_always(rhs != this, "Cannot connect port to itself");
    const char *errmsg = NULL;

    if (isFifo() != rhs->isFifo())
        errmsg = "fifos can only be connected to other fifos";
    else if (direction == PORT_INOUT)
    {
        if (rhs->direction != PORT_INOUT)
            errmsg = "InOut ports can only be connected to InOut ports";
        else if (_delay)
            errmsg = "synchronous connections not allowed for InOut ports";
    }
    else
    {
        if (connectedTo)
            errmsg = "port is already connected";
        else if (verilog_wr)
            errmsg = "port is already bound to a Verilog port";
    }

    // Fifo-specific validation
    if (isFifo() && rhs->isFifo())
    {
        if (rhs->producer)
            errmsg = "source has already been connected to";
        else if (rhs->connection & FIFO_NOREADER)
            errmsg = "source has been sent to the bit bucket";
        else if (rhs->triggers.size())
            errmsg = "source activates a trigger";
        else if (connection & FIFO_NOWRITER)
            errmsg = "target has been wired to zero";
    }

    assert_always(!errmsg, "Can't connect to %s: %s", *rhs->getName(), errmsg);

    // Ignore the delay if setDelay() has been called for this port
    int delay = this->delay ? this->delay : _delay;

    // Indicate that something is connected to the RHS
    rhs->producer = 1;
    if (isFifo())
        rhs->readers.clear();

    // Find the terminal ports for InOuts (which can have multiple connections)
    // to avoid creating combinational cycles
    PortWrapper *port1 = (direction == PORT_INOUT) ? getTerminalWrapper() : this;
    PortWrapper *port2 = (direction == PORT_INOUT) ? rhs->getTerminalWrapper() : rhs;

    // If the terminal ports are distinct then we don't have
    // to worry about creating a cycle.
    if (port1 != port2)
    {
        // If port1 is already connected to something then reverse them
        // (This can only happen if we're connecting InOuts)
        if (port1->connectedTo)
        {
            PortWrapper *temp = port1;
            port1 = port2;
            port2 = temp;
        }

        // At this point it's an error for port1 to be connected
        assert_always(!port1->connectedTo, "Cannot connect ports %s and %s"
            " because they are each already wired to a constant or variable", *getName(), *rhs->getName());

        // We made it - connect the nodes
        port1->connectedTo = port2;
        if (!isFifo())
            port1->connection = delay ? PORT_SYNCHRONOUS : PORT_CONNECTED;
        port1->delay = delay;
    }

    readOnly = (direction != PORT_INOUT) ? 1 : 0;
    if (readOnly)
        writers.clear();
}

////////////////////////////////////////////////////////////////////////////////
//
// setDelay()
//
////////////////////////////////////////////////////////////////////////////////
void PortWrapper::setDelay (int _delay)
{
    assert_always(_delay >= 0);
    if (!_delay)
        return;
    delay = _delay;
    if (connection == PORT_CONNECTED)
        connection = PORT_SYNCHRONOUS;
}

/////////////////////////////////////////////////////////////////
//
// setType()
//
/////////////////////////////////////////////////////////////////
void PortWrapper::setType (PortType _type)
{
    assert_always(!connectedTo, "Cannot set type of port connected to a constant, variable or another port");
    assert_always(!verilog_wr, "Cannot set type of port bound to a Verilog port");
    type = _type;
}

/////////////////////////////////////////////////////////////////
//
// addTrigger()
//
/////////////////////////////////////////////////////////////////
void PortWrapper::addTrigger (const Trigger &trigger)
{ 
    if (isFifo())
        assert_always(triggers.empty(), "A fifo can have at most one trigger target");
    triggers.push(trigger);
    Sim::stats.numTriggers++;
}

/////////////////////////////////////////////////////////////////
//
// getFifoClockPeriod()
//
/////////////////////////////////////////////////////////////////
int PortWrapper::getFifoClockPeriod (int defaultPeriod) const
{
    for (const PortWrapper *port = this ; port ; port = port->connectedTo)
    {
        int period = port->getClockDomain()->getPeriod();
        if (period)
            return period;
    }
    return defaultPeriod;
}

/////////////////////////////////////////////////////////////////
//
// resolveFifo()
//
/////////////////////////////////////////////////////////////////
void PortWrapper::resolveFifo ()
{
    // Always run the resolution from the last fifo in the chain
    // (i.e. the one from which data is read).
    if (producer)
        return;

    // Walk down the chain of fifos to determine the total delay and size.  
    // Each fifo contributes its own size and its own delay in clock cycles of 
    // its own clock domain.
    int totalDelay = 0;
    int totalSize = 0;
    bool disableFlowControl = !triggers.empty();
    PortWrapper *producer = NULL;
    PortWrapper *port;
    int period = params.DefaultClockPeriod;
    for (port = this ; port ; producer = port, port = port->connectedTo)
    {
        totalSize += port->fifoSize;
        period = port->getFifoClockPeriod(period);
        totalDelay += port->delay * period;
        disableFlowControl |= port->fifoDisableFlowControl;
    }
    assert_always(readers.size() || (connection & FIFO_NOREADER) || (triggers.size() && totalDelay),
        "Fifo has no reader and has not been sent to the bit bucket");
    assert_always(producer->writers.size() || (producer->connection & FIFO_NOWRITER),
        "Fifo has no writer and has not been wired to zero");

    // Compute the minimum size required to avoid bubbles (if flow control is enabled)
    // or overflow (if flow control is disabled).
    int consumerClockPeriod = getFifoClockPeriod(period);
    int producerClockPeriod = producer->getFifoClockPeriod(period);
    totalDelay = (totalDelay + consumerClockPeriod - 1) / consumerClockPeriod;
    int minSize = 0;
    if (disableFlowControl)
    {
        // The worst case delay in picoseconds from the first push to the first
        // pop is d1 = delay * consumerClockPeriod.  In that time, the worst case
        // number of pushes before the first pop is d1 / producerClockPeriod + 1
        //
        //            
        //            push  push  push  push  push
        //  producer  |     |     |     |     |
        //                                        pop
        //  consumer  |      |      |      |      |
        //             \__________________________/
        //                          |
        //                       delay = 4
        // 
        //
        minSize = totalDelay * consumerClockPeriod / producerClockPeriod + 1;
    }
    else
    {
        // Same analysis, but use twice the delay to account for when the first
        // pop is visible back at the producer.
        minSize = 2 * totalDelay * consumerClockPeriod / producerClockPeriod + 1;
    }
    
    // If the fifo size is zero, then automatically size it to minSize.  Otherwise,
    // make sure that the size is at least minSize.  If it's not, generate a warning
    // if flow control is enabled, and an error if flow control is disabled.
    if (!totalSize)
        totalSize = minSize;
    else if (totalSize < minSize)
    {
        if (disableFlowControl)
            die("Fifo size must be at least %d to accomodate the specified delay", minSize);
        else if (params.FifoSizeWarnings)
        {
            log("Warning: Fifo %s with size %d must have size at least %d\n"
                "         to achieve full throughput with the specified delay\n",
                *getName(), totalSize, minSize);
        }
    }

    // Set the delay and size of the producer fifo and reparent it under the consumer
    assert_always(totalDelay < CASCADE_MAX_FIFO_DELAY, "Fifo delay (%d) exceeds maximum of %d", totalDelay, CASCADE_MAX_FIFO_DELAY);
    assert_always(totalSize * size < CASCADE_MAX_FIFO_SIZE, "Fifo size in bytes (%d) exceeds maximum of %d", size, CASCADE_MAX_FIFO_SIZE);
    producer->delay = totalDelay;
    producer->fifoSize = totalSize;
    producer->fifoDisableFlowControl = disableFlowControl;
    if (producer != this)
    {
        producer->parent = parent;
        if (readers.size())
        {
            CascadeValidate(readers.size() == 1, "Multiple readers declared for fifo %s", *getName());
            CascadeValidate(producer->readers.size() == 0, "Producer fifo %s has a reader", *getName());
            producer->readers.push_back(readers[0]);

            // Add a strong edge to the update graph if necessary
            if (!totalDelay && producer->writers.size())
            {
                if ((producer->readers[0]->clockDomain == producer->writers[0]->clockDomain) &&
                    (producer->readers[0] != producer->writers[0]))
                    producer->writers[0]->addStrongEdge(producer->readers[0], producer);
            }
        }
        else if (connection & FIFO_NOREADER)
            producer->connection |= FIFO_NOREADER;
        if (triggers.size())
            producer->triggers.push(triggers[0]);
    }

    // Resolve the connectedTo pointers
    PortWrapper *next;
    port = this;
    while (port != producer)
    {
        next = port->connectedTo;
        port->connectedTo = producer;
        port = next;
    }
}

//////////////////////////////////////////////////////////////////
//
// resolveNet()
//
//////////////////////////////////////////////////////////////////
void PortWrapper::resolveNet ()
{
    if (mark)
        return;
    mark = 1;

    // Deal with constants immediately
    if (connection == PORT_CONNECTED || connection == PORT_SYNCHRONOUS)
    {
        connectedTo->resolveNet();
        if (connectedTo->connection == PORT_CONSTANT)
        {
            connection = PORT_CONSTANT;
            constant = connectedTo->constant;
            return;
        }

        // Resolve the connection to a terminal port.  Need to use a while loop instead of
        // an if statement because the source port might not have been resolved if we're in
        // a loop.
        while (connectedTo->connection == PORT_CONNECTED)
            connectedTo = connectedTo->connectedTo;
        if (connection == PORT_SYNCHRONOUS)
            connectedTo->isD = 1;
    }

    // Verify that all writers are in the same clock domain
    for (int i = 1 ; i < writers.size() ; i++)
        assert_always(writers[0]->clockDomain == writers[i]->clockDomain, "Port is written from two different clock domains");

    if (connection == PORT_SYNCHRONOUS)
    {
        // Verify that all readers are in the same clock domain
        for (int i = 1 ; i < readers.size() ; i++)
            assert_always(readers[0]->clockDomain == readers[i]->clockDomain, "Synchronous port is read from two different clock domains");

        // Deal with cross-domain synchronous connections.  Don't worry about cross-domain connections
        // to wired ports with delay = 1, because they are already dealt with in PortStorage::addPort().
        if (getClockDomain() != connectedTo->getClockDomain())
        {
            if ((delay == 1) && (connectedTo->connection != PORT_SYNCHRONOUS))
            {
                if (connectedTo->connection != PORT_WIRED)
                    connection = PORT_SLOWQ;
            }
            else
                patchRegister();
        }
    }
    else if (!verilog_rd && !verilog_wr && !readOnly)
    {
        // Verify that all readers and writers are in compatible clock domains
        ClockDomain *writer = writers.size() ? writers[0]->clockDomain : parent->getClockDomain();
        for (int i = 0 ; i < readers.size() ; i++)
            assert_always(writer->compatible(readers[i]->clockDomain), "Port is read and written by incompatible clock domains");
    }

    if (connection == PORT_CONNECTED)
    {
        // Push all writers to the terminal port and verify that they are in the same clock
        // domain (only applies to InOut ports)
        for (int i = 0 ; i < writers.size() ; i++)
        {
            connectedTo->writers.push_back(writers[i]);
            assert_always(writers[i]->clockDomain == connectedTo->writers[0]->clockDomain, 
                "InOut net is written from two different clock domains");
        }

        // Push all readers to the terminal port and verify that they are in compatible clock domains
        ClockDomain *writer = connectedTo->verilog_wr ? NULL : connectedTo->getClockDomain();
        for (int i = 0 ; i < readers.size() ; i++)
        {
            connectedTo->readers.push_back(readers[i]);
            assert_always(!writer || writer->compatible(readers[i]->clockDomain),
                "Port net is read and written by incompatible clock domains");
        }
        connectedTo->verilog_rd |= verilog_rd;

        // Push all triggers to the terminal port
        for (int i = 0 ; i < triggers.size() ; i++)
            connectedTo->triggers.push(triggers[i]);

    }
}

////////////////////////////////////////////////////////////////////////////////
//
// resolveRegister()
//
////////////////////////////////////////////////////////////////////////////////
void PortWrapper::resolveRegister ()
{
    mark = 0;
    if (!connectedTo->mark)
        patchRegister(); // Break the cycle
    else if (connectedTo->connection == PORT_SYNCHRONOUS)
    {
        // Point directly to the source port with the appropriate delay
        connectedTo->resolveRegister();
        assert_always(delay + connectedTo->delay < CASCADE_MAX_PORT_DELAY, "Maximum port delay exceeded");
        delay += connectedTo->delay;
        connectedTo = connectedTo->connectedTo;
    }
    mark = 1;
}

////////////////////////////////////////////////////////////////////////////////
//
// patchRegister()
//
////////////////////////////////////////////////////////////////////////////////
void PortWrapper::patchRegister ()
{
    PortWrapper *temp = new PortWrapper(NULL, size, PORT_TEMP);
    if (connectedTo->connection == PORT_SYNCHRONOUS)
        temp->connection = PORT_PATCHED;
    else
    {
        CascadeValidate(delay > 1, "Patching register with delay <= 1");
        delay--;
        if (connectedTo->connection == PORT_WIRED)
            temp->connection = PORT_SYNCHRONOUS;
        else
            temp->connection = PORT_SLOWQ;
        temp->delay = 1;
    }
    temp->connectedTo = connectedTo;
    temp->mark = 1;
    connectedTo = temp;
    patched = 1;
}

//////////////////////////////////////////////////////////////////
//
// resolveNets()
//
//////////////////////////////////////////////////////////////////
void PortWrapper::resolveNetlists ()
{
    logInfo("Resolving port netlists...\n");

    // First pass: resolve connections, readers and writers
    for (PortWrapper *w = g_ports.first() ; w ; w = w->next)
    {
        if (w->isFifo())
            w->resolveFifo();
        else
            w->resolveNet();
    }

    // Second pass: sort ports and initialize update graph
    for (PortList::Remover it(g_ports) ; it ; it++)
    {
        PortWrapper *w = *it;
        bool isFifo = w->isFifo();
        if (isFifo)
        {
            if (w->connectedTo)
            {
                g_connectedPorts.addPort(w);
                continue;
            }
        }
        else
        {
#ifdef _DEBUG
            w->port->isReadOnly = w->readOnly;
#endif
            // Handle constants immediately
            if (w->connection == PORT_CONSTANT)
            {
                byte *data = (byte *) w->constant->data();
                w->port->value = data;
#ifdef _DEBUG
                w->port->hasValidFlag = false;
#endif
                // Check for stuck triggers
                if (w->triggers.size())
                {
                    byte val = 0;
                    for (int i = 0 ; i < w->size ; i++)
                        val |= data[i];
                    for (int i = 0 ; i < w->triggers.size() ; i++)
                    {
                        if ((val && !w->triggers[i].activeLow) || (!val && w->triggers[i].activeLow))
                        {
                            w->getClockDomain()->addStuckTrigger(w);
                            break;
                        }
                    }
                }
                continue;
            }

            // Handle wired ports immediately
            if (w->connection == PORT_WIRED)
            {
                w->port->value = (byte *) it->wire;
#ifdef _DEBUG
                w->port->hasValidFlag = false;
#endif
                // Check for wired triggers
                if (w->triggers.size())
                    w->getClockDomain()->addPort(w);
            }

            // Set aside connected ports
            if (w->connection == PORT_CONNECTED)
            {
                g_connectedPorts.addPort(w);
                continue;
            }

            // Resolve synchronous connections
            if (w->connection == PORT_SYNCHRONOUS)
                w->resolveRegister();

            // Use port readers/writers to create strong edges in the update graph
            if (w->delay == 0)
            {
                for (int i = 0 ; i < w->readers.size() ; i++)
                {
                    for (int j = 0 ; j < w->writers.size() ; j++)
                    {
                        if ((w->readers[i]->clockDomain == w->writers[j]->clockDomain) &&
                            (w->readers[i] != w->writers[j]))
                            w->writers[j]->addStrongEdge(w->readers[i], w);
                    }
                }
            }

            // If this is potentially a fake register connection, then add weak edges
            // to the update graph
            if ((w->direction <= PORT_REGISTER) && (w->delay == 1) && !w->isD && !w->verilog_rd && 
                !w->connectedTo->nofake && (w->connectedTo->type == PORT_NORMAL) && (w->connectedTo->connection == PORT_UNCONNECTED))
            {
                for (int i = 0 ; i < w->readers.size() ; i++)
                {
                    for (int j = 0 ; j < w->connectedTo->writers.size() ; j++)
                    {
                        if ((w->connectedTo->writers[j]->clockDomain == w->readers[i]->clockDomain) &&
                            (w->connectedTo->writers[j] != w->readers[i]))
                            w->readers[i]->addWeakEdge(w->connectedTo->writers[j], w->size);
                    }
                }

            }
        }

        if (isFifo || (w->connection != PORT_WIRED))
        {
            w->getClockDomain()->addPort(w);
            if (w->patched)
                w->getClockDomain()->addPort(w->connectedTo);
        }
    }

    // Need to reset g_ports here because patching might have added some PORT_TEMP wrappers
    g_ports.reset();
}

////////////////////////////////////////////////////////////////////////////////
//
// finalizeConnectedPorts()
//
////////////////////////////////////////////////////////////////////////////////
void PortWrapper::finalizeConnectedPorts ()
{
    for (PortWrapper *w = g_connectedPorts.first() ; w ; w = w->next)
    {
        Port<byte> *port = w->port;
        Port<byte> *source = w->connectedTo->port;
        CascadeValidate(source->value, "Null storage pointer");
        port->value = source->value;
#ifdef _DEBUG
        if (w->isFifo())
        {
            w->fifo->source = !w->producer && (!w->connection & FIFO_NOREADER);
            w->fifo->sink = false;
        }
        else
        {
            port->hasValidFlag = source->hasValidFlag;
            port->isReadOnly = w->readOnly;
            port->validValue = source->validValue;
        }
#endif
    }
    g_connectedPorts.reset();
}

/////////////////////////////////////////////////////////////////
//
// getClockDomain()
//
/////////////////////////////////////////////////////////////////
ClockDomain *PortWrapper::getClockDomain () const
{
    // For ports with a synchronous connection or bound to a verilog port,
    // the reader determines the clock domain.  Otherwise, the writer determines
    // the clock domain.  In the absence of a reader/writer, the parent
    // component determines the clock domain.
    const UpdateVector &updates = (verilog_wr || delay) ? readers : writers;
    return updates.size() ? updates[0]->clockDomain : parent->getClockDomain();
}

//////////////////////////////////////////////////////////////////
//
// getName()
//
//////////////////////////////////////////////////////////////////
strbuff PortWrapper::getName () const
{
    // FIFO ports get reparented, so we need to do the full name lookup.
    if (isFifo())
        return PortName::getPortName(port);

    // TEMP ports don't actually exist
    if (direction == PORT_TEMP)
        return strbuff("[TEMP] %s", *connectedTo->getName());

    return PortName::getComponentPortName(parent, port);
}

////////////////////////////////////////////////////////////////////////////////
//
// getAssertionContext()
//
////////////////////////////////////////////////////////////////////////////////
string PortWrapper::getAssertionContext () const
{
    return *getName();
}

//////////////////////////////////////////////////////////////////
//
// getPortName
//
//////////////////////////////////////////////////////////////////

strbuff PortName::getPortName (const void *address)
{
    strbuff s;
    CascadeValidate(formatPortName(s, Sim::topLevelComponents, address),
        "Could not find port at address %p\n"
        "    (This can be caused by an invalid port/interface pointer cast)", address);
    return s;
}

strbuff PortName::getComponentPortName (const Component *c, const void *address)
{
    strbuff s;
    CascadeValidate(formatPortName(s, c, address, false),
        "Could not find port at address %p in %s\n"
        "    (This can be caused by an invalid port/interface pointer cast)", 
        address, *c->getName());
    return s;
}

// Find the port and create its name.  Returns true if successful.
bool PortName::formatPortName (strbuff &s, const Component *c, const void *address, bool search)
{
    for ( ; c ; c = c->nextComponent)
    {
        InterfaceDescriptor *descriptor = c->getInterfaceDescriptor();

        // We can eliminate this component as a possible parent if:
        //  (i)  the component contains no port/interface Arrays
        //  (ii) the port address does not fall within the component
        if (descriptor->containsArray() || (BDIFF(address, c) < descriptor->maxOffset()))
        {
            // Try to find the port in this interface
            PortIterator it(PortSet::Everything, descriptor, c);
            for ( ; it ; it++)
            {
                if (it.address() == address)
                {
                    // Found it!
                    c->formatName(s);
                    it.formatName(s);
                    return true;
                }
            }
        }

        if (!search)
            return false;

        if (formatPortName(s, c->childComponent, address))
            return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// cleanup()
//
////////////////////////////////////////////////////////////////////////////////
void PortWrapper::cleanup ()
{
    g_ports.reset();
    g_connectedPorts.reset();
    s_arrayIndex = -1;
}


END_NAMESPACE_CASCADE

