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
// ClockDomain.cpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/22/2007
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Cascade.hpp"
#include <descore/MapIterators.hpp>
#include <descore/Thread.hpp>

#ifndef _MSC_VER
#include <sys/time.h>
#endif

#ifdef _VERILOG
#include <veriuser.h>
#endif

BEGIN_NAMESPACE_CASCADE

////////////////////////////////////////////////////////////////////////
//
// Static/global variables
//
////////////////////////////////////////////////////////////////////////
ClockDomain *ClockDomain::s_first = NULL;
ClockDomain *ClockDomain::s_firstManual = NULL;
int ClockDomain::s_numClockDomains = 0;
time_t ClockDomain::s_lastDeadlockCheckTime = 0;
ClockDomain *ClockDomain::s_defaultClockDomain = NULL;
__thread ClockDomain *t_currentClockDomain = NULL;
__thread const S_Update *t_currentUpdate = NULL;
Cascade::WavesSignal *ClockDomain::s_globalWaves = NULL;

////////////////////////////////////////////////////////////////////////////////
//
// getTimer()
//
////////////////////////////////////////////////////////////////////////////////
static uint64 getTimer ()
{
    uint64 ret = 0;

#ifndef _MSC_VER
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ret = ((uint64) tv.tv_sec) * 1000000 + tv.tv_usec;
#endif

    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//
// Threading
//
////////////////////////////////////////////////////////////////////////////////

// Thread pool
static descore::Thread *g_threads;
static int g_numThreads;

// Global variables used to assign work
static ClockDomain * volatile * g_domains;
static void (ClockDomain::* volatile g_func) ();

// Synchronization
static volatile bool g_exitThreads;
static volatile bool g_beginLoop[2];
static volatile int g_turnstile;
static int g_signalIndex;

// Errors
static descore::runtime_error * volatile g_error;
static descore::SpinLock g_errorLock;

static void setThreadError (descore::runtime_error *error)
{
    descore::ScopedSpinLock lock(g_errorLock);
    if (!g_error)
        g_error = error;
    else
    {
        error->handled();
        delete error;
    }
}

static void clockDomainThreadFunc (int id);

static void initThreads ()
{
    int numProcessors = descore::numProcessors();
    if (params.NumThreads <= 0)
    {
        assert(params.NumThreads == -1, "cascade.NumThreads must be -1 or a positive integer");
        g_numThreads = numProcessors - 1;
        log("Running with %d threads\n", numProcessors);
    }
    else if (numProcessors < params.NumThreads)
    {
        log("cascade.NumThreads is set to %d but only %d processors have been detected.\n", 
            *params.NumThreads, numProcessors);
        log("Running with %d threads\n", numProcessors);
        g_numThreads = numProcessors - 1;
    }
    else
        g_numThreads = params.NumThreads - 1;
    g_domains = new ClockDomain * volatile [g_numThreads + 1];
    g_threads = new descore::Thread[g_numThreads];
    g_exitThreads = false;
    g_beginLoop[0] = false;
    g_signalIndex = 0;
    for (int i = 0 ; i < g_numThreads ; i++)
        g_threads[i].start(&clockDomainThreadFunc, i);
}

static void cleanupThreads ()
{
    g_exitThreads = true;
    g_beginLoop[0] = g_beginLoop[1] = true;
    delete[] g_threads;
    g_threads = NULL;
}

// These should really be static, but MSVC then requires the static keyword in the 
// ClockDomain friend declaration, whereas gcc prohibits it.  So, just make the
// functions non-static; they're in the Cascade namespace anyways.
void forallThreaded (int id)
{
    try
    {
        for (t_currentClockDomain = g_domains[id] ; t_currentClockDomain && !g_error ; t_currentClockDomain = t_currentClockDomain->m_next)
            (t_currentClockDomain->*g_func)();
    }
    catch (descore::runtime_error &e)
    {
        setThreadError(e.clone());
    }
    catch (std::exception &e)
    {
        setThreadError(descore::runtime_error("Error: %s", e.what()).clone());
    }
    catch (...)
    {
        setThreadError(descore::runtime_error("Error: Unknown exception").clone());
    }
}

void forallUnthreaded (ClockDomain *domains, void (ClockDomain::*func) ())
{
    ClockDomain *prev = t_currentClockDomain;
    for (t_currentClockDomain = domains ; t_currentClockDomain ; t_currentClockDomain = t_currentClockDomain->m_nextSameTick)
        (t_currentClockDomain->*func)();
    t_currentClockDomain = prev;
}

static void clockDomainThreadFunc (int id)
{
    int signalIndex = 0;
    while (1)
    {
        while (!g_beginLoop[signalIndex])
            descore::Thread::yield();
        if (g_exitThreads)
            return;
        forallThreaded(id);
        descore::atomicDecrement(g_turnstile);
        signalIndex = 1 - signalIndex;
    }
}

void runThreaded (ClockDomain *domains, void (ClockDomain::*func) ())
{
    static bool runningThreaded = false;

    // If we're currently within a threaded loop, then run unthreaded instead.
    // This can occur when a component manually ticks a clock from its own tick() function.
    if (runningThreaded)
    {
        forallUnthreaded(domains, func);
        return;
    }

    // Divide up the work
    for (int i = 0 ; i <= g_numThreads ; i++)
        g_domains[i] = NULL;
    int id = g_numThreads;
    for ( ; domains ; domains = domains->m_nextSameTick)
    {
        domains->m_next = g_domains[id];
        g_domains[id] = domains;
        id = id ? id - 1 : g_numThreads;
    }

    // Run
    runningThreaded = true;
    g_error = NULL;
    g_func = func;
    g_turnstile = g_numThreads;
    g_beginLoop[1 - g_signalIndex] = false;
    g_beginLoop[g_signalIndex] = true;
    forallThreaded(g_numThreads);

    // Synchronize
    while (g_turnstile)
        descore::Thread::yield();
    g_signalIndex = 1 - g_signalIndex;

    // Check for errors
    if (g_error)
    {
        cleanupThreads();
        g_error->rethrow();
    }
    runningThreaded = false;
}

////////////////////////////////////////////////////////////////////////
//
// cleanupClockDomains()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::cleanupClockDomains ()
{
    // Clean up the automatic clock domains
    while (s_first)
    {
        ClockDomain *temp1 = s_first;
        s_first = s_first->m_nextDifferentTick;

        while (temp1)
        {
            ClockDomain *temp2 = temp1;
            temp1 = temp2->m_nextSameTick;
            delete temp2;
        }
    }

    // Clean up the manual clock domains
    while (s_firstManual)
    {
        ClockDomain *temp1 = s_firstManual;
        s_firstManual = s_firstManual->m_nextDifferentTick;

        while (temp1)
        {
            ClockDomain *temp2 = temp1;
            temp1 = temp2->m_nextSameTick;
            delete temp2;
        }
    }

    s_numClockDomains = 0;
    s_defaultClockDomain = NULL;
    s_globalWaves = NULL;

    // Clean up the threads
    cleanupThreads();
}

////////////////////////////////////////////////////////////////////////
//
// ClockDomain()
//
////////////////////////////////////////////////////////////////////////
ClockDomain::ClockDomain (int period, int offset)
{
    assert_always(period > 0, "Clock domain period cannot be zero");
    initialize(false);
    m_period = period;
    m_clockOffset = offset;
}

ClockDomain::ClockDomain (Clock *generator, float ratio, int offset)
{
    initialize(false);
    m_dividedClock = generator;
    m_clockRatio = ratio;
    m_clockOffset = offset;
}

ClockDomain::ClockDomain ()
{
    initialize(true);
    m_clockOffset = 0;
}

void ClockDomain::initialize (bool manual)
{
    m_id = s_numClockDomains++;
    m_period = 0; 
    m_numTicks = 0;
    m_numEdges = 0;
    m_nextDifferentTick = NULL;
    m_nextSameTick = NULL;
    m_lastSameTick = NULL;
    m_updates = NULL;
    m_updateSize = 0;
    m_syncTriggers = NULL;
    m_syncFifoPush = NULL;
    m_syncFifoPop = NULL;
    m_syncIndex = 0;
    m_syncDepth = 0;
    m_updateWrappers = NULL;
    m_dividedClock = NULL;
    m_generator = NULL;
    m_clockRatio = 1.0f;
    m_resolvedPeriod = false;
    m_waveSignals = NULL;
    m_waveRegQs = NULL;
    m_waveClocks = NULL;
    m_waveFifos = NULL;

    Sim::updateChecksum("ClockDomain", m_period);
    if (manual)
    {
        m_nextDifferentTick = s_firstManual;
        s_firstManual = this;
    }
    else
    {
        m_nextDifferentTick = s_first;
        s_first = this;
    }

    Sim::stats.numClockDomains++;
}

////////////////////////////////////////////////////////////////////////
//
// ~ClockDomain()
//
////////////////////////////////////////////////////////////////////////
ClockDomain::~ClockDomain ()
{
    delete[] m_updates;
    delete[] m_syncTriggers;
    delete[] m_syncFifoPush;
    delete[] m_syncFifoPop;
}

////////////////////////////////////////////////////////////////////////////////
//
// compatible()
//
////////////////////////////////////////////////////////////////////////////////
static int rounded_gcd (int a, int b)
{
    if (a > b)
        return rounded_gcd(b, a);
    if (a <= (int) params.ClockRounding)
        return b;
    return rounded_gcd(a, b-a);
}

bool ClockDomain::compatible (const ClockDomain *d) const
{
    // A clock domain is always compatible with itself
    if (this == d)
        return true;

    // Manual clock domains are only compatible with themselves
    if (!m_period || !d->m_period)
        return false;

    // Find the GCD of the periods, taking rounding into account
    int gcd = rounded_gcd(m_period, d->m_period);

    // Find the relative offset within the GCD period
    int offset = d->m_clockOffset - m_clockOffset;
    while (offset <= - (gcd/2))
        offset += gcd;
    while (offset > gcd / 2)
        offset -= gcd;

    // If the offset is within than clock rounding, then the domains may have 
    // a simultaneous rising clock edge
    return abs(offset) > (int) params.ClockRounding;
}

/////////////////////////////////////////////////////////////////
//
// getDefaultClockDomain()
//
/////////////////////////////////////////////////////////////////
ClockDomain *ClockDomain::getDefaultClockDomain ()
{
    if (!s_defaultClockDomain)
        s_defaultClockDomain = new ClockDomain(params.DefaultClockPeriod, 0);
    return s_defaultClockDomain;
}

////////////////////////////////////////////////////////////////////////////////
//
// preTick()
//
////////////////////////////////////////////////////////////////////////////////
void ClockDomain::preTick ()
{
    if (m_numEdges & 1) 
        m_ports.preTick();
}

////////////////////////////////////////////////////////////////////////
//
// tick()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::tick ()
{
    if (!(m_numEdges & 1))
        return;

    int i;

    // Tick components
    for (i = 0 ; i < m_tickableComponents.size() ; i++)
    {
        if (m_tickableComponents[i]->m_componentActive)
            m_tickableComponents[i]->doTick();
    }

    // Tick registers
    m_ports.tick();
}

/////////////////////////////////////////////////////////////////
//
// postTick
//
/////////////////////////////////////////////////////////////////
void ClockDomain::postTick ()
{
    if (!(m_numEdges & 1))
        return;

    int i;

    // Invalidate N ports and zero pulse ports
    m_ports.postTick();

    // Scheduled trigger/fifo events
    if (m_syncDepth)
    {
        m_syncIndex = (m_syncIndex + 1) & m_syncMask;

        // Fifo push
        stack<GenericFifo *> &pushes = m_syncFifoPush[m_syncIndex];
        for (i = 0 ; i < pushes.size() ; i++)
        {
            GenericFifo *fifo = pushes[i];
            if (fifo->target & TRIGGER_ITRIGGER)
            {
                GenericTrigger *target = (GenericTrigger*) ((fifo->target) - TRIGGER_ITRIGGER);
                const byte *data = GENERIC_FIFO_DATA(fifo) + fifo->head;
                target->trigger(*data);

                // Automatically pop the data from the fifo
                if (!fifo->head)
                    fifo->head = fifo->size;
                fifo->head -= fifo->dataSize;
                fifo->freeCount++;
            }
            else if (!fifo->fullCount++)
                ((Component *) fifo->target)->activate();
        }
        pushes.clear();

        // Fifo pop
        stack<GenericFifo *> &pops = m_syncFifoPop[m_syncIndex];
        for (i = 0 ; i < pops.size() ; i++)
            pops[i]->freeCount++;
        pops.clear();

        // Synchronous triggers
        TriggerStack &triggers = m_syncTriggers[m_syncIndex];
        for (i = 0 ; i < triggers.size() ; i++)
            TRIGGER_ACTIVATE_TARGET(triggers[i].trigger->target, &triggers[i].val);
        triggers.clear();
    }

    // Call tick() for all wave fifos (captures state following scheduled fifo events to 
    // correctly dump fifos with delay).
    for (WavesFifo *f = m_waveFifos ; f ; f = f->next)
        f->tick();

    // Dump RegQ waves (need to do this before update() so that fake registers get 
    // dumped correctly).
    dumpRegQs();

    if (m_numTicks++)
    {
        m_prevTick = m_nextEdge;
        m_prevIndex++;
    }
}

/////////////////////////////////////////////////////////////////
//
// schedulePush()
//
/////////////////////////////////////////////////////////////////
void ClockDomain::schedulePush (GenericFifo *fifo)
{
    int index = (m_syncIndex + fifo->delay) & m_syncMask;
    m_syncFifoPush[index].push(fifo);
}

/////////////////////////////////////////////////////////////////
//
// schedulePop()
//
/////////////////////////////////////////////////////////////////
void ClockDomain::schedulePop (GenericFifo *fifo)
{
    int index = (m_syncIndex + fifo->delay) & m_syncMask;
    m_syncFifoPop[index].push(fifo);
}

////////////////////////////////////////////////////////////////////////
//
// doAcross()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::doAcross (void (ClockDomain::*func) ())
{
    ClockDomain *domainList;
    for (domainList = s_first ; domainList ; domainList = domainList->m_nextDifferentTick)
    {
        for (ClockDomain *domain = domainList ; domain ; domain = domain->m_nextSameTick)
            (domain->*func)();
    }
    for (domainList = s_firstManual ; domainList ; domainList = domainList->m_nextDifferentTick)
    {
        for (ClockDomain *domain = domainList ; domain ; domain = domain->m_nextSameTick)
            (domain->*func)();
    }
}

////////////////////////////////////////////////////////////////////////
//
// doAcross()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::doAcross (void (PortStorage::*func) ())
{
    ClockDomain *domainList;
    for (domainList = s_first ; domainList ; domainList = domainList->m_nextDifferentTick)
    {
        for (ClockDomain *domain = domainList ; domain ; domain = domain->m_nextSameTick)
            (domain->m_ports.*func)();
    }
    for (domainList = s_firstManual ; domainList ; domainList = domainList->m_nextDifferentTick)
    {
        for (ClockDomain *domain = domainList ; domain ; domain = domain->m_nextSameTick)
            (domain->m_ports.*func)();
    }
}

////////////////////////////////////////////////////////////////////////
//
// initialize()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::initialize ()
{
    // Create the threads
    initThreads();

    // Resolve the clock period and schedule the clock domain
    doAcross(&ClockDomain::resolvePeriod);
    ClockDomain *domain = s_first;
    s_first = NULL;
    while (domain)
    {
        ClockDomain *next = domain->m_nextDifferentTick;
        domain->scheduleClockDomain();
        domain = next;
    }

    // Now that the update wrappers have been sorted, we can initialize
    // the ports (we needed to sort the update wrappers first in order
    // to finalize the set of fake registers).
    logInfo("Initializing ports...\n");
    doAcross(&ClockDomain::initPorts);
    doAcross(&PortStorage::finalizeCopies);

    // Once the ports have been initialized we can create the update array
    // (we need to initialize the ports first in order to set the 
    // value pointers which are needed for trigger evaluation).
    logInfo("Writing update array...\n");
    doAcross(&ClockDomain::createUpdateArray);
}

////////////////////////////////////////////////////////////////////////////////
//
// initializeGeneratorParams()
//
// If the ratio is rational, then initialize (a, b, m0, n0, k) as
// described in the header file comments.
//
////////////////////////////////////////////////////////////////////////////////
void ClockDomain::initializeGeneratorParams ()
{
    // Try to express the ratio as a/b
    for (int b = 1 ; b < 64 ; b++)
    {
        int a = m_clockRatio * b + 0.5;
        if (m_clockRatio == (float) ((float) a / (float) b) ||
            m_clockRatio == (float) ((float) a / (double) b) ||
            m_clockRatio == (float) ((double) a / (float) b) ||
            m_clockRatio == (float) ((double) a / (double) b))
        {
            m_gen_a = a;
            m_gen_b = b;
            m_gen_k = m_clockOffset;
            m_gen_m = 0;

            while (m_gen_k > m_generator->m_period / 2)
            {
                m_gen_k -= m_generator->m_period;
                m_gen_m++;
            }
            while (m_gen_k < -m_generator->m_period / 2)
            {
                m_gen_k += m_generator->m_period;
                m_gen_m--;
            }
            return;
        }
    }
}

/////////////////////////////////////////////////////////////////
//
// resolvePeriod()
//
/////////////////////////////////////////////////////////////////
void ClockDomain::resolvePeriod ()
{
    if (m_resolvedPeriod)
        return;
    m_resolvedPeriod = true;
    if (m_dividedClock)
    {
        m_generator = m_dividedClock->resolveClockDomain();
        m_generator->resolvePeriod();
        m_period = (int) (m_generator->getPeriod() * m_clockRatio);
        if (m_period)
            initializeGeneratorParams();
        if (m_generator->m_dividedClock)
        {
            m_clockRatio *= m_generator->m_clockRatio;
            m_clockOffset += m_generator->m_clockOffset;
            m_dividedClock = m_generator->m_dividedClock;
        }
    }
    if (m_period)
    {
        m_prevTick = m_clockOffset;
        for (m_prevIndex = 0 ; m_prevTick < 0 ; m_prevIndex++)
            m_prevTick = getNextTick(m_prevIndex + 1, m_prevTick);
        m_nextEdge = m_prevTick;
    }
}

/////////////////////////////////////////////////////////////////
//
// resetDomains()
//
/////////////////////////////////////////////////////////////////
static void resetPendingFifos (stack<GenericFifo *> &fifos)
{
    // Drop pending fifo events for any fifos that have been reset.
    for (int i = 0 ; i < fifos.size() ; i++)
    {
        if (fifos[i]->head == 0 && 
            fifos[i]->tail == 0 && 
            fifos[i]->fullCount == 0 && 
            fifos[i]->freeCount == (fifos[i]->size / fifos[i]->dataSize))
        {
            GenericFifo *f = fifos.pop();
            if (i < fifos.size())
                fifos[i--] = f;
        }
    }
}

void ClockDomain::resetDomains ()
{
    doAcross(&ClockDomain::resetDomain);
}

void ClockDomain::resetDomain ()
{
    for (int i = 0 ; i < m_syncDepth ; i++)
    {
        resetPendingFifos(m_syncFifoPush[i]);
        resetPendingFifos(m_syncFifoPop[i]);
        m_syncTriggers[i].clear();
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// resetPorts()
//
////////////////////////////////////////////////////////////////////////////////
void ClockDomain::resetPorts ()
{
    doAcross(&PortStorage::postTick);
}

////////////////////////////////////////////////////////////////////////////////
//
// resetEvents()
//
////////////////////////////////////////////////////////////////////////////////
void ClockDomain::resetEvents ()
{
    doAcross(&ClockDomain::resetEventsInternal);
}
void ClockDomain::resetEventsInternal ()
{
    for_map_values (IEvent *event, m_events)
        delete event;
    m_events.clear();
}

////////////////////////////////////////////////////////////////////////////////
//
// propagateReset()
//
////////////////////////////////////////////////////////////////////////////////
void ClockDomain::propagateReset ()
{
    doAcross(&PortStorage::propagateReset);
}

/////////////////////////////////////////////////////////////////
//
// resetTriggers()
//
/////////////////////////////////////////////////////////////////

static bool s_isReset = false;

void ClockDomain::resetTriggers (bool isReset)
{
    s_isReset = isReset;
    doAcross(&ClockDomain::resetTriggersInternal);
}

void ClockDomain::resetTriggersInternal ()
{
    // Synchronous triggers are normally placed on a pending-activation list
    // following component updates, but at initialization time the values can
    // be written directly instead of written by a component, so we need to 
    // explicitly check for initially active synchronous triggers.  Specifically,
    // suppose there is a synchronous connection from A to B.  Then on the first
    // clock cycle, the input port on B will see the reset value from A.  If the
    // port is read by B::update() then this value will be observed, buf if it's
    // read by a trigger function then by default it won't because A's output was
    // set during reset rather than during update.  So, for this case, manually
    // fire the trigger once to ensure that this initial value is "observed".
    //
    // Also make sure the state of the sticky triggers is consistent.  Do this the
    // easy way by just putting everything in the set and setting all of the
    // 'active' bits.  (The slightly harder way is to actually check the
    // current values, but there's not really much benefit since everything
    // gets cleaned up on the next rising clock edge).
    byte *curr = m_updates;
    byte *end = m_updates + m_updateSize;
    while (curr < end)
    {
        const S_Update *update = (const S_Update *) curr;
        curr += sizeof(S_Update);
        for (int i = 0 ; i < update->numTriggers ; i++, curr += sizeof(S_Trigger))
        {
            S_Trigger *trigger = (S_Trigger *) curr;

            // Manually activate initially-active triggers
            if (trigger->delay)
                resetSyncTrigger(trigger);

            // Place all latch triggers in the sticky list to force evaluation on the
            // next update.
            if (trigger->latch)
            {
                trigger->active = 1;
                m_stickyTriggers.insert(trigger);
            }
        }
    }
}

void ClockDomain::resetSyncTrigger (S_Trigger *trigger)
{
    if (!s_isReset)
        return;

    CascadeValidate(trigger->size <= sizeof(intptr_t), "Invalid size for synchronous trigger");
    int offset = trigger->value - m_ports.m_portData;

    for (int i = 0 ; i < trigger->delay ; i++)
    {
        const byte *value = m_ports.m_portData + offset + m_ports.m_delayOffset[i];

        bool zero = true;
        for (unsigned j = 0 ; j < trigger->size ; j++)
        {
            if (value[j])
            {
                zero = false;
                break;
            }
        }

        if (zero == (bool) trigger->activeLow)
        {
            TriggerStack &triggers = m_syncTriggers[(m_syncIndex + trigger->delay - i) & m_syncMask];
            memcpy(triggers.push(trigger), value, trigger->size);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// initPorts()
//
////////////////////////////////////////////////////////////////////////////////
void ClockDomain::initPorts ()
{
    //
    // Assign triggers to update wrappers.  A trigger is assigned to the last
    // update wrapper that writes the associated port with three exceptions:
    //
    // 1. Cross-domain (slow/patched) synchronous triggers.
    //
    // 2. Synchronous triggers with delay > 255 or size > sizeof(intptr_t)
    //
    // 3. Triggers with no known writers, which are assumed to be written by an
    //    external agent prior to the rising clock edge
    //
    // These triggers are assigned to the head sentinel so that they are evaluated
    // before combinational update.
    //
    for (PortList::Remover it = m_portWrappers ; it ; it++)
    {
        PortWrapper *port = *it;
        if (!port->isFifo() && !port->triggers.empty())
        {
            PortWrapper *source = (port->connection == PORT_SYNCHRONOUS) ? port->connectedTo : port;
            if (port->connection == PORT_SLOWQ || port->connection == PORT_PATCHED || port->delay > 255 || source->writers.empty() ||
                (port->delay && (port->size > sizeof(intptr_t))))
            {
                m_updateSentinel.triggers.push(port);
            }
            else
            {
                // Find the last writer for this trigger
                int lastWriterIndex = -1;
                UpdateWrapper *lastWriter = NULL;
                for (int i = 0 ; i < source->writers.size() ; i++)
                {
                    if (source->writers[i]->index > lastWriterIndex)
                    {
                        lastWriter = source->writers[i];
                        lastWriterIndex = lastWriter->index;
                    }
                }
                lastWriter->triggers.push(port);
                if (port->delay > m_syncDepth)
                    m_syncDepth = port->delay;
            }
        }

        // Pass the port on to port storage for further initialization
        if (port->isFifo() || port->connection != PORT_WIRED)
            m_ports.addPort(port);
    }

    // Initialize the port storage
    m_ports.initPorts(this);
}

////////////////////////////////////////////////////////////////////////
//
// registerTickableComponent()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::registerTickableComponent (Component *c)
{
    m_tickableComponents.push_back(c);
}

////////////////////////////////////////////////////////////////////////
//
// registerVerilogClock()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::registerVerilogClock (vpiHandle port)
{
    m_verilogClocks.push(port);
}

////////////////////////////////////////////////////////////////////////
//
// registerUpdateFunction()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::registerUpdateFunction (UpdateWrapper *update)
{
    update->next = m_updateWrappers;
    m_updateWrappers = update;
}

////////////////////////////////////////////////////////////////////////
//
// sortUpdateFunctions()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::sortUpdateFunctions ()
{
    // Sort the update functions
    m_updateWrappers = UpdateFunctions::sort(m_updateWrappers);

    // Mark the update functions with their sorted order
    int index = 0;
    for (UpdateWrapper *w = m_updateWrappers ; w ; w = w->next)
        w->index = index++;
}

////////////////////////////////////////////////////////////////////////
//
// createUpdateArray()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::createUpdateArray ()
{
    // Round m_syncDepth up to a power of two and set the sync mask
    if (m_syncDepth)
    {
        int pow2;
        for (pow2 = 1 ; pow2 < m_syncDepth ; pow2 *= 2);
        m_syncDepth = pow2;
        m_syncMask = pow2 - 1;
    }

    // Add the head sentinel to the list if necessary
    UpdateWrapper *firstUpdate = m_updateWrappers;
    if (m_updateSentinel.triggers.size())
    {
        firstUpdate = &m_updateSentinel;
        m_updateSentinel.next = m_updateWrappers;
    }

    // We can allocate the vector of synchronous trigger/fifo lists now
    m_syncTriggers = new TriggerStack[m_syncDepth];
    m_syncFifoPush = new stack<GenericFifo *>[m_syncDepth];
    m_syncFifoPop = new stack<GenericFifo *>[m_syncDepth];

    // Now that triggers have been assigned we can compute the size of
    // every update and trigger structure
    setUpdateOffsets(firstUpdate);

    // Finally, create and write the update array
    Sim::stats.numUpdateBytes += m_updateSize;
    m_updates = new byte[m_updateSize];
    writeUpdates(firstUpdate);

    // Add a sentinel to the end of the sticky triggers to simplify the 
    // loop that iterates over sticky triggers up to a certain point.
    m_stickyTriggers.insert((S_Trigger *) (m_updates + m_updateSize));
}

void ClockDomain::setUpdateOffsets (UpdateWrapper *w)
{
    m_updateSize = 0;
    for ( ; w ; w = w->next)
    {
        w->offset = m_updateSize;
        m_updateSize += sizeof(S_Update);
        for (int i = 0 ; i < w->triggers.size() ; i++)
            m_updateSize += w->triggers[i]->triggers.size() * sizeof(S_Trigger);
    }
}

void ClockDomain::writeUpdates (UpdateWrapper *w)
{
    byte *dst = m_updates;
    for ( ; w ; w = w->next)
    {
        S_Update *update = (S_Update *) dst;
        update->component = w->component;
        update->numTriggers = 0;
        update->fn = w->update ? w->update : &Component::update;
        dst += sizeof(S_Update);
        update->numTriggers = writeTriggers(w, dst);
    }

    CascadeValidate(dst == m_updates + m_updateSize, "Update array size mismatch");
}

int ClockDomain::writeTriggers (UpdateWrapper *w, byte *&dst)
{
    int numTriggers = 0;
    for (int i = 0 ; i < w->triggers.size() ; i++)
    {
        PortWrapper *port = w->triggers[i];
        PortWrapper *source = port;
        bool singleWriter = (port->writers.size() == 1);

        // For regular update functions, resolve the source across a single synchronous connection
        // and set the delay.  For the sentinel (component == NULL), don't resolve the source because 
        // the triggers will be evaluated on every clock cycle.  
        int delay = 0;
        if (w->component && (port->connection == PORT_SYNCHRONOUS))
        {
            source = port->connectedTo;

            // If the port delay is 0 then it's a fake register, so the trigger delay should be 1
            delay = port->delay ? port->delay : 1;
        }

        bool latch = (source->type == PORT_LATCH) || (source->connection == PORT_WIRED);
        bool sticky = (source->writers.size() > 1) || latch;
        latch &= singleWriter;

        numTriggers += port->triggers.size();
        for (int j = 0 ; j < port->triggers.size() ; j++, dst += sizeof(S_Trigger))
        {
            S_Trigger *trigger = (S_Trigger *) dst;
            bool activeLow = port->triggers[j].activeLow;
            bool fast = (port->size == 1) && !activeLow && !latch;

            // The trigger needs to be evaluated on every cycle after it becomes
            // active if any of the following hold:
            //
            //   1. It has multiple writers
            //   2. The port is a LATCH port
            //   3. The port is wired to a variable
            //   4. The port is a Pulse port and the trigger is active low
            //
            // The 'latch' flag indicates case (2) or (3) with a single writer, and
            // allows the port to be removed from m_stickyTriggers after it becomes
            // inactive.  For cases (1) and (4) the port is always in m_stickyTriggers
            // and is evaluated on every cycle.
            //
            if (sticky || (activeLow && (source->type == PORT_PULSE)))
                m_stickyTriggers.insert(trigger);

            trigger->value = source->port->value;
            trigger->size = port->size;
            trigger->fast = fast;
            trigger->delay = delay;
            trigger->activeLow = activeLow;
            trigger->latch = latch;
            trigger->active = 0;
            trigger->target = port->triggers[j].target;
        }
    }
    return numTriggers;
}

////////////////////////////////////////////////////////////////////////////////
//
// Waves
//
////////////////////////////////////////////////////////////////////////////////
void ClockDomain::addWavesSignal (Cascade::WavesSignal *s)
{
    s->next = m_waveSignals;
    m_waveSignals = s;
}
void ClockDomain::addWavesRegQ (Cascade::WavesSignal *s)
{
    s->next = m_waveRegQs;
    m_waveRegQs = s;
}
void ClockDomain::addWavesClock (Cascade::WavesSignal *s)
{
    s->next = m_waveClocks;
    m_waveClocks = s;
}
void ClockDomain::addWavesFifo (Cascade::WavesFifo *f)
{
    f->next = m_waveFifos;
    m_waveFifos = f;
}
void ClockDomain::addGlobalWavesSignal (Cascade::WavesSignal *s)
{
    s->next = s_globalWaves;
    s_globalWaves = s;
}
void ClockDomain::dumpWaves ()
{
    if (!(m_numEdges & 1))
    {
        dumpClocks();
        return;
    }
    Cascade::WavesSignal *s;
    for (s = m_waveSignals ; s ; s = s->next)
        s->dump();
    for (s = s_globalWaves ; s ; s = s->next)
        s->dump();
    Cascade::WavesFifo *f;
    for (f = m_waveFifos ; f ; f = f->next)
        f->update();
}
void ClockDomain::dumpRegQs ()
{
    dumpClocks();
    for (Cascade::WavesSignal *s = m_waveRegQs ; s ; s = s->next)
        s->dump();
}
void ClockDomain::dumpClocks ()
{
    for (Cascade::WavesSignal *s = m_waveClocks ; s ; s = s->next)
        s->dump();
}

////////////////////////////////////////////////////////////////////////////////
//
// Verilog clocks
//
////////////////////////////////////////////////////////////////////////////////
void ClockDomain::driveVerilogClocks ()
{
#ifdef _VERILOG
    for (int i = 0 ; i < m_verilogClocks.size() ; i++)
    {
        s_vpi_value value;
        value.format = vpiVectorVal;
        s_vpi_vecval vecval = { m_numEdges & 1, 0 };
        value.value.vector = &vecval;    
        vpi_put_value(m_verilogClocks[i], &value, NULL, vpiForceFlag);
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////
//
// findOwner()
//
////////////////////////////////////////////////////////////////////////////////
ClockDomain *ClockDomain::findOwner (const byte *data)
{
    // Take advantage of coherence to speed this up
    static ClockDomain *prevOwner = NULL;

    if (prevOwner && prevOwner->m_ports.isOwner(data))
        return prevOwner;

    // Look in the automatically scheduled clock domains
    ClockDomain *c;
    for (c = s_first ; c ; c = c->m_nextDifferentTick)
    {
        for (ClockDomain *c1 = c ; c1 ; c1 = c1->m_nextSameTick)
        {
            if (c1->m_ports.isOwner(data))
                return prevOwner = c1;
        }
    }

    // Look in the manually scheduled clock domains
    for (c = s_firstManual ; c ; c = c->m_nextDifferentTick)
    {
        for (ClockDomain *c1 = c ; c1 ; c1 = c1->m_nextSameTick)
        {
            if (c1->m_ports.isOwner(data))
                return prevOwner = c1;
        }
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////
//
// updateNextTick()
//
/////////////////////////////////////////////////////////////////

// If we're really close to an even number of nanoseconds in absolute time or relative
// to our clock offset then round
uint64 ClockDomain::roundTime (uint64 time)
{
    // Try rounding to absolute time
    uint64 round = ((time + 500) / 1000) * 1000;
    if ((time <= round + params.ClockRounding) && (time >= round - params.ClockRounding))
        return round;

    // Try rounding to relative time
    round = ((time - m_clockOffset + 500) / 1000) * 1000 + m_clockOffset;
    if ((time <= round + params.ClockRounding) && (time >= round - params.ClockRounding))
        return round;

    return time;
}

void ClockDomain::updateNextEdge ()
{
    if (m_numEdges & 1)
        m_nextEdge = roundTime(m_nextEdge + m_period / 2);
    else
        m_nextEdge = getNextTick(m_prevIndex + 1, m_prevTick);
}

int64 ClockDomain::getTick (int index)
{
    assert_always(index >= m_prevIndex, "Attempted to offset from a stale rising clock edge");
    int64 ret = m_prevTick;
    for (int i = m_prevIndex + 1 ; i <= index ; i++)
        ret = getNextTick(i, ret);
    return ret;
}

int64 ClockDomain::getNextTick (int index, int64 prevRisingEdge)
{
    if (m_generator)
    {
        int n = index / m_gen_b;
        if (index == n * m_gen_b)
        {
            if (m_generator->m_numTicks + m_gen_m >= 0)
                return m_generator->getTick(n * m_gen_a + m_gen_m) + m_gen_k;
        }
    }
    int64 nextEdge = roundTime(prevRisingEdge + m_period / 2);
    return roundTime(nextEdge + m_period - m_period / 2);
}

////////////////////////////////////////////////////////////////////////
//
// scheduleClockDomain()
//
// Re-insert the clock domain into the global linked list.  For manual
// clock domains, this is only called at initialize or when the simulation
// is loaded from an archive.
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::scheduleClockDomain ()
{
    if (m_period)
    {
        // Insert into the top-level linked list; sort by time
        ClockDomain **ppDomain = &s_first;
        for ( ; *ppDomain && ((*ppDomain)->m_nextEdge + params.ClockRounding < m_nextEdge) ; 
            ppDomain = &((*ppDomain)->m_nextDifferentTick));

        // If there is an existing second-level linked list with the appropriate next Tick time
        // then append the clock domain to this list.  Otherwise, create a new second-level list
        // and add it to the top-level list.
        if (*ppDomain && ((*ppDomain)->m_nextEdge <= m_nextEdge + params.ClockRounding))
        {
            // Existing list - insert clock domain
            (*ppDomain)->m_lastSameTick->m_nextSameTick = this;
            (*ppDomain)->m_lastSameTick = this;
        }
        else
        {
            // Insert a new list
            m_nextDifferentTick = *ppDomain;
            *ppDomain = this;
            m_lastSameTick = this;
        }
        m_nextSameTick = NULL;
    }
    else
    {
        // If this clock domain is dividing the clock of a manually-scheduled domain,
        // then add it to the nextSameTick list of that domain.
        CascadeValidate(m_dividedClock, "Clock domain has no period and no generator");
        CascadeValidate(Sim::state == Sim::SimInitializing, 
            "scheduleClockDomain() should only be called during initialization for divider of manual clock");
        ClockDomain *generator = m_dividedClock->resolveClockDomain();
        while (generator->m_dividedClock)
            generator = generator->m_dividedClock->resolveClockDomain();
        m_nextSameTick = generator->m_nextSameTick;
        generator->m_nextSameTick = this;
    }
}

/////////////////////////////////////////////////////////////////
//
// scheduleEvent()
//
/////////////////////////////////////////////////////////////////
void ClockDomain::scheduleEvent (int delay, IEvent *event)
{
    int ticks = m_numTicks + delay;

    // If this is called from reset(), then check to make sure we're not duplicating an event
    if (Sim::state == Sim::SimResetting)
    {
        for (Iterator<EventMap> it(m_events, ticks, ticks) ; it ; it++)
        {
            if (event->equals(*it))
            {
                delete event;
                return;
            }
        }
    }
    m_events.insert(EventMap::value_type(ticks, event));
}

////////////////////////////////////////////////////////////////////////
//
// archiveClockDomains()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::archiveClockDomains (Archive &ar)
{
    int numDomains = 0;
    if (ar.isLoading())
    {
        // Create an array of clock domains so that we can reference clock domain pointers by id.
        ClockDomain **domains = new ClockDomain * [s_numClockDomains];
        ClockDomain *d1;
        for (d1 = s_first ; d1 ; d1 = d1->m_nextDifferentTick)
        {
            for (ClockDomain *d2 = d1 ; d2 ; d2 = d2->m_nextSameTick, numDomains++)
                domains[d2->m_id] = d2;
        }
        for (d1 = s_firstManual ; d1 ; d1 = d1->m_nextDifferentTick)
        {
            for (ClockDomain *d2 = d1 ; d2 ; d2 = d2->m_nextSameTick, numDomains++)
                domains[d2->m_id] = d2;
        }

        // Clear the schedule
        s_first = NULL;

        // Archive the domains
        for (int i = 0 ; i < s_numClockDomains ; i++)
        {
            int id;
            ar | id;
            domains[id]->archive(ar);
            if (domains[id]->m_period)
                domains[id]->scheduleClockDomain();
        }

        // Delete the temporary array
        delete[] domains;
    }
    else
    {
        // Archive the clock domains
        ClockDomain *d1;
        for (d1 = s_first ; d1 ; d1 = d1->m_nextDifferentTick)
        {
            for (ClockDomain *d2 = d1 ; d2 ; d2 = d2->m_nextSameTick, numDomains++)
            {
                ar | d2->m_id;
                d2->archive(ar);
            }
        }
        for (d1 = s_firstManual ; d1 ; d1 = d1->m_nextDifferentTick)
        {
            for (ClockDomain *d2 = d1 ; d2 ; d2 = d2->m_nextSameTick, numDomains++)
            {
                ar | d2->m_id;
                d2->archive(ar);
            }
        }
    }
    CascadeValidate(numDomains == s_numClockDomains, "Somebody dropped a clock domain");
}

////////////////////////////////////////////////////////////////////////
//
// archive()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::archive (Archive &ar)
{
    // Make sure the period hasn't changed since the archive was created
    int period = m_period;
    ar | period;
    assert_always(period == m_period, "Clock period (%d) does not match archive clock period (%d)", m_period, period);

    // Archive tick state
    ar | m_nextEdge | m_numTicks | m_numEdges | m_prevIndex | m_prevTick;

    // Archive ports
    m_ports.archive(ar);

    // Make sure the state of the sticky triggers is consistent
    resetTriggers(false);

    // Archive fifo events scheduled for a future rising clock edge
    ar | m_syncIndex;
    for (int i = 0 ; i < m_syncDepth ; i++)
    {
        m_ports.archiveFifoStack(ar, m_syncFifoPush[i]);
        m_ports.archiveFifoStack(ar, m_syncFifoPop[i]);
    }

    // A WavesFifo needs to know how many in-flight pushes there are, so the consumer
    // side of a fifo with delay can set its tail pointer properly.  So if we're loading 
    // then temporarily record the number of in-flight pushes in fullCount, then copy 
    // this information to the WavesFifos, then archive the actual Fifo data.  The
    // Fifo fullCounts were reset to zero in PortStorage::archive()
    if (ar.isLoading())
    {
        // Count the in-flight pushes
        for (int i = 0 ; i < m_syncDepth ; i++)
        {
            for (int j = 0 ; j < m_syncFifoPush[i].size() ; j++)
                m_syncFifoPush[i][j]->fullCount++;
        }

        // Copy this information to the waves
        for (WavesFifo *wf = m_waveFifos ; wf ; wf = wf->next)
            wf->archiveFullCount();
    }

    // Archive Fifos
    m_ports.archvieFifos(ar);

    // Archive triggers scheduled for a future rising clock edge
    for (int i = 0 ; i < m_syncDepth ; i++)
    {
        TriggerStack &triggers = m_syncTriggers[i];
        if (ar.isLoading())
        {
            int size;
            ar | size;
            triggers.resize(size);
            for (int i = 0 ; i < size ; i++)
            {
                int offset;
                ar | offset;
                triggers[i].trigger = (S_Trigger *) (m_updates + offset);
                ar | triggers[i].val;
            }
        }
        else
        {
            int size = triggers.size();
            ar | size;
            for (int i = 0 ; i < size ; i++)
            {
                unsigned offset = ((byte *) triggers[i].trigger) - m_updates;
                ar | offset | triggers[i].val;
            }
        }
    }

    // Archive events
    if (ar.isLoading())
        resetEvents();
    ar | m_events;
}

////////////////////////////////////////////////////////////////////////
//
// runSimulation()
//
// Run the simulation until a specified time in picoseconds, 
// or for a single tick() event if runUntil is zero.
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::runSimulation (uint64 runUntil)
{
    // Automatically initialize the simulation if it has not been initialized
    if (Sim::state != Sim::SimInitialized)
        Sim::init();

    // In a Verilog-driven simulations there might not be any scheduled clock domains
    if (Sim::isVerilogSimulation && !s_first)
    {
        Sim::simTime = runUntil;
        return;
    }

    assert_always(s_first, "No scheduled clock domains: cannot run simulation");

    bool runSingleTick = (runUntil == 0);
    if (runSingleTick)
        runUntil = (uint64) 0x7fffffffffffffffLL;

    while (s_first->m_nextEdge < (int64) runUntil)
    {
        CascadeValidate((int64) Sim::simTime <= s_first->m_nextEdge, "Simulation went backwards in time");
        Sim::simTime = s_first->m_nextEdge;
        assert_always(!params.Timeout || (Sim::simTime < uint64(params.Timeout) * 1000), "Simulation timed out");
        if (params.Finish && (Sim::simTime >= uint64(params.Finish) * 1000))
        {
#ifdef _VERILOG
            if (Sim::isVerilogSimulation)
                tf_dofinish();
            else
#endif
                exit(0);
        }

        // Checkpoints
        if (Sim::simTime >= Sim::nextCheckpoint)
        {
            SimArchive::saveSimulation(*str("%s_%u.ckp", params.CheckpointName->c_str(), (unsigned) (s_first->m_nextEdge / 1000)), params.SafeCheckpoint);
            if (params.CheckpointInterval)
                Sim::nextCheckpoint += params.CheckpointInterval * 1000;
            else
                Sim::nextCheckpoint = (uint64) 0x7fffffffffffffffLL;
        }

        // Strip off the first list of clock domains (which have the same nextTick time)
        ClockDomain *runList = s_first;
        s_first = s_first->m_nextDifferentTick;
        Sim::simTicks++;
        Sim::tracing = (Sim::simTime >= 1000 * params.TraceStartTime && Sim::simTime <= 1000 * params.TraceStopTime);

        // Tick the domains
        tickDomains(runList);

        // Check to see if there was a rising edge (don't count falling edges when
        // we're running for a single tick).
        bool risingEdge = false;
        for (ClockDomain *c = runList ; c ; c = c->m_nextSameTick)
            risingEdge |= ((c->m_numEdges & 1) != 0);

        // Reschedule the clock domains
        while (runList)
        {
            ClockDomain *domain = runList;
            runList = domain->m_nextSameTick;
            domain->updateNextEdge();
            domain->scheduleClockDomain();
        }

        if (runSingleTick && (risingEdge || Sim::verilogCallbackPump))
        {
            runUntil = s_first->m_nextEdge;
            break;
        }
    }
    Sim::simTime = runUntil;
}

/////////////////////////////////////////////////////////////////
//
// tickDomains()
//
/////////////////////////////////////////////////////////////////

#define TIMESTAT(stat) \
    t2 = getTimer(); \
    Sim::stats.stat += t2 - t1; \
    t1 = t2

void ClockDomain::tickDomains (ClockDomain *runList)
{
    uint64 t1, t2;

    // Once every 10 seconds, check for non-empty queues feeding
    // deactivated components (indicates a deactivation bug).
    if (time(NULL) >= s_lastDeadlockCheckTime + 10)
    {
        doAcross(&PortStorage::checkDeadlock);
        s_lastDeadlockCheckTime = time(NULL);
    }

    t1 = getTimer();

    // Update the edge count and drive verilog from the main thread
    for (ClockDomain *c = runList ; c ; c = c->m_nextSameTick)
    {
        c->m_numEdges++;
        c->driveVerilogClocks();
    }

    // Tick the clock domains
    runThreaded(runList, &ClockDomain::preTick);
    TIMESTAT(preTickTime);

    runThreaded(runList, &ClockDomain::tick);
    TIMESTAT(tickTime);

    // Synchronous events; invalidate ports; zero pulse ports
    runThreaded(runList, &ClockDomain::postTick);
    TIMESTAT(postTickTime);

    // Update the clock domains
    runThreaded(runList, &ClockDomain::update);
    TIMESTAT(updateTime);

    // Dump waves
    runThreaded(runList, &ClockDomain::dumpWaves);
}

/////////////////////////////////////////////////////////////////
//
// manualTick()
//
/////////////////////////////////////////////////////////////////
void ClockDomain::manualTick ()
{
    assert_always(!m_period, "Cannot manually tick a scheduled clock domain");

    // The first time a manual domain is ticked, set the offset for the domain
    // and all of its generated domains, and tick all of the domains with a
    // zero or negative offset (that's all we can do since we don't yet know what
    // the period of the manual domain looks like).
    if (!m_numEdges)
    {
        // Compute m_clockOffset and use this to sort the clock domains
        ClockDomain *ticks = NULL;
        for (ClockDomain *c = this ; c ; )
        {
            ClockDomain *next = c->m_nextSameTick;

            c->m_clockOffset += (int) Sim::simTime;
            ClockDomain **ppc;
            for (ppc = &ticks ; *ppc && ((*ppc)->m_clockOffset < c->m_clockOffset) ; ppc = &(*ppc)->m_nextSameTick);
            c->m_nextSameTick = *ppc;
            *ppc = c;

            c = next;
        }

        // Tick any domains with clock offset in [0, currTime]
        ClockDomain *nextTick = ticks;
        while (nextTick && nextTick->m_clockOffset <= m_clockOffset + (int) params.ClockRounding)
        {
            // Figure out how many domains in a row we're ticking
            ClockDomain **ppc;
            for (ppc = &nextTick->m_nextSameTick ; 
                *ppc && (*ppc)->m_clockOffset <= ticks->m_clockOffset + (int) params.ClockRounding ; 
                ppc = &(*ppc)->m_nextSameTick);
            ClockDomain *temp = *ppc;
            *ppc = NULL;

            // Tick the domains
            if (nextTick->m_clockOffset >= 0)
                tickDomains(nextTick);

            nextTick = *ppc = temp;
        }

        // Remove 'this' from the list and restore this->m_nextSameTick
        ClockDomain **ppc;
        for (ppc = &ticks ; *ppc != this ; ppc = &(*ppc)->m_nextSameTick);
        *ppc = m_nextSameTick;
        m_nextSameTick = ticks;

        return;
    }

    // Compute the effective period of this clock domain
    int64 currTime = Sim::simTime;
    double period = (double) (currTime  - m_clockOffset) / (double) m_numTicks;
    CascadeValidate((m_numEdges & 1), "Manual clock domain is in invalid state");

    // Initialize a list of edge events in order
    ClockDomain *edges = NULL;
    ClockDomain *c = this;
    while (c)
    {
        ClockDomain *next = c->m_nextSameTick;

        // Compute the next edge
        double dividedPeriod = period * c->m_clockRatio;
        c->m_nextEdge = c->roundTime(c->m_clockOffset + (uint64) (dividedPeriod / 2.0 * c->m_numEdges));

        // Add to the list of edges
        ClockDomain **ppc;
        for (ppc = &edges ; *ppc && ((*ppc)->m_nextEdge < c->m_nextEdge) ; ppc = &(*ppc)->m_nextSameTick);
        c->m_nextSameTick = *ppc;
        *ppc = c;

        c = next;
    }

    // Now process events until we hit the rising clock edge of this domain
    Sim::simTime = edges->m_nextEdge;
    while (edges->m_nextEdge <= currTime + (int) params.ClockRounding)
    {
        assert_always(!params.Timeout || (Sim::simTime < uint64(params.Timeout) * 1000), "Simulation timed out");
        bool positiveTime = (edges->m_nextEdge >= 0);

        // Figure out how many domains in a row we're ticking
        ClockDomain **ppc;
        for (ppc = &edges->m_nextSameTick ;
            *ppc && (*ppc)->m_nextEdge <= edges->m_nextEdge + (int) params.ClockRounding ;
            ppc = &(*ppc)->m_nextSameTick);
        c = edges;
        edges = *ppc;
        *ppc = NULL;

        // Tick the domains
        if (positiveTime)
            tickDomains(c);
        else
        {
            for (ClockDomain *d = c ; d ; d = d->m_nextSameTick)
                d->m_numEdges += d->m_numEdges ? 2 : 1;
        }

        // Put the domain backs in the sorted list
        while (c)
        {
            ClockDomain *next = c->m_nextSameTick;

            // Compute the next edge; ignore falling edges if we have no clocks
            double dividedPeriod = period * c->m_clockRatio;
            if ((c == this) && !(m_numEdges & 1))
                c->m_nextEdge = currTime;
            else
                c->m_nextEdge = c->roundTime(c->m_clockOffset + (uint64) (dividedPeriod / 2.0 * c->m_numEdges));

            ClockDomain **ppc;
            for (ppc = &edges ; *ppc && ((*ppc)->m_nextEdge < c->m_nextEdge) ; ppc = &(*ppc)->m_nextSameTick);
            c->m_nextSameTick = *ppc;
            *ppc = c;

            c = next;
        }

        if (edges->m_nextEdge > (int64) Sim::simTime + params.ClockRounding)
            Sim::simTime = edges->m_nextEdge;
    }
    Sim::simTime = currTime;

    // Remove 'this' from the list and restore this->m_nextSameTick
    ClockDomain **ppc;
    for (ppc = &edges ; *ppc != this ; ppc = &(*ppc)->m_nextSameTick);
    *ppc = m_nextSameTick;
    m_nextSameTick = edges;
}

////////////////////////////////////////////////////////////////////////
//
// update()
//
////////////////////////////////////////////////////////////////////////
void ClockDomain::evalTrigger (S_Trigger *trigger)
{
    if (trigger->fast)
    {
        if (*trigger->value)
        {
            if (trigger->delay)
                *m_syncTriggers[(m_syncIndex + trigger->delay) & m_syncMask].push(trigger) = *trigger->value;
            else
                TRIGGER_ACTIVATE_TARGET(trigger->target, trigger->value);
        }
    }
    else
    {
        bool zero = true;
        for (unsigned i = 0 ; i < trigger->size ; i++)
        {
            if (trigger->value[i])
            {
                zero = false;
                break;
            }
        }

        if (zero == (bool) trigger->activeLow)
        {
            if (trigger->delay)
                memcpy(m_syncTriggers[(m_syncIndex + trigger->delay) & m_syncMask].push(trigger), trigger->value, trigger->size);
            else
                TRIGGER_ACTIVATE_TARGET(trigger->target, trigger->value);

            if (trigger->latch && !trigger->active)
            {
                trigger->active = 1;
                m_stickyTriggers.insert(trigger);
            }
        }
        else if (trigger->latch && trigger->active)
        {
            trigger->active = 0;
            m_stickyTriggers.erase(trigger);
        }
    }
}

void ClockDomain::update ()
{
    if (!(m_numEdges & 1))
        return;

    int i;

    // First fire all of the events scheduled for this clock cycle
    while (m_events.size() && (m_events.begin()->first == m_numTicks))
    {
        EventMap::iterator it = m_events.begin();
        IEvent *event = it->second;
        event->fireEvent();
        delete event;
        m_events.erase(it);
    }

    // Now do all the combinational updates
    byte *curr = m_updates;
    byte *end = m_updates + m_updateSize;
    std::set<S_Trigger *, descore::allow_ptr<S_Trigger *> >::const_iterator itSticky;

    while (curr < end)
    {
        t_currentUpdate = (const S_Update *) curr;
        Component *component = t_currentUpdate->component;
        if (component)
        {
            Sim::stats.numUpdatesProcessed++;
            if (component->isActive())
            {
                Sim::stats.numActiveUpdates++;
                (component->*(t_currentUpdate->fn))();
            }
            else
            {
                itSticky = m_stickyTriggers.lower_bound((S_Trigger *) curr);
                curr += sizeof(S_Update) + t_currentUpdate->numTriggers * sizeof(S_Trigger);
                for ( ; *itSticky < (S_Trigger *) curr ; itSticky++)
                    evalTrigger(*itSticky);
                continue;
            }
        }

        // Evaluate the triggers
        curr += sizeof(S_Update);
        for (i = 0 ; i < t_currentUpdate->numTriggers ; i++, curr += sizeof(S_Trigger))
            evalTrigger((S_Trigger *) curr);
    }
    t_currentUpdate = NULL;
}

END_NAMESPACE_CASCADE
