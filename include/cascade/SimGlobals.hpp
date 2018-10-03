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
// SimGlobals.hpp
//
// Copyright (C) 2007 D. E. Shaw Research
//
// Author:  J.P. Grossman (jp.grossman@deshaw.com)
// Created: 02/21/2007
//
//////////////////////////////////////////////////////////////////////

#ifndef Cascade_SimGlobals_hpp_
#define Cascade_SimGlobals_hpp_

class Component;
class Archive;

////////////////////////////////////////////////////////////////////////
//
// CascadeStats
//
////////////////////////////////////////////////////////////////////////
struct CascadeStats
{
    CascadeStats ();
    void reset ();
    void Dump ();

    // Initialization stats
    int   numPorts;
    int   numFifos;
    int64 numPortWrapperBytes;
    int   numUpdates;
    int64 numUpdateWrapperBytes;
    int   numTriggers;
    int   numComponents;
    int64 numTemporaryBytes;
    int   numClockDomains;

    // Run-time stats
    int64 numConstantBytes;
    int64 numPortBytes;
    int64 numFifoBytes;
    int64 numUpdateBytes;
    int64 numRegisterBytes;
    int64 numFakeRegisterBytes;

    // Activation stats
    int64 numActiveUpdates;
    int64 numUpdatesProcessed;
    int64 numActivations;
    int64 numDeactivations;

    // Performance stats
    uint64 preTickTime;
    uint64 tickTime;
    uint64 postTickTime;
    uint64 updateTime;
};

////////////////////////////////////////////////////////////////////////
//
// Sim
//
// Class used for convenience as a namespace.  All members are static.
//
////////////////////////////////////////////////////////////////////////
class Sim
{
    friend class Component;
    friend class SimArchive;

public: 
    //---------------------------------------------------------------
    // External functions
    //---------------------------------------------------------------

    // This should be called ONCE after all components have been created
    // and before the simulation begins.  It resolves all port pointers
    // and initializes all components.
    static void init ();

    // Reset the entire simulation (calls reset() for all components and interfaces)
    static void reset (int level = RESET_COLD);

    // Reset a portion of the simulation hierarchy (calls reset() for all components
    // and interfaces contained within the specified component)
    static void reset (Component *component, int level = RESET_COLD);

    // Main entry point to run a simulation.  Runs for the specified number of
    // picoseconds or for a single tick() event if runTime = 0.
    static void run (uint64 runTime = 0);

    // Variation - run to an absolute time rather than for a delta
    static void runUntil (uint64 endTime);

    // Cascade log header
    static void logHeader ();

    // Helper function - call the component function or member function for each component in the list
    static void doComponents (void (*f) (Component *), bool childrenFirst = false);
    static void doComponents (void (Component::*f) (), bool childrenFirst = false);

    // Same as above, but always do parents first and do a wildcard match by name
    static void doComponents (void (*f) (Component *), const char *wildcardName);
    static void doComponents (void (Component::*f) (), const char *wildcardName);

    // Additional variation provided as an optimization to avoid regenerating component names
    static void doComponents (void (*f) (Component *, const char *), const char *wildcardName);

    // Try as best as possible to tidy up after an error in preparation for the next test
    static void cleanup ();

public:
    //----------------------------------------------------------------------
    // Debugging functions
    //----------------------------------------------------------------------

    // Dump a list of all components to the log
    static void dumpComponentNames (); 

    // Dump the full component name
    static void dumpComponentName (Component *c);

    // Parse dump directives from the command line and remove them from the 
    // command-line arguments.  Each dump directive is of the form 
    // '-dump <signals>', where <signals> is a string specifying a set of 
    // signals to dump to the waves file.  First, <signals> is expanded into a 
    // set of individual dump strings using the the recursive expansion rules
    // described in descore::expandSpecifierString().  Each individual dump
    // string should then be of the form '<component>[:<level>][/<signal>]'
    // where <component> is a wildcard strings specifying which components to
    // dump, <signal> is a wildcard string specifying which signals to dump
    // (if it is omitted then it defaults to "*"), and <level> specifies how
    // many levels of the hierarchy to dump starting with each matching
    // component, where 0 (default) causes the entire hierarchy below matching
    // components to be dumped.
    //
    static void parseDumps (int &csz, char *rgsz[]);

    // Parse a dump directive (as defined in the above comment) and dump
    // the appropriate signals.
    static void setDumps (const char *dumps);

    // Retrieve a component by name (returns the first match)
    static Component *getComponent (const char *wildardName, Component *list = topLevelComponents);

    // Waves
    static void dumpSignals (const char *wcComponent, int level = 0);
    static void dumpSignals (const char *wcComponent, const char *wcSignals, int level = 0);
    static void dumpSignals (const Component *c, int level = 0);
    static void dumpSignals (const Component *c, const char *wcSignals, int level = 0);

private:
    // Called from component destructors to clean things up.
    static void cleanupInternal ();

    // Internal reset functions
    static void resetInternal (Component *component, int level, bool resetSiblings);
    static void resetComponent (Component *component, int level);

public:
    //----------------------------------------------------------------------
    // Internal functions (infrastructure use)
    //----------------------------------------------------------------------

    // Used during construction to compute a hardware checksum
    static void updateChecksum (const char *sz, int id = 0);

private:

    // Helper functions used by doComponents
    static void initComponent (Component *c);
    static void checkComponentDeadlock (Component *c);

public:
    //----------------------------------------------------------------------
    // Global simulation time
    //----------------------------------------------------------------------

    static uint64 simTime;        // time of current rising clock edge in picoseconds
    static uint32 simTicks;       // total number of rising clock edges
    static bool   tracing;        // tracing is enabled/disabled based on simulation time (see CascadeParams)
    static uint64 nextCheckpoint; // simTime at which next checkpoint should be written

public:
    //----------------------------------------------------------------------
    // Internal globals
    //----------------------------------------------------------------------

    static enum SimState 
    { 
        SimNone, 
        SimConstruct, 
        SimInitializing, 
        SimInitialized,
        SimResetting,
        SimArchiving
    } state;
    static Component *topLevelComponents;   // Linked list of top-level components
    static bool verbose;                    // Display Infos

    //----------------------------------------------------------------------
    // Statistics
    //----------------------------------------------------------------------
    static CascadeStats stats;

private:
    static uint32 s_checksum;               // hardware checksum (used to validate archives)

public:
    static bool isVerilogSimulation; // Verilog is the master, Sim is the slave
    static bool verilogCallbackPump; // True if the simulation is being driven by callbacks from Verilog

    // Called when an error occurs to append context information
    static void errorHook (descore::runtime_error &error);
};

#endif

