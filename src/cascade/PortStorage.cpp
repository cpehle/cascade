/*
Copyright 2010, D. E. Shaw Research.
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

////////////////////////////////////////////////////////////////////////////////
//
// PortStorage.cpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 11/29/2010
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SimDefs.hpp"
#include "SimGlobals.hpp"
#include "BitVector.hpp"
#include "Params.hpp"
#include "Ports.hpp"
#include "FifoPorts.hpp"
#include "PortStorage.hpp"
#include "ClockDomain.hpp"
#include <descore/MapIterators.hpp>

BEGIN_NAMESPACE_CASCADE

////////////////////////////////////////////////////////////////////////////////
//
// PortStorage()
//
////////////////////////////////////////////////////////////////////////////////
PortStorage::PortStorage () : 
m_fifoData(NULL),
m_portData(NULL),
m_pulsePorts(NULL),
m_delayOffset(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////
//
// ~PortStorage()
//
////////////////////////////////////////////////////////////////////////////////
PortStorage::~PortStorage ()
{
    delete[] m_fifoData;
    delete[] m_portData;
    delete[] m_delayOffset;
}

////////////////////////////////////////////////////////////////////////////////
//
// addPort()
//
////////////////////////////////////////////////////////////////////////////////
void PortStorage::addPort (PortWrapper *port)
{
    if (port->isFifo())
        m_fifoPorts.addPort(port);
    else if (port->connection == PORT_SLOWQ)
    {
        CascadeValidate(port->delay == 1, "Slow register with delay > 1");
        port->delay = 0;
        m_terminalPorts.addPort(port);
        ValueCopy copy = { (byte *) port, (byte *) port->connectedTo, port->size };
        m_slowRegs.push(copy);
        port->type = PORT_LATCH;
    }
    else if (port->connection == PORT_SYNCHRONOUS)
    {
        if (port->connectedTo->connection == PORT_WIRED)
            port->delay--;
        if (port->delay)
            m_synchronousPorts.addPort(port);
        else
        {
            m_terminalPorts.addPort(port);
            CascadeValidate(port->connectedTo->connection == PORT_WIRED, "Synchronous port with zero delay");
            ValueCopy copy = { (byte *) port, (byte *) port->connectedTo->wire, port->size };
            m_wiredRegs.push(copy);
        }
    }
    else
    {
        m_terminalPorts.addPort(port);
        if (port->connection == PORT_PATCHED)
        {
            ValueCopy copy = { (byte *) port, (byte *) port->connectedTo, port->size };
            m_patchedRegs.push(copy);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// initPorts()
//
////////////////////////////////////////////////////////////////////////////////
void PortStorage::initPorts (ClockDomain *domain)
{
    initFifos(domain);

    // Finalize fake registers and compute max delay for each terminal port
    m_maxDelay = 0;
    for (PortWrapper *p = m_synchronousPorts.first() ; p ; p = p->next)
    {
        if ((p->delay == 1) && (p->connectedTo->type == PORT_NORMAL) && !p->verilog_rd && 
            !p->connectedTo->nofake && (p->connectedTo->connection == PORT_UNCONNECTED) && !p->isD)
        {
            // If there is no read after write, then this is a fake register
            int lastReader = 0;
            int firstWriter = 0x7fffffff;
            for (int i = 0 ; i < p->readers.size() ; i++)
            {
                if (p->readers[i]->index > lastReader)
                    lastReader  = p->readers[i]->index;
            }
            for (int i = 0 ; i < p->connectedTo->writers.size() ; i++)
            {
                if (p->connectedTo->writers[i]->index < firstWriter)
                    firstWriter = p->connectedTo->writers[i]->index;
            }
            if (firstWriter > lastReader)
            {
                p->delay = 0;
#ifdef _DEBUG
                p->port->validValue = VALUE_VALID_PREV;
#endif
            }
        }
        if (p->connectedTo->delay < p->delay)
            p->connectedTo->delay = p->delay;
        if (m_maxDelay < p->delay)
            m_maxDelay = p->delay;
    }

    // Sort terminal ports by (P/NL, delay, N/L, size)
    PortMap sortedTerminalPorts;
    PortMap sortedPulsePorts;
    for (PortList::Remover it = m_terminalPorts ; it ; it++)
    {
        PortWrapper *p = *it;
        if (p->type == PORT_PULSE)
            sortedPulsePorts[(~p->delay << 17) | p->size].addPort(p);
        else if (p->type == PORT_LATCH)
            sortedTerminalPorts[(p->delay << 17) | 0x10000 | p->size].addPort(p);
        else
            sortedTerminalPorts[(p->delay << 17) | p->size].addPort(p);
    }

    // Compute storage requirements for terminal ports
    int *ndepthOffset = new int[m_maxDelay + 1];
    int *pdepthOffset = new int[m_maxDelay + 1];
    int *nsize = new int[m_maxDelay + 1];
    memset(ndepthOffset, 0xff, (m_maxDelay + 1) * sizeof(int));
    memset(pdepthOffset, 0xff, (m_maxDelay + 1) * sizeof(int));
    memset(nsize, 0, (m_maxDelay + 1) * sizeof(int));
    int normalBytes = allocateValues(sortedTerminalPorts, ndepthOffset, nsize);
    m_pulsePortBytes = allocateValues(sortedPulsePorts, pdepthOffset);
    int offset = normalBytes;
    for (int i = m_maxDelay ; i >= 0 ; i--)
    {
        if (ndepthOffset[i] == -1)
            ndepthOffset[i] = offset;
        else
            offset = ndepthOffset[i];
    }
    offset = m_pulsePortBytes;
    for (int i = 0 ; i <= m_maxDelay ; i++)
    {
        if (pdepthOffset[i] == -1)
            pdepthOffset[i] = offset;
        else
            offset = pdepthOffset[i];
    }

    // Compute storage requirements and offsets for non-terminal ports
    int *portOffset = new int[m_maxDelay + 1];
    int *portBytes = new int[m_maxDelay + 1];
    m_delayOffset = new int[m_maxDelay + 1];
    portBytes[0] = normalBytes + m_pulsePortBytes;
    portOffset[0] = 0;
    m_delayOffset[0] = 0;
    for (int i = 1 ; i <= m_maxDelay ; i++)
    {
        portOffset[i] = portOffset[i-1] + portBytes[i-1];
        portBytes[i] = portBytes[i-1];
        portBytes[i] -= ndepthOffset[i] - ndepthOffset[i-1];
        portBytes[i] -= ((i == 1) ? m_pulsePortBytes : pdepthOffset[i-2]) - pdepthOffset[i-1];
        m_delayOffset[i] = portOffset[i] - ndepthOffset[i];
    }
    m_portBytes = portOffset[m_maxDelay] + portBytes[m_maxDelay];
    Sim::stats.numPortBytes += m_portBytes;
    Sim::stats.numRegisterBytes += m_portBytes - portBytes[0];

    // Allocate values and initialize nports (for port invalidation)
    m_portData = new byte[m_portBytes];
    m_pulsePorts = m_portData + normalBytes;
    allocateValues(sortedTerminalPorts, ndepthOffset, nsize, m_portData);
    allocateValues(sortedPulsePorts, pdepthOffset, NULL, m_pulsePorts);

    // Initialize nports (for port invalidation)
#ifdef _DEBUG
    for (int i = 0 ; i <= m_maxDelay ; i++)
    {
        Region r = { m_portData + ndepthOffset[i], nsize[i] };
        if (nsize[i])
            m_nports.push(r);
    }
#endif

    // Initial memcpy so that synchronous region block headers are valid
    for (int i = 0 ; i < m_maxDelay ; i++)
    {
        memcpy(m_portData + portOffset[i+1], 
            m_portData + portOffset[i] + ndepthOffset[i+1] - ndepthOffset[i], 
            portBytes[i+1]);
    }

    // Set synchronous port value pointers and mark the values to distinguish
    // between explicit and implicit registers
    for (PortWrapper *p = m_synchronousPorts.first() ; p ; p = p->next)
    {
        byte *value = (p->connectedTo->direction == PORT_TEMP) ? p->connectedTo->value : p->connectedTo->port->value;
        p->port->value = value + portOffset[p->delay] - ndepthOffset[p->delay];

        // Fake registers
        if (!p->delay)
        {
            Sim::stats.numFakeRegisterBytes += p->size;
#ifdef _DEBUG
            p->port->validValue = VALUE_VALID_PREV;
#endif
        }
    }

    // Initialize memcpys
    m_regCopies.resize(m_maxDelay);
    for (int i = 0 ; i < m_maxDelay ; i++)
    {
        m_regCopies[i].dst = m_portData + portOffset[m_maxDelay - i];
        m_regCopies[i].src = m_portData + portOffset[m_maxDelay - i - 1] +
            ndepthOffset[m_maxDelay - i] - ndepthOffset[m_maxDelay - i - 1];
        m_regCopies[i].size = portBytes[m_maxDelay - i];
    }

    // Validate and clear P ports
    for (ValueIterator it(m_pulsePorts, m_pulsePortBytes) ; it ; it++)
    {
        memset(it.value(), 0, it.size());
#ifdef _DEBUG
        it.flags() = VALUE_VALID;
#endif
    }

    // cleanup
    delete[] ndepthOffset;
    delete[] pdepthOffset;
    delete[] nsize;
    delete[] portOffset;
    delete[] portBytes;
}

////////////////////////////////////////////////////////////////////////////////
//
// allocateValues()
//
////////////////////////////////////////////////////////////////////////////////
int PortStorage::allocateValues (const PortMap &ports, int *depthOffset, int *nsize /* = NULL */, byte *storage /* = NULL */)
{
    int offset = 0;        // Current offset into storage (return at end)
    uint16 size = 0;       // Port size in current block; size = 0 forces new block.
    uint16 delay = 0xffff; // Port delay in current block; delay = 0xffff forces new block.
    uint16 type = 0xffff;  // Port type in current block
    uint16 _count = 0;
    uint16 *count = &_count; // Pointer to count field in current block or to _count
    int flagsSize ATTRIBUTE_UNUSED = 0;
    int regionOffset = -1;
    for_map_values (const PortList &portList, ports)
    {
        for (PortWrapper *p = portList.first() ; p ; p = p->next)
        {
            // Check for a new block
            bool newRegion = (delay != p->delay) || (type != p->type);
            if (newRegion || (size != p->size))
            {
                offset = (offset + 3) & ~3;
                if (delay != p->delay)
                    depthOffset[p->delay] = offset;

                // Check for the start/end of an n region
                if (newRegion)
                {
                    if (type == PORT_NORMAL)
                    {
                        CascadeValidate(nsize, "nsize is NULL but type is not PORT_PULSE");
                        nsize[delay] = offset - regionOffset;
                    }
                    if (p->type == PORT_NORMAL)
                        regionOffset = offset;
                }

                size = p->size;
                delay = p->delay;
                type = p->type;
                flagsSize = (size < 4) ? size : 4;
                if (storage)
                {
                    *(uint16*)(storage + offset) = size;
                    count = (uint16*)(storage + offset + 2);
                }
                *count = 0;
                offset += 4;
            }

            // Valid flag (debug only)
#ifdef _DEBUG
            if (storage)
                memset(storage + offset, 0, flagsSize);
            offset += flagsSize;
#endif

            // Value
            if (storage)
            {
                memset(storage + offset, 0xcd, size);
                if (p->port)
                    p->port->value = storage + offset;
                else
                    p->value = storage + offset;
            }
            offset += size;

            // Count is a 16-bit field; if it wraps around then start a new block
            (*count)++;
            if (!(*count))
                size = 0;
        }
    }

    if (type == PORT_NORMAL)
        nsize[delay] = offset - regionOffset;

    // Return the number of bytes used, rounded to a multiple of 4
    return (offset + 3) & ~3;
}

////////////////////////////////////////////////////////////////////////////////
//
// initFifos()
//
////////////////////////////////////////////////////////////////////////////////
void PortStorage::initFifos (ClockDomain *domain)
{
    // Allocate fifo storage
    m_fifoDataSize = 0;
    m_numFifos = 0;
    for (PortWrapper *p = m_fifoPorts.first() ; p ; p = p->next, m_numFifos++)
    {
        if (p->connection != FIFO_NORMAL || (p->triggers.size() && !p->delay))
            p->fifoSize = 0;
        if (p->connection != FIFO_NORMAL)
            p->delay = 0;
        m_fifoDataSize += sizeof(GenericFifo) + p->size * p->fifoSize;
        m_fifoDataSize = (m_fifoDataSize + 3) & ~3;
    }

    m_fifoData = new byte[m_fifoDataSize];
    memset(m_fifoData, 0, m_fifoDataSize);
    Sim::stats.numFifoBytes += m_fifoDataSize;

    // Initialize the fifos
    int offset = 0;
    for (PortWrapper *p = m_fifoPorts.first() ; p ; p = p->next)
    {
        // Get the current fifo pointer and update offset
        GenericFifo *currFifo = (GenericFifo *) (m_fifoData + offset);
        offset += sizeof(GenericFifo) + p->size * p->fifoSize;
        offset = (offset + 3) & ~3;
        p->fifo->fifo = (Fifo<byte> *) currFifo;

        // Resolve the target
        intptr_t target = (intptr_t) &fifoBitbucketTarget;
        if (p->triggers.size())
        {
            target = p->triggers[0].target;
            CascadeValidate(target & TRIGGER_ITRIGGER, "Fifo %s has invalid trigger", *p->getName());
            if (!p->delay)
                target -= TRIGGER_ITRIGGER;
        }
        else if (p->readers.size())
        {
            CascadeValidate(p->readers.size() == 1, "Fifo %s has multiple readers", *p->getName());
            target = (intptr_t) p->readers[0]->component;
        }

        // Get the producer clock domain
        ClockDomain *producer = NULL;
        if (p->writers.size())
        {
            CascadeValidate(p->writers.size() == 1, "Fifo %s has multiple writers", *p->getName());
            producer = p->writers[0]->clockDomain;
        }

        // Initialize the fifo
        currFifo->initialize(p->size, p->size * p->fifoSize, p->delay, 
            p->fifoDisableFlowControl, target, producer, domain);

        // Check to see if we need to increase the sync depth
        if ((int) p->delay > domain->m_syncDepth)
            domain->m_syncDepth = p->delay;

#ifdef _DEBUG
        p->fifo->source = !p->producer && !(p->connection & FIFO_NOREADER) && !p->triggers.size();
        p->fifo->sink = !(p->connection & FIFO_NOWRITER);
#endif
    }
    CascadeValidate(offset == m_fifoDataSize, "Fifo data size mismatch");
}

////////////////////////////////////////////////////////////////////////////////
//
// finalizeCopies()
//
////////////////////////////////////////////////////////////////////////////////
void PortStorage::finalizeCopies (stack<ValueCopy> &copies)
{
    for (int i = 0 ; i < copies.size() ; i++)
    {
        ValueCopy &copy = copies[i];
        PortWrapper *dst = (PortWrapper *) copy.dst;
        if (dst->direction == PORT_TEMP)
            copy.dst = dst->value;
        else
            copy.dst = dst->port->value;
        copy.src = ((PortWrapper *) copy.src)->port->value;
#ifdef _DEBUG
        copy.dst--;
        copy.src--;
        copy.size++;
#endif
    }
}

void PortStorage::finalizeCopies ()
{
    finalizeCopies(m_patchedRegs);
    finalizeCopies(m_slowRegs);

    for (int i = 0 ; i < m_wiredRegs.size() ; i++)
    {
        ValueCopy &copy = m_wiredRegs[i];                               
        PortWrapper *dst = (PortWrapper *) copy.dst;
        if (dst->direction == PORT_TEMP)
            copy.dst = dst->value;
        else
            copy.dst = dst->port->value;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// propagateReset()
//
////////////////////////////////////////////////////////////////////////////////
void PortStorage::propagateReset ()
{
    // Patched registers
    for (int i = 0 ; i < m_patchedRegs.size() ; i++)
        memcpy(m_patchedRegs[i].dst, m_patchedRegs[i].src, m_patchedRegs[i].size);

    // Wired registers
    for (int i = 0 ; i < m_wiredRegs.size() ; i++)
    {
        memcpy(m_wiredRegs[i].dst, m_wiredRegs[i].src, m_wiredRegs[i].size);
#ifdef _DEBUG
        m_wiredRegs[i].dst[-1] = VALUE_VALID;
#endif
    }

    // Slow registers
    for (int i = 0 ; i < m_slowRegs.size() ; i++)
        memcpy(m_slowRegs[i].dst, m_slowRegs[i].src, m_slowRegs[i].size);

    // Memcpys
    for (int i = m_regCopies.size() - 1 ; i >= 0 ; i--)
        memcpy(m_regCopies[i].dst, m_regCopies[i].src, m_regCopies[i].size);
}

////////////////////////////////////////////////////////////////////////////////
//
// preTick()
//
////////////////////////////////////////////////////////////////////////////////
void PortStorage::preTick ()
{
    // Copy into patched temporaries
    for (int i = 0 ; i < m_patchedRegs.size() ; i++)
        memcpy(m_patchedRegs[i].dst, m_patchedRegs[i].src, m_patchedRegs[i].size);
}

////////////////////////////////////////////////////////////////////////////////
//
// tick()
//
////////////////////////////////////////////////////////////////////////////////
void PortStorage::tick ()
{
    // Synchronous register memcpys
    for (int i = 0 ; i < m_regCopies.size() ; i++)
        memcpy(m_regCopies[i].dst, m_regCopies[i].src, m_regCopies[i].size);

    // Wired registers
    for (int i = 0 ; i < m_wiredRegs.size() ; i++)
    {
        memcpy(m_wiredRegs[i].dst, m_wiredRegs[i].src, m_wiredRegs[i].size);
#ifdef _DEBUG
        m_wiredRegs[i].dst[-1] = VALUE_VALID;
#endif
    }

    // Slow registers
    for (int i = 0 ; i < m_slowRegs.size() ; i++)
        memcpy(m_slowRegs[i].dst, m_slowRegs[i].src, m_slowRegs[i].size);
}

////////////////////////////////////////////////////////////////////////////////
//
// postTick()
//
////////////////////////////////////////////////////////////////////////////////
void PortStorage::postTick ()
{
#ifdef _DEBUG
    // Invalidate N ports
    for (int i = 0 ; i < m_nports.size() ; i++)
    {
        for (ValueIterator it(m_nports[i].data, m_nports[i].size) ; it ; it++)
            it.flags() >>= 1;
    }

    // Zero pulse ports one value at a time in debug builds to preserve the port flags
    for (ValueIterator it(m_pulsePorts, m_pulsePortBytes) ; it ; it++)
        memset(it.value(), 0, it.size());
#else
    // Zero pulse ports one block at a time in release builds to preserve the block headers
    byte *start = m_pulsePorts;
    byte *end = m_pulsePorts + m_pulsePortBytes;
    while (start < end)
    {
        // A count of zero actually means 0x10000, so subtract one from the
        // uint16 and then add one back as an integer to convert 0 to 0x10000.
        int size = *(uint16*) start;
        uint16 count_minus_one = (*(uint16*) (start+2)) - 1;
        int len = size * (count_minus_one + 1);
        start += 4;
        memset(start, 0, len);
        start += len;
        start = (byte *) ((((intptr_t) start) + 3) & ~3);
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////
//
// archive()
//
////////////////////////////////////////////////////////////////////////////////
void PortStorage::archive (Archive &ar)
{
    // Archive Values
    for (ValueIterator it(m_portData, m_portBytes) ; it ; it++)
    {
        // Archive flags
#ifdef _DEBUG
        ar | it.flags();
#else
        byte staticFlag = VALUE_VALID | VALUE_VALID_PREV;
        ar | staticFlag;
#endif
        // Archive value
        ar.archiveData(it.value(), it.size());
    }

    // A WavesFifo needs to know how many in-flight pushes there are, so the consumer
    // side of a fifo with delay can set its tail pointer properly.  So if we're loading 
    // then temporarily record the number of in-flight pushes in fullCount, then copy 
    // this information to the WavesFifos, then archive the actual Fifo data.  The first
    // step (here) is to set the full counts to zero, then we'll iterate over
    // m_syncFifoPush in ClockDomain to tally up the proper full counts.
    if (ar.isLoading())
    {
        // Set the full counts to zero
        for (int offset = 0 ; offset < m_fifoDataSize ; )
        {
            GenericFifo *fifo = (GenericFifo *) (m_fifoData + offset);
            offset += sizeof(GenericFifo) + fifo->size;
            offset = (offset + 3) & ~3;
            fifo->fullCount = 0;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// archiveFifos()
//
////////////////////////////////////////////////////////////////////////////////
void PortStorage::archvieFifos (Archive &ar)
{
    int numFifos = m_numFifos;
    ar | numFifos;
    assert_always(numFifos == m_numFifos, "Archive contains invalid number of fifos");
    int expectedDataSize = m_fifoDataSize - m_numFifos * sizeof(GenericFifo);
    int fifoDataSize = expectedDataSize;
    ar | fifoDataSize;
    assert(fifoDataSize == expectedDataSize, "Archive contains invalid fifo data.\n"
        "    Have you changed fifo sizes since the archive was created?");
    for (int offset = 0 ; offset < m_fifoDataSize ; )
    {
        GenericFifo *fifo = (GenericFifo *) (m_fifoData + offset);
        offset += sizeof(GenericFifo) + fifo->size;
        offset = (offset + 3) & ~3;
        fifo->archive(ar);
    }
}

/////////////////////////////////////////////////////////////////
//
// archiveFifoStack()
//
/////////////////////////////////////////////////////////////////
void PortStorage::archiveFifoStack (Archive &ar, stack<GenericFifo *> &v)
{
    if (ar.isLoading())
    {
        int size;
        ar | size;
        v.resize(size);
        for (int i = 0 ; i < size ; i++)
        {
            int offset;
            ar | offset;
            v[i] = (GenericFifo *) (m_fifoData + offset);
        }
    }
    else
    {
        int size = v.size();
        ar | size;
        for (int i = 0 ; i < size ; i++)
        {
            int offset = ((byte *) v[i]) - m_fifoData;
            ar | offset;
        }
    }
}

/////////////////////////////////////////////////////////////////
//
// checkDeadlock()
//
/////////////////////////////////////////////////////////////////
void PortStorage::checkDeadlock ()
{
    for (int offset = 0 ; offset < m_fifoDataSize ; )
    {
        GenericFifo *fifo = (GenericFifo *) (m_fifoData + offset);
        offset += sizeof(GenericFifo) + fifo->size;
        offset = (offset + 3) & ~3;

        if (!(fifo->target & TRIGGER_ITRIGGER) && fifo->size)
        {
            Component *consumer = (Component *) fifo->target;
            if (fifo->fullCount && !consumer->isActive())
                die("Deadlock detected!\n    %s is inactive, but reads a non-empty fifo",
                *consumer->getName());
        }
    }
}

END_NAMESPACE_CASCADE
