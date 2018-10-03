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
// SimGlobals.cpp
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
#include "Update.hpp"
#include <descore/Statistics.hpp>
#include <descore/crc.hpp>
#include <descore/Wildcard.hpp>
#include "Clock.hpp"
#include "Verilog.hpp"
#include "Waves.hpp"

using namespace Cascade;

////////////////////////////////////////////////////////////////////////
//
// Static variables
//
////////////////////////////////////////////////////////////////////////
Sim::SimState Sim::state = SimNone;
uint64 Sim::simTime = 0;
uint32 Sim::simTicks = 0;
bool Sim::tracing = true;
uint64 Sim::nextCheckpoint = (uint64) 0x7fffffffffffffffLL;
uint32 Sim::s_checksum = 0xffffffff;
CascadeStats Sim::stats;

Component *Sim::topLevelComponents = NULL;
bool Sim::isVerilogSimulation = false;
bool Sim::verilogCallbackPump = false;

BEGIN_NAMESPACE_CASCADE

DeclareParameterGroup(params);

END_NAMESPACE_CASCADE

////////////////////////////////////////////////////////////////////////////////
//
// Prototypes
//
////////////////////////////////////////////////////////////////////////////////

static void initializeTracing ();

////////////////////////////////////////////////////////////////////////
//
// Statistics
//
////////////////////////////////////////////////////////////////////////
CascadeStats::CascadeStats ()
{
    reset();
}

void CascadeStats::reset ()
{
    memset(this, 0, sizeof(*this));
}

static double timer_precision;

#define DUMP_STAT(stat)     log("    %-21s %d\n", #stat, stat)
#define DUMP_STAT64(stat)   log("    %-21s %" PRId64 "\n", #stat, stat)
#define DUMP_TIMESTAT(stat) log("    %-21s %.3lf\n", #stat, (double) stat / timer_precision)

void CascadeStats::Dump ()
{
    timer_precision = 1000000.0;

    numPortWrapperBytes = numPorts * sizeof(PortWrapper);
    numUpdateWrapperBytes = numUpdates * sizeof(UpdateWrapper);

    log("\n=== Cascade Statistics ===\n\n");
    log("Initialization Statistics:\n");
    DUMP_STAT(numComponents);
    DUMP_STAT(numPorts);
    DUMP_STAT(numClockDomains);
    DUMP_STAT(numUpdates);
    DUMP_STAT(numFifos);
    DUMP_STAT(numTriggers);
    DUMP_STAT64(numPortWrapperBytes);
    DUMP_STAT64(numUpdateWrapperBytes);
    DUMP_STAT64(numTemporaryBytes);
    log("Memory Statistics:\n");
    DUMP_STAT64(numConstantBytes);
    DUMP_STAT64(numPortBytes);
    DUMP_STAT64(numFifoBytes);
    DUMP_STAT64(numUpdateBytes);
    DUMP_STAT64(numRegisterBytes);
    DUMP_STAT64(numFakeRegisterBytes);
    log("Activation Statistics:\n");
    DUMP_STAT64(numActiveUpdates);
    DUMP_STAT64(numUpdatesProcessed);
#ifdef ENABLE_ACTIVATION_STATS
    DUMP_STAT64(numActivations);
    DUMP_STAT64(numDeactivations);
#endif
    log("Performance Statistics:\n");
    DUMP_TIMESTAT(preTickTime);
    DUMP_TIMESTAT(tickTime);
    DUMP_TIMESTAT(postTickTime);
    DUMP_TIMESTAT(updateTime);
}

////////////////////////////////////////////////////////////////////////
//
// getAssertionContext()
//
// Global assertion context for cascade simulations
//
////////////////////////////////////////////////////////////////////////
static string getCascadeAssertionContext ()
{
    if (t_currentUpdate)
        return str("during evaluation of %s", *getUpdateName(t_currentUpdate));
    return "Top level";
}

/////////////////////////////////////////////////////////////////
//
// cleanup()
//
/////////////////////////////////////////////////////////////////
void Sim::cleanup ()
{
    if (topLevelComponents)
    {
        logerr("Memory leak detected: %s was never deallocated\n", *topLevelComponents->getName());
        topLevelComponents = NULL;
        cleanupInternal();
    }
}

////////////////////////////////////////////////////////////////////////
//
// cleanupInternal()
//
// Called from component destructor to clean things up when the last
// component is destroyed.
//
////////////////////////////////////////////////////////////////////////
void Sim::cleanupInternal ()
{
    CascadeValidate(state != SimNone, "Sim::cleanup() called but state is already SimNone");
    CascadeValidate(!topLevelComponents, "Sim::cleanup() called but there are still components");

    // Reset static variables
    simTime = 0;
    simTicks = 0;
    s_checksum = 0xffffffff;
    descore::resetWarningCount();

    // Clean up port state
    PortWrapper::cleanup();

    // Clean up the component tree
    topLevelComponents = NULL;

    // Clean up the clock domains
    ClockDomain::cleanupClockDomains();

    // Clean up the global clocks
    Clock::cleanup();

    // Clean up the update hierarchy (will destroy wrappers if init() has not been called)
    UpdateFunctions::cleanup();

    // If a fatal error occurs during construction then leave the state as SimFatalError;
    // it will be set to SimNone when the last construction frame is deleted.
    if (!Hierarchy::currFrame || descore::g_error)
        Sim::state = SimNone;

    // Reset the constants
    Constant::cleanup();

    // Delete wrapper storage (in case Sim::init() was never called)
    Wrapper::freeBlocks();

    // Cleanup up waves state
    Waves::cleanup();

    if (params.Verbose)
        stats.Dump();
    stats.reset();
}

////////////////////////////////////////////////////////////////////////
//
// init()
//
// This should be called ONCE after all components have been created
// and before the simulation begins.  It resolves all node pointers
// and initializes all components.
//
////////////////////////////////////////////////////////////////////////
void Sim::init ()
{
    // Add parameter-specified traces and signal dumping
    descore::setTraces(**params.Traces);
    setDumps(**params.DumpSignals);

    unsigned start = (unsigned) time(NULL);
    assert_always(state != SimInitialized, "Simulation has already been initialized");
    state = SimInitializing;

    // Set the assertion context
    descore::setGlobalAssertionContext(&getCascadeAssertionContext);

    // Initialize tracing
    initializeTracing();

    // Initialize constants
    Constant::initConstants();

    // Resolve update wrapper clock domains
    UpdateFunctions::resolveClockDomains();

    // Initialize waves (before resolveNetlists() so that all ports still have wrappers)
    Waves::initialize();

    // Resolve netlists
    PortWrapper::resolveNetlists();

    // Sort update functions
    UpdateFunctions::sort();

    // Initialize components (before ClockDomain::initialize() because initComponent() 
    // might create the default clock domain).
    doComponents(initComponent, true);

    // Initialize ports and create the update array
    ClockDomain::initialize();

    // Resolve combinational port connections
    PortWrapper::finalizeConnectedPorts();

    // Resolve wave signal value pointers and add signals to clock domains
    Waves::resolveSignals();

    // Make sure everything got cleaned up
    CascadeValidate(!Hierarchy::currFrame && !Hierarchy::currComponent,
        "Construction frames did not get cleaned up properly\n"
        "Note: On some systems this can be caused by dynamically allocating a\n"
        "component within a function call that runs the simulation, e.g.\n\n"
        "    runSimulation(new MainComponent);\n\n"
        "To fix this problem, do this instead:\n\n"
        "    MainComponent *c = new MainComponent;\n"
        "    runSimulation(c);");
    Wrapper::freeBlocks();

    state = SimInitialized;

    logInfo("Resetting simulation...\n");

    // Initialize CModules
#ifdef _VERILOG
    VerilogModule::initModules();
#endif
    reset();

    // Possibly restore from checkpoint
    if (*params.RestoreFromCheckpoint != "")
    {
        logInfo("Restoring simulation from %s...\n", params.RestoreFromCheckpoint->c_str());
        SimArchive::loadSimulation(params.RestoreFromCheckpoint->c_str());

        // Set the global tracing flag using the restored time
        tracing = (simTime >= 1000 * params.TraceStartTime && simTime <= 1000 * params.TraceStopTime);

        // Possibly validate the checkpoint
        if (*params.ValidateCheckpoint != "")
        {
            Archive ar(*params.ValidateCheckpoint, Archive::Validate);
            SimArchive::archiveSimulation(ar);
        }
    }
    logInfo("Simulation initialized (%d seconds).\n", int(time(NULL) - start));

    // Set next checkpoint time
    if (params.CheckpointInterval)
        nextCheckpoint = simTime + params.CheckpointInterval * 1000;
    else
        nextCheckpoint = (uint64) 0x7fffffffffffffffLL;
}

/////////////////////////////////////////////////////////////////
//
// reset()
//
/////////////////////////////////////////////////////////////////

static bool g_iterateReset;
static std::vector<byte> g_portSnapshot;

void Sim::reset (int level)
{
    resetInternal(topLevelComponents, level, true);
}

void Sim::reset (Component *component, int level)
{
    resetInternal(component, level, false);
}

void Sim::resetInternal (Component *component, int level, bool resetSiblings)
{
    // Make sure simulation is initialized
    if (Sim::state != Sim::SimInitialized)
        Sim::init();
    Sim::state = Sim::SimResetting;

    // First reset the ports.  Do this before resetting the components to avoid
    // destroying port initialization within component reset() functions.
    ClockDomain::resetPorts();

    // Next reset the events.  Do this before resetting the components so that
    // components can schedule events from their reset() functions.
    ClockDomain::resetEvents();

    logInfo("Resetting components...\n");

    // Initial component reset
    for (Component *list = component ; list ; list = resetSiblings ? list->nextComponent : NULL)
        resetComponent(list, level);

    // Reset the components.  Do this before resetting the clock domains, because
    // this will reset all of the component FIFOs, and the clock domains check to see
    // which fifos have been reset in order to determine which pending fifo events
    // should be dropped.
    int numResets = 0;
    do 
    {
        if (++numResets > params.MaxResetIterations)
            die("reset() failed to converge (try increasing MaxResetIterations)");

        logInfo("    Reset iteration %d\n", numResets);
        ClockDomain::propagateReset();
        for (Component *list = component ; list ; list = resetSiblings ? list->nextComponent : NULL)
            resetComponent(list, level);
        g_iterateReset = false;
        for (Component *list = component ; list ; list = resetSiblings ? list->nextComponent : NULL)
            resetComponent(list, level);
    } 
    while (g_iterateReset);

    // Now reset clock domain state
    ClockDomain::resetDomains();

    // Do some final trigger consistency checks following the reset
    ClockDomain::resetTriggers();

    Sim::state = Sim::SimInitialized;
}

static void checkOutputs (Component *component, bool snapshot)
{
    if ((params.MaxResetIterations <= 1) || g_iterateReset)
        return;

    int numBytes = 0;
    PortIterator it(PortSet::Outputs, component);
    for ( ; it ; it++)
    {
        int size = it.entry()->portInfo->sizeInBytes;
        if (size + numBytes > (int) g_portSnapshot.size())
            g_portSnapshot.resize(size + numBytes);
        byte *value = *(byte **) it.address();
        if (snapshot)
            memcpy(&g_portSnapshot[numBytes], value, size);
        else if (memcmp(&g_portSnapshot[numBytes], value, size))
            g_iterateReset = true;
        numBytes += size;
    }
}

void Sim::resetComponent (Component *component, int level)
{
    component->m_componentActive = 1;
    checkOutputs(component, true);
    component->getInterfaceDescriptor()->reset(component, level);
    checkOutputs(component, false);
    for (Component *list = component->childComponent ; list ; list = list->nextComponent)
        resetComponent(list, level);
}

////////////////////////////////////////////////////////////////////////
//
// doComponents()
//
////////////////////////////////////////////////////////////////////////
static inline void eval (void (*f) (Component *), Component *c, const char * = NULL)
{
    f(c);
}
static inline void eval (void (Component::*f) (), Component *c, const char * = NULL)
{
    (c->*f)();
}
static inline void eval (void (*f) (Component *, const char *), Component *c, const char *name)
{
    f(c, name);
}

template <typename Tf>
static void doComponentsInternal (Tf f, bool childrenFirst, Component *list)
{
    for ( ; list ; list = list->nextComponent)
    {
        if (!childrenFirst)
            eval(f, list);
        doComponentsInternal(f, childrenFirst, list->childComponent);
        if (childrenFirst)
            eval(f, list);
    }
}

template <typename Tf>
static void doComponentsInternal (Tf f, const char *wildcardName, Component *list, strbuff &name)
{
    if (!list)
        return;
    int len0 = name.len();
    Component *parent = list->parentComponent;
    if (parent && parent->getComponentName() && !parent->suppressDot())
        name.putch('.');
    int len1 = name.len();
    for ( ; list ; list = list->nextComponent)
    {
        list->formatLocalName(name);
        if (wildcardMatch(wildcardName, name))
            eval(f, list, name);

        if (list->childComponent)
        {
            int len2 = name.len();
            name.putch('*');
            bool match = wildcardMatch(wildcardName, name);
            name.truncate(len2);
            if (match)
                doComponentsInternal(f, wildcardName, list->childComponent, name);
        }
        name.truncate(len1);
    }
    name.truncate(len0);
}

void Sim::doComponents (void (*f) (Component *), bool childrenFirst)
{
    doComponentsInternal(f, childrenFirst, topLevelComponents);
}
void Sim::doComponents (void (Component::*f) (), bool childrenFirst)
{
    doComponentsInternal(f, childrenFirst, topLevelComponents);
}
void Sim::doComponents (void (*f) (Component *), const char *wildcardName)
{
    strbuff name;
    doComponentsInternal(f, wildcardName, topLevelComponents, name);
}
void Sim::doComponents (void (Component::*f) (), const char *wildcardName)
{
    strbuff name;
    doComponentsInternal(f, wildcardName, topLevelComponents, name);
}
void Sim::doComponents (void (*f) (Component *, const char *), const char *wildcardName)
{
    strbuff name;
    doComponentsInternal(f, wildcardName, topLevelComponents, name);
}

////////////////////////////////////////////////////////////////////////
//
// initComponent()
//
////////////////////////////////////////////////////////////////////////
void Sim::initComponent (Component *pc)
{
    // Components should be active at this point
    assert_always(pc->isActive(), "Error in %s:\n"
        "    deactivate() should not be called from the constructor.\n"
        "    Call it from reset() instead.", *pc->getName());

    // If this is a tickable component then register it with the clock domain
    if (pc->hasTick())
        pc->getClockDomain()->registerTickableComponent(pc);
}

////////////////////////////////////////////////////////////////////////
//
// run()
//
////////////////////////////////////////////////////////////////////////
void Sim::run (uint64 runTime)
{
    // Translate delta time into absolute time
    if (runTime)
        runTime += simTime;

    // Run the simulation
    ClockDomain::runSimulation(runTime);
}

////////////////////////////////////////////////////////////////////////
//
// runUntil()
//
////////////////////////////////////////////////////////////////////////
void Sim::runUntil (uint64 endTime)
{
    ClockDomain::runSimulation(endTime);
}

////////////////////////////////////////////////////////////////////////
//
// dumpComponentName()
//
// Log the full name of a component.
//
////////////////////////////////////////////////////////////////////////
void Sim::dumpComponentName (Component *pc)
{
    if (pc->getComponentName())
        log("%s\n", *pc->getName());
}

////////////////////////////////////////////////////////////////////////
//
// dumpComponentNames()
//
// Log the full names of all components in the system.
//
////////////////////////////////////////////////////////////////////////
void Sim::dumpComponentNames ()
{
    doComponents(dumpComponentName);
}

/////////////////////////////////////////////////////////////////
//
// Dumping
//
/////////////////////////////////////////////////////////////////

void Sim::parseDumps (int &csz, char *rgsz[])
{
    int numArgs = csz;
    char **srcArgs = rgsz;

    csz = 1;
    for (int i = 1 ; i < numArgs ; i++)
    {
        if (!strcmp(rgsz[i], "-dump"))
        {
            assert_always(i < numArgs - 1, "-dump requires an argument (-dump <specifiers>)");
            setDumps(rgsz[++i]);
        }
        else
            rgsz[csz++] = srcArgs[i];
    }
}

void Sim::setDumps (const char *dumps)
{
    std::vector<string> vecDumps = descore::expandSpecifierString(dumps);
    for (unsigned i = 0 ; i < vecDumps.size() ; i++)
    {
        string name = vecDumps[i];

        // Check for a signal specification
        string signal = "*";
        int idx = (int) name.find('/');
        if (idx != (int) string::npos)
        {
            signal = name.substr(idx + 1);
            name = name.substr(0, idx);
        }

        // Check for a level specification
        int level = 0;
        idx = (int) name.find(':');
        if (idx != (int) string::npos)
        {
            level = atoi(*name.substr(idx + 1));
            name = name.substr(0, idx);
        }

        Sim::dumpSignals(*name, *signal, level);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Tracing
//
////////////////////////////////////////////////////////////////////////////////

struct Tracer : public descore::Tracer
{
    virtual void traceHeader (const string &context, const string &keyname)
    {
        appendTrace("[%d.%03d] ", (int) (Sim::simTime / 1000), (int) (Sim::simTime % 1000));
        descore::Tracer::traceHeader(context, keyname);
    }
    virtual bool traceEnabled () const
    {
        return Sim::tracing;
    }
} g_cascadeTracer;

static void notifyTrace (Component *c, const char *name)
{
    c->setTraceKeys(descore::computeTraceKeys(name));
}

struct CascadeTraceCallback : public descore::ITraceCallback
{
    void notifyTrace (const char *context)
    {
        Sim::doComponents(::notifyTrace, context);
    }
} g_cascadeTraceCallback;

void initializeTracing ()
{
    descore::setTracer(&g_cascadeTracer);
    Sim::doComponents(notifyTrace, "*");
}

////////////////////////////////////////////////////////////////////////////////
//
// Waves
//
////////////////////////////////////////////////////////////////////////////////
void Sim::dumpSignals (const char *wildcardName, int level /* = 0 */)
{
    Cascade::Waves::dumpSignals(wildcardName, "*", level);
}
void Sim::dumpSignals (const char *wcComponent, const char *wcSignals, int level /* = 0 */)
{
    Cascade::Waves::dumpSignals(wcComponent, wcSignals, level);
}
void Sim::dumpSignals (const Component *c, int level /* = 0 */)
{
    Cascade::Waves::dumpSignals(c, "*", level);
}
void Sim::dumpSignals (const Component *c, const char *wcSignals, int level /* = 0 */)
{
    Cascade::Waves::dumpSignals(c, wcSignals, level);
}

/////////////////////////////////////////////////////////////////
//
// getComponent()
//
/////////////////////////////////////////////////////////////////
Component *Sim::getComponent (const char *wildardName, Component *list /* = topLevelComponents */)
{
    for ( ; list ; list = list->nextComponent)
    {
        if (wildcardMatch(wildardName, list->getName()))
            return list;
        Component *c = getComponent(wildardName, list->childComponent);
        if (c)
            return c;
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////
//
// updateChecksum()
//
// Used during construction and initialization to compute a hardware checksum
//
////////////////////////////////////////////////////////////////////////
void Sim::updateChecksum (const char *sz, int id)
{
    if (sz)
        s_checksum = crc32(s_checksum, (const byte *)sz, (int) strlen(sz));
    s_checksum = crc32(s_checksum, (const byte *)&id, 4);
}

////////////////////////////////////////////////////////////////////////
//
// Error hooks
//
////////////////////////////////////////////////////////////////////////

#ifdef _VERILOG
#include <veriuser.h>
#else
#define tf_dofinish() (void) 0
#endif

static descore::ErrorHook s_errorHook;
static descore::FatalHook s_fatalHook;

void Sim::errorHook (descore::runtime_error &error)
{
    // See if there's any useful error state to output
    if (state == SimConstruct)
        Hierarchy::dumpConstructionStack(error);
    else if (state == SimArchiving)
    {
        if (SimArchive::s_component)
            error.append("    while archiving %s\n", *SimArchive::s_component->getName());
    }
    else if (t_currentUpdate)
        error.append("    during evaluation of %s\n", *getUpdateName(t_currentUpdate));

    if (state == SimInitialized || Sim::simTime)
        error.append("    at simulation time = %.3lf\n", Sim::simTime / 1000.0);

    s_errorHook(error);
}

static void simFatalHook (const descore::runtime_error &error)
{
    if (Sim::isVerilogSimulation)
        tf_dofinish();

    s_fatalHook(error);
}

/////////////////////////////////////////////////////////////////
//
// logHeader()
//
/////////////////////////////////////////////////////////////////
void Sim::logHeader ()
{
    static const char *logHeader = 
        "#         __________________________________________\n"
        "#  C     /                                          \n"
        "#   A   /   Version %s - %s \n"
        "#    S /                                            \n"
        "#     C     Copyright (c) 2011 D. E. Shaw Research  \n"
        "#    / A                                            \n"
        "#   /   D   All Rights Reserved                     \n"
        "#  /     E__________________________________________\n";

    log(logHeader, CASCADE_VERSION, CASCADE_DATE);
}

/////////////////////////////////////////////////////////////////
//
// Static initialization
//
/////////////////////////////////////////////////////////////////
struct CascadeStaticInitialization
{
    CascadeStaticInitialization ()
    {
        descore::setLogHeader(&Sim::logHeader);
        s_errorHook = descore::setErrorHook(&Sim::errorHook);
        s_fatalHook = descore::setFatalHook(&simFatalHook);
    }
} cascadeStaticInitialization;

