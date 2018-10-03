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
// ClockDomain.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
// A clock domain is a set of components which are all updated on the same
// clock.
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_ClockDomain_hpp_
#define Cascade_ClockDomain_hpp_

#include "Update.hpp"
#include "SimGlobals.hpp"
#include "Clock.hpp"
#include <descore/PointerVector.hpp>
#include "FifoPorts.hpp"
#include "PortStorage.hpp"

class Component;
class Archive;

#ifdef _VERILOG
#include <vcs_vpi_user.h>
#else
typedef int32 *vpiHandle;
#endif

BEGIN_NAMESPACE_CASCADE

class PortWrapper;
class ClockDomain;
class WavesSignal;
class WavesFifo;
struct Waves;

extern __thread ClockDomain *t_currentClockDomain;
extern __thread const S_Update *t_currentUpdate;

////////////////////////////////////////////////////////////////////////////////
//
// Structure used to keep track of synchronous triggers
//
////////////////////////////////////////////////////////////////////////////////
struct SyncTrigger
{
    S_Trigger *trigger;
    intptr_t  val;
};

class TriggerStack : public stack<SyncTrigger>
{
public:
    // Push a trigger; return a pointer to the value
    inline byte *push (S_Trigger *trigger)
    {
        if (m_size == m_capacity)
            realloc(2 * m_capacity);
        m_vals[m_size].trigger = trigger;
        return (byte *) &m_vals[m_size++].val;
    }
};

////////////////////////////////////////////////////////////////////////////////
//
// ClockDomain
//
////////////////////////////////////////////////////////////////////////////////
class ClockDomain 
{
    DECLARE_NOCOPY(ClockDomain);

    typedef std::multimap<int, IEvent *> EventMap;
    friend struct Waves;
    friend class PortStorage;
    friend void forallThreaded (int id);
    friend void forallUnthreaded (ClockDomain *domains, void (ClockDomain::*func) ());
    friend void runThreaded (ClockDomain *domains, void (ClockDomain::*func) ());

public:

    // A clock domain can either be created with a period, or defined in terms
    // of another clock domain and a period multiplier which will be resolved
    // during initialization, or declared as manually ticked.
    ClockDomain (int period, int offset);
    ClockDomain (Clock *generator, float ratio, int offset);
    ClockDomain ();
    void initialize (bool manual);

    // Delete various arrays
    ~ClockDomain ();

    //----------------------------------
    // Initialization/cleanup
    //----------------------------------

    // Get the default clock domain, creating it if necessary
    static ClockDomain *getDefaultClockDomain ();

    // Helper functions to call an initialization function on all domains
    static void doAcross (void (ClockDomain::*func) ());
    static void doAcross (void (PortStorage::*func) ());

    // Initialize ports, create the update array, resolve the clock period
    // and schedule the clock domain.
    static void initialize ();

    // Delete all clock domains
    static void cleanupClockDomains ();

    // Called by initialize()
    void initPorts ();
    void resolvePeriod ();
    void initializeGeneratorParams ();

    // Reset state
    static void resetDomains ();
    static void resetPorts ();
    static void propagateReset ();
    void resetDomain ();
    static void resetEvents ();
    void resetEventsInternal ();

    // Force the sticky triggers into a consistent state following a reset or
    // a load from an archive, and on reset manually activate any synchronous triggers
    // that are initially active.
    static void resetTriggers (bool isReset = true);
    void resetTriggersInternal ();
    void resetSyncTrigger (S_Trigger *trigger);

    // Register a port with this clock domain
    inline void addPort (PortWrapper *p)
    {
        m_portWrappers.addPort(p);
    }

    // Register a constant port with stuck triggers
    void addStuckTrigger (PortWrapper *p)
    {
        m_updateSentinel.triggers.push(p);
    }

    // Add a tickable component to the array
    void registerTickableComponent (Component *c);

    // Add an update wrapper to this list
    void registerUpdateFunction (UpdateWrapper *update); 

    // Sort the update functions and create the update tree
    void sortUpdateFunctions ();
    void createUpdateArray ();
    void setUpdateOffsets (UpdateWrapper *w);
    void writeUpdates (UpdateWrapper *w);
    int writeTriggers (UpdateWrapper *w, byte *&dst);

    // Add a signal to dump on every rising clock edge
    void addWavesSignal (Cascade::WavesSignal *s);
    void addWavesRegQ (Cascade::WavesSignal *s);
    void addWavesClock (Cascade::WavesSignal *s);
    void addWavesFifo (Cascade::WavesFifo *f);
    static void addGlobalWavesSignal (Cascade::WavesSignal *s);

    // Find the clock domain that contains this non-fifo data in its port array, 
    // or return NULL if there is no domain that contains the data.
    static ClockDomain *findOwner (const byte *data);

    // Register a verilog clock port with this clock domain
    void registerVerilogClock (vpiHandle port);

    // Helper function to determine if this clock domain is compatible with 
    // another clock domain (i.e. combinational connections are allowed).
    // Two clock domains are compatible if they can never have simultaneous
    // rising clock edges.
    bool compatible (const ClockDomain *d) const;

    //----------------------------------
    // Simulation
    //----------------------------------

    // Manually tick this clock domain and any domains whose clocks are generated
    // from this one.
    void manualTick ();

    // Combinational update
    void update (); 
    void evalTrigger (S_Trigger *trigger);

    // Rising clock edge
    void preTick ();
    void tick ();     // main tick() function
    void postTick (); // reset ports and do scheduled events

    // Synchronous fifo updates
    void schedulePush (GenericFifo *fifo);
    void schedulePop (GenericFifo *fifo);

    // Get the clock period
    inline int getPeriod () const 
    { 
        return m_period; 
    }

    // Update the nextTick time
    uint64 roundTime (uint64 time);
    void updateNextEdge ();
    int64 getTick (int index);
    int64 getNextTick (int index, int64 prevRisingEdge);
    int64 getTime () const
    {
        return m_nextEdge;
    }
        
    // Re-insert the clock domain into the global linked list
    void scheduleClockDomain ();

    // Run the simulation until a specified time in picoseconds, 
    // or for a single tick() event if runUntil is zero.
    static void runSimulation (uint64 runUntil);

    // Simulate a rising clock edge for the list of clock domains defined by
    // m_nextSameTick
    static void tickDomains (ClockDomain *runList);

    // Returns the number of rising clock edges (including the current one if
    // called from an update function).
    inline int getTickCount () const
    {
        return m_numTicks;
    }

    // Schedule an event for some time in the future
    void scheduleEvent (int delay, IEvent *event);

    // Drive Verilog clock ports
    void driveVerilogClocks ();

    // Dump waves
    void dumpWaves ();
    void dumpRegQs ();
    void dumpClocks ();

    //----------------------------------
    // Archiving
    //----------------------------------

    static void archiveClockDomains (Archive &ar);
    void archive (Archive &ar);

private:
    int    m_id;        // Clock domain UID used for archiving
    int    m_period;    // Clock period in picoseconds (0 for a manual clock domain)
    int    m_numTicks;  // Number of rising clock edges that have been simulated
    int    m_numEdges;  // Number of rising or falling clock edges
    int64  m_nextEdge;  // Time of next clock edge in picoseconds
    int64  m_prevTick;  // Time of most recent rising clock edge
    int    m_prevIndex; // Logical index of most recent rising clock edge

    // Store clock domain in a linked list of linked lists.  For automatically ticked
    // clock domains, the top-level linked list has distinct nextTick times and is in 
    // increasing order of these times; each domain in this top-level list is the head 
    // of a secondary list of domains with the same nextTick time.  For manually ticked 
    // clock domains, the top-level linked list has separate driver domains in no
    // particular order, then domains which divide a driver domain are in that domain's
    // sameTick list (again, in no particular order).
    ClockDomain *m_nextDifferentTick;
    ClockDomain *m_nextSameTick;
    ClockDomain *m_lastSameTick;

    // Linked list used to do multithreaded work on clock domains
    ClockDomain *m_next;

    // Rising clock edge
    descore::PointerVector<Component *> m_tickableComponents; // Components with tick() defined
    stack<vpiHandle>   m_verilogClocks;   // List of verilog clock ports to drive

    // Update array
    byte                 *m_updates;        // Sorted list of update functions
    int                   m_updateSize;     // Size in bytes of update data
    std::set<S_Trigger *, descore::allow_ptr<S_Trigger *> > 
                          m_stickyTriggers; // Triggers that need to be checked on every cycle

    // Events scheduled for a future rising clock edge
    TriggerStack         *m_syncTriggers;   // Triggers scheduled for after the clock tick
    stack<GenericFifo *> *m_syncFifoPush;   // Fifo pushes scheduled for a future rising clock edge
    stack<GenericFifo *> *m_syncFifoPop;    // Fifo pops scheduled for a future rising clock edge
    int m_syncIndex;                        // Next set of vectors to fire
    int m_syncDepth;                        // Depth of sync vector arrays (power of two)
    int m_syncMask;                         // = m_syncDepth - 1
    EventMap m_events;                      // Generic explicitly-registered events with arbitrary delay

    // Wave dumping
    Cascade::WavesSignal        *m_waveSignals;
    Cascade::WavesSignal        *m_waveRegQs;
    Cascade::WavesSignal        *m_waveClocks;
    Cascade::WavesFifo          *m_waveFifos;
    static Cascade::WavesSignal *s_globalWaves;

    // Initialization
    UpdateWrapper *m_updateWrappers; // List of update functions belonging to this clock domain
    UpdateWrapper  m_updateSentinel; // Head sentinel used for rising-clock-edge triggers
    Clock         *m_dividedClock;   // Clock that we're dividing
    float          m_clockRatio;     // Clock period multiplier applied to generator domain clock
    int            m_clockOffset;    // Offset in ps of first rising edge from time 0
    bool           m_resolvedPeriod; // Flag indicating that we've resolved ratio/offset/period
    PortList       m_portWrappers;   // Ports assigned to this clock domain

    // If this clock domain has a generator and the clock ratio is rational (a/b), then 
    // synchronize every b'th rising edge of this domain with every a'th rising edge of the
    // generator domain according to a fixed offset.  Specifically, pick integers m and k
    // such that the bn'th logical rising clock edge of this domain is offset by k from the
    // (an+m)'th logical rising clock edge of the generator domain, where "logical rising
    // clock edge" includes clock edges before time 0 (for domains with a negative offset)
    // that don't actually get simulated.  
    ClockDomain *m_generator;
    int m_gen_a;   
    int m_gen_b;   
    int m_gen_m;
    int m_gen_k;

    // Port values
    PortStorage m_ports;

private:
    static ClockDomain *s_first;              // Automatically scheduled clock domains
    static ClockDomain *s_firstManual;        // Manually scheduled clock domains
    static int s_numClockDomains;
    static time_t s_lastDeadlockCheckTime;
    static ClockDomain *s_defaultClockDomain; // Clock domain for top-level components with no explicit domain
};

END_NAMESPACE_CASCADE

#endif

