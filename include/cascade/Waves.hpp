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
// Waves.hpp
//
// Copyright (C) 2010 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshawresearch.com)
// Created: 02/18/2010
//
// Support for VCD file generation
//
////////////////////////////////////////////////////////////////////////////////

#ifndef Waves_hpp_100218100811
#define Waves_hpp_100218100811

BEGIN_NAMESPACE_CASCADE

struct WavesComponent;

////////////////////////////////////////////////////////////////////////////////
//
// Static class with global dumping functions
//
////////////////////////////////////////////////////////////////////////////////
struct Waves
{
    // Enable dumping for a portion of the hierarchy.  'level' levels of the
    // hierarchy will be dumped starting with every component that matches the
    // specified wildcard name.  If level = 0, then the entire component 
    // hierarchy will be dumped starting with every matching component.  Only 
    // those signals matching the signal wildcard name will be dumped.
    static void dumpSignals (const char *wcComponent, const char *wcSignals, int level);
    static void dumpSignals (const Component *component, const char *wcSignals, int level);

    //----------------------------------
    // Internal helper functions
    //----------------------------------

    // Initialization (called from Sim::init)
    static void initialize ();
    static void resolveSignals ();

    // Cleanup (called from Sim::cleanup)
    static void cleanup ();

    // Refresh signal state following an archive load
    static void archive ();

    //----------------------------------
    // These are static member functions for various friend access
    //----------------------------------

    // Add matching signals from the specified component
    static void initSignals (const Component *c, const char *signals);

    // Retrieve a waves component object based on a hierarchical component name, 
    // creating waves component objects as necessary.
    static WavesComponent *getWavesComponent (const Component *c);
};

////////////////////////////////////////////////////////////////////////////////
//
// Waves file
//
////////////////////////////////////////////////////////////////////////////////
class WavesFile
{
public:
    WavesFile ();
    virtual ~WavesFile ();

    //----------------------------------
    // Base class functions
    //----------------------------------
    void open (const char *filename);
    void close ();

    void valueChange (uint32 id, const byte *value, bool undefined, int sizeInBits);

    //----------------------------------
    // Specialized functions
    //----------------------------------

    // File header
    virtual const char *fmode () const
    {
        return "w";
    }
    virtual void beginFile () = 0;

    // Define signals
    virtual void beginComponent (const char *name) = 0;
    virtual uint32 addSignal (const char *name, int sizeInBits) = 0;     // returns UID
    virtual void endComponent () = 0;
    virtual void endSignals () = 0;

    // Add value change events
    virtual void dumpTime (uint64 time) = 0;
    virtual void dumpValue (uint32 id, const byte *value, bool undefined, int sizeInBits) = 0;

protected:
    FILE *m_file;
    uint64 m_currTime;
    uint64 m_currSimTime;
    descore::SpinLock m_lock;
};

// VCD implementation
class VcdWavesFile : public WavesFile
{
public:
    VcdWavesFile () : m_nextId(33) {}
    ~VcdWavesFile ();
    void beginFile ();

    // Define signals
    void beginComponent (const char *name);
    uint32 addSignal (const char *name, int sizeInBits); // returns UID
    void endComponent ();
    void endSignals ();

    // Add value change events
    void dumpTime (uint64 time);
    void dumpValue (uint32 id, const byte *value, bool undefined, int sizeInBits);

private:
    uint32 m_nextId;
};

////////////////////////////////////////////////////////////////////////////////
//
// Shared interface for signals/FIFOs to simplify recursive calls over the
// entire tree
//
////////////////////////////////////////////////////////////////////////////////
struct IWavesFunctions
{
    virtual ~IWavesFunctions () {}

    // Called during initialization after ports have been resolved to finalize 
    // types, flags and data pointers.
    virtual void resolve () = 0;

    // Called before the first time change to make sure that initial values for
    // all signals have been dumped to the waves file.
    virtual void dumpInitialValues () = 0;

    // Archive waves state
    virtual void archive () = 0;
};

typedef void (IWavesFunctions::*wavefunc) ();

////////////////////////////////////////////////////////////////////////////////
//
// Signal to dump
//
////////////////////////////////////////////////////////////////////////////////
class WavesSignal : public IWavesFunctions
{
    friend struct WavesComponent;
    friend class WavesFifo;
public:
    // Signal types
    enum SignalType
    {
        CLOCK,
        RESET,
        FIFO,          // Will be specialized to one of the below
        FIFO_PRODUCER,
        FIFO_CONSUMER,
        FIFO_TRIGGER,
        PORT,
        REGQ,
        SIGNAL
    };

    // Construct the signal at construction time
    WavesSignal (SignalType type, const void *data, const PortInfo *info, int index);
    ~WavesSignal ();

    // IWavesFunctions
    void resolve ();
    void dumpInitialValues ();
    void archive ();

    // Write the index and obtain the id at initialization time
    void writeIndex (string name);

    // Dump the signal at run time (called from WaveComponent)
    void dump ();

    // Linked lists
    WavesSignal *next;

private:
    const PortInfo *m_info; // Information about the port type
    union
    {
        const Port<byte> *m_port; // Pointer to port (will be resolved to m_data)
        const byte       *m_data; // Pointer to data
    };
    byte   *m_currVal;   // Most recent observed value
    byte   m_validValue; // validValue for a debug build port with a value flag, or 0
    byte   m_currValid;  // Most recent observed valid flag
    byte   m_type;       // One of SignalType
    uint32 m_id;         // Signal UID
    int    m_index;      // Signal index within parent component
};

////////////////////////////////////////////////////////////////////////////////
//
// Support for FIFO ports
//
////////////////////////////////////////////////////////////////////////////////
class WavesFifoTriggerProxy;

class WavesFifo : public IWavesFunctions
{
    friend struct WavesComponent;
public:
    WavesFifo (WavesSignal::SignalType type, FifoPort<byte> *port, const PortInfo *info, int index, string name);
    ~WavesFifo ();

    // IWavesFunctions
    void resolve ();
    void dumpInitialValues () {}
    void archive ();

    // Stash the number of in-flight pushes in m_fullCount
    void archiveFullCount ();

    // Signals
    WavesSignal dataSignal;
    WavesSignal validSignal;
    WavesSignal creditSignal;
    WavesFifo   *next;

    // Update state & dump
    void tick ();
    void update ();

private:
    bit    m_valid;
    bit    m_credit;

    // Snapshot of FIFO state used to deduce valid and credit signals at both the 
    // input and output.  The rules are as follows:
    //
    //   Producer.valid:  set when tail has changed following update()
    //   Consumer.valid:  delay = 0: set when tail has changed following update()
    //                    delay > 0: set when fullCount has changed following tick()
    //   Consumer.credit: set when head has changed following update()
    //   Producer.credit: delay = 0: set when head has changed following update()
    //                    delay < 0: set when freeCount has changed following tick()
    uint16 m_head;  
    uint16 m_tail;  
    uint16 m_freeCount;
    uint16 m_fullCount;

    union
    {
        FifoPort<byte> *m_fifoPort;  // Pointer to port (will be resolved to m_fifo)
        Fifo<byte> *m_fifo;          // Pointer to actual fifo
        WavesFifoTriggerProxy *m_trigger; // Used for FIFO_TRIGGER
    };
    string m_name;
};

class WavesFifoTriggerProxy : public ITrigger<byte>
{
    friend class WavesFifo;
public:
    WavesFifoTriggerProxy (Fifo<byte> *fifo);

    virtual void trigger (const byte &_data);

private:
    ITrigger<byte> *m_trigger;
    byte *m_data;
    int m_size;
    bit m_triggered;
};

////////////////////////////////////////////////////////////////////////////////
//
// Hierarchy automatically constructed from signal names
//
////////////////////////////////////////////////////////////////////////////////
struct WavesComponent
{
    WavesComponent (ClockDomain *_domain = NULL) : domain(_domain) {}
    ~WavesComponent ();
    void doAcross (wavefunc f);
    void cleanup ();

    typedef std::map<string, WavesComponent *> ComponentMap;
    typedef std::map<string, WavesSignal *> SignalMap;

    ComponentMap children;
    SignalMap signals;
    std::vector<WavesFifo *> fifos;

    // Default clock domain for this component
    ClockDomain *domain;

    // Generate the file index for this component
    void writeIndex ();
};

END_NAMESPACE_CASCADE

#endif // #ifndef Waves_hpp_100218100811

