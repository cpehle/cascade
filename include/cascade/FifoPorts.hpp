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
// FifoPorts.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 03/13/2007
//
// Implement a FIFO connection between components
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_FifoPorts_hpp_
#define Cascade_FifoPorts_hpp_

#include "Ports.hpp"
#include "Component.hpp"

class Archive;
class Sim;

BEGIN_NAMESPACE_CASCADE

#define MAX_INITIAL_FIFO_SIZE 16

// Sneak this into PortWrapper::connection
enum FifoType
{
    FIFO_NORMAL,
    FIFO_NOREADER,
    FIFO_NOWRITER
};

/////////////////////////////////////////////////////////////////
//
// Fifo struct.  Allocated by clock domains in the same
// manner as port storage.  Fifo storage is really just a
// specialized type of port storage with a different set 
// of accessors.
//
/////////////////////////////////////////////////////////////////
struct GenericFifo
{
    // Deferred construction
    void initialize (unsigned _dataSize, 
        unsigned _size, 
        unsigned _delay, 
        bool _noflow,
        intptr_t target, 
        ClockDomain *producer,
        ClockDomain *consumer);

    // Simulation
    void reset ();
    void archive (Archive &ar);

    uint16 head;       // Head byte offset seen by consumer
    uint16 tail;       // Tail byte offset seen by producer
    uint16 freeCount;  // Count of free fifo entries seen by producer
    uint16 fullCount;  // Non-trigger: Count of used fifo entries seen by consumer
    uint16 size;       // Size in bytes of fifo storage.  0 for a bit-bucket or a combinational trigger
    uint16 dataSize;   // Size in bytes of a single fifo entry.
    uint16 minFree;    // Used to compute fifo high water mark

    // Optimization: can replace the test (!noflow && delay) with (popDelay > 0)
    union
    {
        int16 popDelay;
        struct
        {
            uint16 delay : 15; // Synchronous delay of fifo in consumer clock cycles
            uint16 noflow : 1; // Implicit flow control is disabled
        };
    };

    // Make sure the size of this structure is a multiple of 64 to avoid
    // alignment problems in 32-bit builds (Fifo::data needs to appear
    // immediately following GenericFifo with no gap).
    union
    {
        uint32 unused[4];
        struct  
        {
            // Note: As an optimization, combinational triggers are stored with size = 0 and
            //       the trigger pointer stored directly in target (without bit 0 set).
            intptr_t target;          // ITrigger * (bit 0 set) or Component * (bit 0 clear)
            ClockDomain *producerClockDomain; // Producer update function clock domain
            ClockDomain *consumerClockDomain; // Consumer update function clock domain    
       };
    };
};

#define GENERIC_FIFO_DATA(fifo) ((byte*)(fifo+1))

template <typename T>
struct Fifo : public GenericFifo
{
    // Data array with Fifo contents
    T data[1];
};

// Bitbucket target
struct FifoBitbucketTarget : public ITrigger<byte>
{
    virtual void trigger (const byte &) {}
};

extern FifoBitbucketTarget fifoBitbucketTarget;

// Various places in the code assume that there is no gap between 
// GenericFifo and the data field.  Make sure that structure alignment
// doesn't violate this for large types.  gcc doesn't want these assertions
// to be static, so make them dynamic in GenericFifo::initialize()
#ifdef _MSC_VER
STATIC_ASSERT(offsetof(Fifo<byte>,   data) == sizeof(GenericFifo));
STATIC_ASSERT(offsetof(Fifo<uint64>, data) == sizeof(GenericFifo));
STATIC_ASSERT(offsetof(Fifo<u128>,   data) == sizeof(GenericFifo));
#endif

#define CASCADE_MAX_FIFO_SIZE      65535
#define CASCADE_MAX_FIFO_DELAY     65535

/////////////////////////////////////////////////////////////////
//
// Generic fifo port
//
/////////////////////////////////////////////////////////////////

// Validation macros

#ifdef _DEBUG

#define VALIDATE_FIFO_READ assert(source, fifo->size ? \
    "Cannot read from fifo that has been connected to" : \
    "Cannot read from bit-bucket fifo");
#define VALIDATE_FIFO_WRITE assert(sink, fifo->size ? \
    "Cannot write to fifo that has been connected" : \
    "Cannot write to fifo that has been wired to zero");
#define VALIDATE_FIFO_FLOW assert(!fifo->noflow, \
    "Cannot access full() or freeCount() on fifo with flow control disabled");

#else

#define VALIDATE_FIFO_READ
#define VALIDATE_FIFO_WRITE
#define VALIDATE_FIFO_FLOW

#endif

// FifoPort
template <typename T>
class FifoPort
{
    DECLARE_NOCOPY(FifoPort);

    typedef typename PortValueType<T>::value_t value_t;

    friend class ::Sim;
    friend class ClockDomain;
    friend class PortWrapper;
    friend class UpdateConstructor;
    friend class InterfaceDescriptor;
    friend struct Waves;
    friend class WavesFifo;
    friend class PortStorage;

public:

    //----------------------------------
    // Construction
    //----------------------------------

    FifoPort (PortDirection dir, unsigned size, unsigned delay)
#ifdef _DEBUG
        : source(false), sink(false)
#endif
    {
        // Maximum port size
        STATIC_ASSERT(sizeof(value_t) * 8 < CASCADE_MAX_PORT_SIZE);

        wrapper = new PortWrapper(this, sizeof(T), dir);
        Hierarchy::addPort(dir, this, getPortInfo<T>());
        Sim::updateChecksum("Fifo", dir);
        wrapper->fifoSize = 0;
        if (size)
            setSize(size);
        setDelay(delay);
        wrapper->connection = FIFO_NORMAL;
        wrapper->fifoDisableFlowControl = false;
    }

    void setSize (int size)
    {
        assert_always(Sim::state == Sim::SimConstruct);
        assert_always(unsigned(size) <= CASCADE_MAX_FIFO_SIZE, "Fifo size (%d) exceeds maximum of %d", size, CASCADE_MAX_FIFO_SIZE);
        wrapper->fifoSize = size;
    }

    void setDelay (int delay)
    {
        assert_always(Sim::state == Sim::SimConstruct);
        assert_always(unsigned(delay) <= CASCADE_MAX_FIFO_DELAY, "Fifo delay (%d) exceeds maximum of %d", delay, CASCADE_MAX_FIFO_DELAY);
        wrapper->delay = delay;
    }

    // Mark a FIFO as unread - push data directly into the bit bucket
    void sendToBitBucket ()
    {
        assert_always(Sim::state == Sim::SimConstruct);
        assert_always(!wrapper->producer, "Cannot send fifo to bit bucket because it has been connected to");
        assert_always(!wrapper->readers.size(), "Cannot send fifo to bit bucket because it is read by %s", *wrapper->readers[0]->getName());
        assert_always(!wrapper->triggers.size(), "Cannot send fifo to bit bucket because it activates a trigger");
        wrapper->connection |= FIFO_NOREADER;
    }

    // Mark a FIFO as unwritten - it will always be empty
    void wireToZero ()
    {
        assert_always(Sim::state == Sim::SimConstruct);
        assert_always(!wrapper->connectedTo, "Cannot wire fifo to zero because it has already been connected");
        assert_always(!wrapper->writers.size(), "Cannot wire fifo to zero because it is written by %s", *wrapper->writers[0]->getName());
        wrapper->connection |= FIFO_NOWRITER;
    }

    // Disable implicit flow control, which has the following effects:
    // - size does not need to be >= 2*delay to avoid bubbles
    //   (but it still needs to be >= delay)
    // - there should be no calls to full() or freeCount() (since the simulation 
    //   should be explicitly managing flow control)
    void disableFlowControl ()
    {
        assert_always(Sim::state == Sim::SimConstruct);
        wrapper->fifoDisableFlowControl = 1;
    }

    // Associate a trigger action with the FIFO (at most one trigger action can be
    // associated with a single FIFO).
    void setTrigger (ITrigger<value_t> *trigger)
    {
        assert_always(Sim::state == Sim::SimConstruct);
        assert_always(!wrapper->producer, "Cannot set fifo trigger because it has been connected to");
        assert_always(!(wrapper->connection & FIFO_NOREADER), "Cannot set fifo trigger because it has been sent to the bit bucket");
        wrapper->addTrigger(Trigger(((intptr_t) trigger) | TRIGGER_ITRIGGER, false)); 
    }

    //----------------------------------
    // Connections
    //----------------------------------

    FifoPort<T> &operator<< (FifoPort<T> &rhs)
    {
        assert_always(Sim::state == Sim::SimConstruct);
        wrapper->connect(rhs.wrapper, 0);
        return rhs;
    }
    FifoPort<T> &operator<= (FifoPort<T> &rhs)
    {
        assert_always(Sim::state == Sim::SimConstruct);
        wrapper->connect(rhs.wrapper, 1);
        return rhs;
    }

    //----------------------------------
    // Accessors
    //----------------------------------

    inline bool empty () const
    {
        VALIDATE_FIFO_READ;
        return !fifo->fullCount;
    }

    inline bool full () const
    {
        VALIDATE_FIFO_WRITE;
        VALIDATE_FIFO_FLOW;
        return !fifo->freeCount;
    }

    inline int freeCount () const
    {
        VALIDATE_FIFO_WRITE;
        VALIDATE_FIFO_FLOW;
        return fifo->freeCount;
    }

    inline int popCount () const
    {
        VALIDATE_FIFO_READ;
        return fifo->fullCount;
    }

    void push (const value_t &data)
    {
        VALIDATE_FIFO_WRITE;
        if (fifo->size)
        {
            assert(fifo->freeCount);
            assert(fifo->tail < fifo->size * fifo->dataSize);
            fifo->freeCount--;
            if (fifo->freeCount < fifo->minFree)
                fifo->minFree = fifo->freeCount;
            *(value_t*)(((byte*)fifo->data) + fifo->tail) = data;
            if (!fifo->tail)
                fifo->tail = fifo->size;
            fifo->tail -= sizeof(T);
            if (fifo->delay)
                fifo->consumerClockDomain->schedulePush((GenericFifo *) fifo);
            else if (!fifo->fullCount++)
                ((Component *) fifo->target)->activate();
        }
        else
            ((ITrigger<value_t> *) fifo->target)->trigger(data);
    }

    const value_t &pop ()
    {
        VALIDATE_FIFO_READ;
        assert(fifo->fullCount);
        fifo->fullCount--;
        if (fifo->popDelay > 0)
            fifo->consumerClockDomain->schedulePop((GenericFifo *) fifo);
        else
            fifo->freeCount++;
        value_t *ret = (value_t *) (((byte *) fifo->data) + fifo->head);
        if (!fifo->head)
            fifo->head = fifo->size;
        fifo->head -= sizeof(value_t);
        return *ret;
    }

    inline const value_t &peek () const
    {
        VALIDATE_FIFO_READ;
        assert(fifo->fullCount);
        return *(const value_t *) (((const byte *) fifo->data) + fifo->head);
    }

    inline int highWaterMark () const
    {
        return fifo->size ? (fifo->size / fifo->dataSize) - fifo->minFree : 0;
    }

protected:
    union
    {
        Fifo<value_t> *fifo;  // points to the Fifo used by this port
        PortWrapper *wrapper; // used during initialization
    };

#ifdef _DEBUG
    bool source; // True if this fifo is not connected
    bool sink;   // True if nothing is connected to this fifo and it's not a trigger
#endif

public:

    // Return the name of this port
    strbuff getName () const
    {
        return PortName::getPortName(this);
    }
    string getAssertionContext () const
    {
        return *getName();
    }
};

END_NAMESPACE_CASCADE

/////////////////////////////////////////////////////////////////
//
// Specific fifo ports
//
/////////////////////////////////////////////////////////////////

// Input fifo
template <typename T>
class FifoInput : public Cascade::FifoPort<T>
{
public:
    FifoInput<T> (unsigned size = 0, unsigned delay = 0) 
        : Cascade::FifoPort<T>(Cascade::PORT_INFIFO, size, delay) 
    {
    }

    // Support for arrays of ports
    static bool addPortArrayEntry (const void *arrayAddress)
    {
        return Cascade::Hierarchy::addPortArray(Cascade::PORT_INFIFO, arrayAddress, Cascade::getPortInfo<T>(), sizeof(Cascade::FifoPort<byte>));
    }
};

// Output fifo
template <typename T>
class FifoOutput : public Cascade::FifoPort<T>
{
public:
    FifoOutput<T> (unsigned size = 0, unsigned delay = 0) 
        : Cascade::FifoPort<T>(Cascade::PORT_OUTFIFO, size, delay) 
    {
    }

    // Support for arrays of ports
    static bool addPortArrayEntry (const void *arrayAddress)
    {
        return Cascade::Hierarchy::addPortArray(Cascade::PORT_OUTFIFO, arrayAddress, Cascade::getPortInfo<T>(), sizeof(Cascade::FifoPort<byte>));
    }
};

#endif

