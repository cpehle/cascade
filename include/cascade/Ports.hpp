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
// Ports.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_Ports_hpp_
#define Cascade_Ports_hpp_

#include "SimDefs.hpp"
#include "Hierarchy.hpp"
#include "Interface.hpp"
#include "PortTypes.hpp"
#include "Update.hpp"
#include <descore/PointerVector.hpp>
#include "Stack.hpp"
#include "Triggers.hpp"
#include "Constants.hpp"
#include "Wrapper.hpp"

// Specific port types that need access to the protected port union 
// (and should already have access but g++ is a bit messed up).
template <typename T> class Input;
template <typename T> class Output;
template <typename T> class InOut;
template <typename T> class Register;

BEGIN_NAMESPACE_CASCADE

class ClockDomain;
struct Waves;

//////////////////////////////////////////////////////////////////
//
// PortName
//
// Static helper class for constructing the full hierarchical
// name of a port.
//
//////////////////////////////////////////////////////////////////
class PortName
{
public:
    // Find the name of any port in the system
    static strbuff getPortName (const void *address);

    // If the containing component is known, this function is much more efficient
    static strbuff getComponentPortName (const Component *c, const void *address);

private:
    static bool formatPortName (strbuff &s, const Component *c, const void *address, bool search = true);

public:
    static const char *portName[NUM_PORT_DIRECTIONS]; // = "Input", "Output", etc.
};

//////////////////////////////////////////////////////////////////
//
// Port Wrapper Constants
//
//////////////////////////////////////////////////////////////////

// enum PortDirection is defined in Interface.hpp

END_NAMESPACE_CASCADE

enum PortType
{
    PORT_NORMAL,   //  /--XXXXX
    PORT_LATCH,    //  /-------
    PORT_PULSE,    //  /--\____
};

enum PortActivationType
{
    ACTIVE_HIGH,
    ACTIVE_LOW
};

BEGIN_NAMESPACE_CASCADE

enum PortConnection
{
    PORT_UNCONNECTED,   // No connection
    PORT_CONSTANT,      // Wired to a constant
    PORT_WIRED,         // Wired to a variable

    // For each of the remaining connection types, connectedTo points to a 
    // source port wrapper
    PORT_CONNECTED,     // Combinational connection to another port
    PORT_SYNCHRONOUS,   // Synchronous connection to another port
    PORT_SLOWQ,         // Slow synchronous connection (individual copy)
    PORT_PATCHED,       // Patched synchronous connection
};

//////////////////////////////////////////////////////////////////
//
// PortWrapper
//
// Wrapper class that keeps track of construction information and
// gets deleted as part of Sim::init()
//
//////////////////////////////////////////////////////////////////

template <typename T> class Port;
template <typename T> class FifoPort;
template <typename T> class PortArray;
class Constant;

class PortWrapper : public Wrapper
{
    DECLARE_NOCOPY(PortWrapper);
    friend class ClockDomain;
    friend struct UpdateFunctions;
    friend class VerilogPortBinding;
    friend class WavesSignal;
    friend class WavesFifo;
    template <typename T> friend class PortArray;
public:
    // _size is in bytes
    PortWrapper (void *_port, uint16 _size, PortDirection dir);

    // Connection functions
    void wireTo (const void *data);
    void wireToConst (const void *data);
    void connect (PortWrapper *port, int delay);

    // Other construction-time functions
    void setType (PortType _type);
    inline PortType getType () const
    {
        return (PortType) type;
    }
    inline bool isFifo () const
    {
        return direction == PORT_INFIFO || direction == PORT_OUTFIFO;
    }
    void addTrigger (const Trigger &trigger);
    void setDelay (int _delay);

private:
    // Helper function to find the terminal wrapper in a sequence
    // of combinational connections.
    PortWrapper *getTerminalWrapper ()
    {
        return (connection == PORT_CONNECTED) ? connectedTo->getTerminalWrapper() : this;
    }

    // Recursively resolve the port's net
    void resolveNet ();
    void resolveRegister ();

    // Fifo connection resolution
    void resolveFifo ();
    
    // Add strong edges to the update graph
    void addStrongEdges ();

    // Patch a synchronous connection
    void patchRegister ();

public:

    // Netlist and constant resolution
    static void resolveNetlists ();

    // Resolve value pointers and set read-only flag for connected pors
    static void finalizeConnectedPorts ();

    // If all of a port's writers are from the same clock domain, then that is the
    // port's clock domain.  If a port has no writers, then it belongs to it's
    // parent component's clock domain.
    ClockDomain *getClockDomain () const;

    // Determine the fifo's clock period.  If it's zero (indicating a manual tick
    // clock), then walk down the list of connections looking for a non-zero clock
    // period.  If they're all zero, then return the supplied default.
    int getFifoClockPeriod (int defaultPeriod) const;

    // Returns Component.Port
    strbuff getName () const;
    string getAssertionContext () const;

    // Cleanup port state
    static void cleanup ();

public:
    union
    {
        Port<byte> *port;
        FifoPort<byte> *fifo;
        byte *value;              // For temporary ports
    };
    uint16 size        : 13; // Size of port in bytes (see CASCADE_MAX_PORT_SIZE)
    uint16 direction   : 3;  // One of PortDirection
    uint16 type        : 2;  // One of PortType
    uint16 connection  : 3;  // One of PortConnection (also used for FifoType)
    uint16 readOnly    : 1;  // Port read-only flag
    uint16 producer    : 1;  // True if this port has been connected to
    uint16 isD         : 1;  // True if this is the D port of a register
    uint16 noverilog   : 1;  // Do not bind this port to a verilog port
    uint16 arrayInternal : 1; // This port is in an array, but is not the first element
    uint16 patched     : 1;  // This port has been patched and is connected to a PORT_TEMP wrapper
    uint16 nofake      : 1;  // Suppress the fake register optimization for this port

    // By default, Fifos have implicit flow control enabled, which means 
    // that the effect of a pop will not be seen by the producer until 
    // fifoDelay cycles after the pop.  If flow control is being
    // explicitly managed outside of the Fifo (e.g. if multiple
    // virtual channels share the Fifo and there is an explicit VC
    // credit return path), then implicit flow control should be 
    // disabled for the Fifo so that the effect of a pop is immediate
    // (and in fact the producer should never call full() because it
    // should know via the explicit flow control whether or not the
    // Fifo can accept data).
    uint16 fifoDisableFlowControl : 1;

    uint16 mark        : 1;  // Used to avoid cycles in recursive algorithms
    uint16 verilog_wr  : 1;  // True if this port is written from a Verilog binding
    uint16 verilog_rd  : 1;  // True if this port is read by a Verilog binding 

    uint16 fifoSize; // Capacity of fifo (FIFO ports only)

    // Delay of FIFO or synchronous connection.  Also used within port storage 
    // initialization for non-synchronous ports to indicate maximum delay of any 
    // synchronous connection to this port.
    uint16 delay;    

    typedef  descore::PointerVector<UpdateWrapper *, WrapperAlloc<UpdateWrapper *> > UpdateVector;

    UpdateVector readers; // Update functions that can read port
    UpdateVector writers; // Update functions that can write port

    // Triggers associated with the port.
    stack<Trigger, WrapperAlloc<Trigger> > triggers;

    Component *parent; // Component containing the port
    PortWrapper *next; // Used to keep wrappers in linked lists
    union
    {
        PortWrapper *connectedTo; // For connected ports
        const void *wire;         // For wired ports
        const Constant *constant; // For constant ports
    };

public:
    // Helper for constructing port arrays
    static int s_arrayIndex;
};

#define CASCADE_MAX_PORT_DELAY 65535

////////////////////////////////////////////////////////////////////////////////
//
// PortList
//
////////////////////////////////////////////////////////////////////////////////

struct PortList
{
public:

    PortList () : m_first(NULL), m_last(&m_first)
    {
    }
    PortList (const PortList &rhs)
    {
        *this = rhs;
    }
    PortList &operator= (const PortList &rhs)
    {
        m_first = rhs.m_first;
        m_last = m_first ? rhs.m_last : &m_first;
        return *this;
    }

    inline void addPort (PortWrapper *w)
    {
        *m_last = w;
        m_last = &(w->next);
        w->next = NULL;
    }

    inline PortWrapper *first () const
    {
        return m_first;
    }
   
    inline void reset ()
    {
        m_first = NULL;
        m_last = &m_first;
    }

private:
    PortWrapper *m_first;
    PortWrapper **m_last;

public:
    class Remover
    {
    public:
        Remover (PortList &list)
        {
            m_next = list.m_first;
            list.m_first = NULL;
            list.m_last = &list.m_first;
            advance();
        }

        PortWrapper *operator-> () const
        {
            return m_curr;
        }
        PortWrapper *operator* () const
        {
            return m_curr;
        }
        operator bool () const
        {
            return m_curr != NULL;
        }
        void operator++ ()
        {
            advance();
        }
        void operator++ (int)
        {
            advance();
        }
    private:
        void advance ()
        {
            m_curr = m_next;
            if (m_next)
                m_next = m_next->next;
        }

    private:
        PortWrapper *m_next;
        PortWrapper *m_curr;
    };
};

//////////////////////////////////////////////////////////////////
//
// Value flags and Port accessor macros
//
//////////////////////////////////////////////////////////////////

#define VALUE_VALID      2
#define VALUE_VALID_PREV 1

//-----------------
// Macros
//-----------------

#ifdef _DEBUG

#define PORT_READ \
    if (Sim::state == Sim::SimConstruct || (Cascade::Port<T>::hasValidFlag && !(Cascade::Port<T>::valid[-1] & Cascade::Port<T>::validValue))) \
        Cascade::Port<T>::readError()

#define PORT_WRITE \
    if (Cascade::Port<T>::isReadOnly) \
        Cascade::Port<T>::writeError(); \
    PORT_VALIDATE

#define PORT_VALIDATE \
    if (Cascade::Port<T>::hasValidFlag) Cascade::Port<T>::valid[-1] = Cascade::Port<T>::validValue;

#else // #ifdef _DEBUG

#define PORT_READ
#define PORT_WRITE
#define PORT_VALIDATE

#endif // #ifdef _DEBUG

//////////////////////////////////////////////////////////////////
//
// Port<T>
//
//////////////////////////////////////////////////////////////////
struct PortBase {};

template <typename T>
class Port : public PortBase
{
    DECLARE_NOCOPY(Port);

    typedef typename PortValueType<T>::value_t value_t;
    typedef typename PortValueType<T>::read_t  read_t;

    friend class PortWrapper;
    friend class ClockDomain;
    friend class UpdateConstructor;
    friend class VerilogPortBinding;
    friend class DPIPortBinding;
    friend struct Cascade::Waves;
    friend class WavesSignal;
    friend class PortStorage;
    friend class EventHelper;

    // Specific port types that need access to the protected union.. not
    // sure why it's not just inherited.
    friend class Input<T>;
    friend class Output<T>;
    friend class InOut<T>;
    friend class Register<T>;

public:
    Port (PortDirection dir)
#ifdef _DEBUG
        : hasValidFlag(true), isReadOnly(true), validValue(VALUE_VALID)
#endif
    {
        // The maximum port size is 65528 bits so that sizeInBits
        // can be represented with 16 bits.
        STATIC_ASSERT(sizeof(value_t) * 8 < CASCADE_MAX_PORT_SIZE);

        wrapper = new PortWrapper(this, sizeof(value_t), dir);
        Hierarchy::addPort(dir, this, getPortInfo<T>(), wrapper);
        Sim::updateChecksum("Port", dir);
    }

    // Wire a port directly to a variable of the same type.  The port 
    // becomes read-only, and has no valid flag.  It is an error to wire a 
    // port to a value if the port (or a connected port) is already wired.
    void wireTo (const value_t &data) 
    { 
        assert_always(Sim::state == Sim::SimConstruct);
        wrapper->wireTo(&data); 
    }

    // Wire a port directly to a constant of the same type.  The port
    // becomes read-only, and has no valid flag.  It is an error to wire a 
    // port to a value if the port (or a connected port) is already wired.
    void wireToConst (value_t data) 
    { 
        assert_always(Sim::state == Sim::SimConstruct);
        wrapper->wireToConst(&data); 
    }

    // Get/set the port type
    void setType (PortType type) 
    { 
        assert_always(Sim::state == Sim::SimConstruct);
        wrapper->setType(type); 
    }
    PortType getType () const
    { 
        assert_always(Sim::state == Sim::SimConstruct);
        return wrapper->getType(); 
    }

    // Set the port delay
    void setDelay (int delay)
    {
        assert_always(Sim::state == Sim::SimConstruct);
        assert_always(unsigned(delay) <= CASCADE_MAX_PORT_DELAY);
        wrapper->setDelay(delay);
    }

    // Associate an activation target with the port (multiple activation 
    // targets can be associated with a single port).
    void activates (Component *target, PortActivationType activationType = ACTIVE_HIGH) 
    { 
        assert_always(Sim::state == Sim::SimConstruct);
        wrapper->addTrigger(Trigger((intptr_t) target, activationType == ACTIVE_LOW));
    }

    // Associate a trigger action with the port (multiple trigger actions can be
    // associated with a single port).
    void addTrigger (ITrigger<value_t> *trigger, PortActivationType activationType = ACTIVE_HIGH)
    {
        assert_always(Sim::state == Sim::SimConstruct);
        wrapper->addTrigger(Trigger(((intptr_t) trigger) | TRIGGER_ITRIGGER, activationType == ACTIVE_LOW)); 
    }

    // Exclude this port when binding to a Verilog interface
    void noVerilog ()
    {
        assert_always(Sim::state == Sim::SimConstruct);
        wrapper->noverilog = 1;
    }

    // Use this function instead of assignment to reset a port in case the port is
    // wired to a constant
    void reset (const value_t &data) 
    { 
        // Be careful not to overwrite constants
        if (Cascade::Constant::isConstant(value))
            return;

        PORT_VALIDATE; 
        *value = data; 
    }

    // Write accessors
    inline T operator= (const value_t &data) 
    { 
        PORT_WRITE; 
        return *value = data; 
    }
    inline value_t *nonConstPtr () 
    { 
        PORT_WRITE; 
        return value; 
    }

    // Read accessors
    inline operator read_t () const 
    { 
        PORT_READ; 
        return *value; 
    }
    inline const value_t &operator() () const 
    { 
        PORT_READ; 
        return *value; 
    }
    inline const value_t &operator* () const 
    { 
        PORT_READ; 
        return *value; 
    }
    inline const value_t *constPtr () const 
    { 
        PORT_READ; 
        return value; 
    }
    inline const value_t *operator-> () const 
    { 
        PORT_READ; 
        return value; 
    }
    inline const value_t peek () const
    {
        return *value;
    }

    // Read-modify-write accessors
    inline T operator+= (const value_t &data)
    {
        return *this = *this + data;
    }
    inline T operator-= (const value_t &data)
    {
        return *this = *this - data;
    }
    inline T operator*= (const value_t &data)
    {
        return *this = *this * data;
    }
    inline T operator/= (const value_t &data)
    {
        return *this = *this / data;
    }
    inline T operator%= (const value_t &data)
    {
        return *this = *this % data;
    }
    inline T operator&= (const value_t &data)
    {
        return *this = *this & data;
    }
    inline T operator|= (const value_t &data)
    {
        return *this = *this | data;
    }
    inline T operator^= (const value_t &data)
    {
        return *this = *this ^ data;
    }
    inline T operator>>= (int shift)
    {
        return *this = *this >> shift;
    }
    inline T operator<<= (int shift)
    {
        return *this = *this << shift;
    }

    // This function makes don't-care states explicit by validating a port
    // without assigning a value to it.  This prevents the infrastructure from
    // complaining in Debug builds about invalid values that are read
    // but not used.  In Release builds the function is optimized away.
    inline void dontCare (void) 
    { 
        PORT_WRITE;
#ifdef _DEBUG
        memset(value, 0xcd, sizeof(T));
#endif
    }

    // Do everything that we do when assigning a port, but don't actually
    // assign a value.  Used as an optimization when the previous value
    // is known to be correct, so all we need to do is validate it.
    // Note that this is the same as dontCare(), but has a different name
    // for code clarity.
    inline void setValid (void) 
    { 
        PORT_WRITE; 
    }

protected:

    // Handle a read from an invalid port.
    void readError () const
    {
        assert(Sim::state != Sim::SimConstruct, "Cannot read ports before simulation has been initialized.\n"
            "    This can happen if you try to connect ports of different integer types");

        // Allow reads of invalid ports during reset to support iterative reset
        if (Sim::state == Sim::SimResetting)
            return;

        // If valid[-1] is VALUE_VALID then we're reading from a fake register
        // that was already written on this clock cycle, but fake registers are 
        // supposed to be write after read.  This indicates that one or more
        // .reads()/.writes() directives are missing, which resulted in an invalid
        // update order.  Otherwise, this is just an invalid port that hasn't been
        // set yet.
        if (valid[-1] == VALUE_VALID)
        {
            warn(false, "Cannot read eliminated register that has already been written on this clock cycle.\n"
                "    This can happen if Cascade doesn't have the full set of readers/writers for this port.\n"
                "    Are you missing a .reads() or .writes() declaration?");
        }
        else
        {
            warn(false, "Port is invalid");
        }
    }

    // Handle a write to a read-only port.
    void writeError () const
    {
        if (Sim::state < Sim::SimInitialized)
            die("Ports cannot be assigned until the simulation has been initialized");
        else if (Cascade::Constant::isConstant(value))
            die("Assignment to port that has been wired to a constant");
        else
            die("Assignment to read-only port");
    }

protected:
    union
    {
        value_t *value;       // points to the value used by this port
        PortWrapper *wrapper; // used during initialization
        byte *valid;          // used in debug builds to access value valid flags
    };

#ifdef _DEBUG
    bool hasValidFlag;
    bool isReadOnly;
    byte validValue;
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

// Make sure the port structure isn't bigger than expected
#ifdef _DEBUG
STATIC_ASSERT(sizeof(Port<byte>) <= 2 * sizeof(void *));
#else
STATIC_ASSERT(sizeof(Port<byte>) == sizeof(void *));
#endif

END_NAMESPACE_CASCADE

#endif

